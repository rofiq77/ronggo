/*
 * ESP32 Super Mini + PCM5102 DAC + SD Card WAV Player
 * Kompatibel dengan Audio.h library
 * Scan otomatis file WAV, 2 tombol input, 2 LED indicator
 */

#include "Arduino.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "Audio.h"

// ==================== PIN CONFIGURATION ====================
// SD Card SPI
#define SD_CS         7     // Chip Select SD Card

// I2S PCM5102 DAC
#define I2S_BCLK      20    // Bit Clock (BCLK)
#define I2S_LRC       21    // L/R Clock / Word Select (LRCK)
#define I2S_DOUT      10    // Data Out (DIN ke PCM5102)

// Sensor Piezo (Analog Input)
#define PIEZO_PIN     1     // A1 - Sensor Piezo

// Tombol Input
#define SW1_PIN       2     // A2 - Tombol Suara 1
#define SW2_PIN       3     // A3 - Tombol Suara 2

// LED Indicator
#define LED1_PIN      8     // RX - LED Suara 1
#define LED2_PIN      9     // TX - LED Suara 2

// ============================================================

// Audio object
Audio audio;

// File storage
String fileSuara1 = "";
String fileSuara2 = "";
int suaraTerpilih = 1;  // 1 = Suara 1, 2 = Suara 2

// Debounce variables
unsigned long lastButtonTime = 0;
unsigned long lastPiezoTime = 0;
const unsigned long DEBOUNCE_DELAY = 300;  // 300ms debounce

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n╔════════════════════════════════════════╗");
  Serial.println("║  ESP32 Drum Kendang + PCM5102 + SD     ║");
  Serial.println("║     WAV File Auto-Scanner v1.0         ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  // Setup GPIO
  Serial.println("Step 1: Initializing GPIO pins...");
  initializeGPIO();
  
  // Setup SD Card
  Serial.println("Step 2: Initializing SD Card...");
  if (!initializeSDCard()) {
    Serial.println("❌ FATAL: SD Card failed!");
    while(1); // Halt
  }
  
  // Scan WAV files
  Serial.println("Step 3: Scanning WAV files...");
  scanSDFiles();
  
  // Setup DAC
  Serial.println("Step 4: Initializing DAC PCM5102...");
  initializeDAC();
  
  Serial.println("\n✅ SYSTEM READY!\n");
  printStatus();
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  // Keep audio running
  audio.loop();
  
  // Read buttons with debounce
  checkButtons();
  
  // Read piezo sensor
  checkPiezo();
  
  // Print status periodically
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 10000) {
    printStatus();
    lastStatus = millis();
  }
}

// ============================================================
// INITIALIZE GPIO
// ============================================================
void initializeGPIO() {
  // Piezo input
  pinMode(PIEZO_PIN, INPUT);
  
  // Button inputs with pull-up
  pinMode(SW1_PIN, INPUT_PULLUP);
  pinMode(SW2_PIN, INPUT_PULLUP);
  
  // LED outputs
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  
  // Initial LED state
  digitalWrite(LED1_PIN, HIGH);   // LED 1 ON
  digitalWrite(LED2_PIN, LOW);    // LED 2 OFF
  
  Serial.println("  ✅ GPIO initialized\n");
}

// ============================================================
// INITIALIZE SD CARD
// ============================================================
bool initializeSDCard() {
  // Setup SPI with custom pins
  SPI.begin();
  
  // Initialize SD
  if (!SD.begin(SD_CS)) {
    Serial.println("  ❌ SD Card failed!");
    Serial.println("  Check: CS pin, VCC, GND, MOSI, MISO, SCK\n");
    return false;
  }
  
  Serial.println("  ✅ SD Card OK\n");
  return true;
}

// ============================================================
// SCAN SD FILES OTOMATIS
// ============================================================
void scanSDFiles() {
  Serial.println("  Scanning for .WAV files...\n");
  
  File root = SD.open("/");
  if (!root) {
    Serial.println("  ❌ Cannot open SD root!\n");
    return;
  }
  
  if (!root.isDirectory()) {
    Serial.println("  ❌ Root is not a directory!\n");
    return;
  }
  
  File file = root.openNextFile();
  int wavCount = 0;
  
  while (file && wavCount < 2) {
    String name = String(file.name());
    
    // Check if file is WAV
    if (!file.isDirectory()) {
      if (name.endsWith(".wav") || name.endsWith(".WAV")) {
        wavCount++;
        String fullPath = "/" + name;
        
        if (wavCount == 1) {
          fileSuara1 = fullPath;
          Serial.printf("  [1] Found: %s\n", name.c_str());
        } else if (wavCount == 2) {
          fileSuara2 = fullPath;
          Serial.printf("  [2] Found: %s\n", name.c_str());
        }
      }
    }
    
    file = root.openNextFile();
  }
  
  root.close();
  
  if (wavCount == 0) {
    Serial.println("  ⚠️  No WAV files found on SD Card!\n");
  } else {
    Serial.println();
  }
}

// ============================================================
// INITIALIZE DAC PCM5102
// ============================================================
void initializeDAC() {
  // Set I2S pins
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  
  // Set volume (0-21)
  audio.setVolume(18);  // Medium volume
  
  Serial.println("  ✅ DAC PCM5102 initialized\n");
}

// ============================================================
// CHECK BUTTONS
// ============================================================
void checkButtons() {
  unsigned long now = millis();
  
  // Button 1
  if (digitalRead(SW1_PIN) == LOW) {
    if (now - lastButtonTime > DEBOUNCE_DELAY) {
      selectSound1();
      lastButtonTime = now;
    }
  }
  
  // Button 2
  if (digitalRead(SW2_PIN) == LOW) {
    if (now - lastButtonTime > DEBOUNCE_DELAY) {
      selectSound2();
      lastButtonTime = now;
    }
  }
}

// ============================================================
// SELECT SOUND 1
// ============================================================
void selectSound1() {
  suaraTerpilih = 1;
  digitalWrite(LED1_PIN, HIGH);
  digitalWrite(LED2_PIN, LOW);
  
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.println("🎵 [SOUND 1 SELECTED]");
  Serial.printf("   File: %s\n", fileSuara1.c_str());
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.println();
}

// ============================================================
// SELECT SOUND 2
// ============================================================
void selectSound2() {
  suaraTerpilih = 2;
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, HIGH);
  
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.println("🎵 [SOUND 2 SELECTED]");
  Serial.printf("   File: %s\n", fileSuara2.c_str());
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.println();
}

// ============================================================
// CHECK PIEZO SENSOR
// ============================================================
void checkPiezo() {
  int piezoValue = analogRead(PIEZO_PIN);
  unsigned long now = millis();
  
  // Threshold: 800 (sesuaikan dengan sensor Anda)
  if (piezoValue > 800) {
    // Check debounce
    if (now - lastPiezoTime > DEBOUNCE_DELAY) {
      // Only play if not already playing
      if (!audio.isRunning()) {
        playSelectedSound();
      }
      lastPiezoTime = now;
    }
  }
}

// ============================================================
// PLAY SELECTED SOUND
// ============================================================
void playSelectedSound() {
  if (suaraTerpilih == 1 && fileSuara1 != "") {
    Serial.println("▶ Playing SOUND 1 (Piezo hit detected)");
    Serial.printf("  File: %s\n", fileSuara1.c_str());
    audio.connecttoFS(SD, fileSuara1.c_str());
    
  } else if (suaraTerpilih == 2 && fileSuara2 != "") {
    Serial.println("▶ Playing SOUND 2 (Piezo hit detected)");
    Serial.printf("  File: %s\n", fileSuara2.c_str());
    audio.connecttoFS(SD, fileSuara2.c_str());
    
  } else {
    Serial.println("❌ No sound file selected or available!");
  }
  
  Serial.println();
}

// ============================================================
// PRINT STATUS
// ============================================================
void printStatus() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║         SYSTEM STATUS                  ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  Serial.println("📂 SD Card Files:");
  Serial.printf("  Sound 1: %s %s\n", 
                fileSuara1 != "" ? "✅" : "❌", 
                fileSuara1.c_str());
  Serial.printf("  Sound 2: %s %s\n", 
                fileSuara2 != "" ? "✅" : "❌", 
                fileSuara2.c_str());
  
  Serial.println("\n🎵 Playback:");
  Serial.printf("  Selected: Sound %d\n", suaraTerpilih);
  Serial.printf("  Playing: %s\n", audio.isRunning() ? "YES" : "NO");
  
  Serial.println("\n📍 Pinout:");
  Serial.printf("  BCLK: GPIO%d\n", I2S_BCLK);
  Serial.printf("  LRCK: GPIO%d\n", I2S_LRC);
  Serial.printf("  DOUT: GPIO%d\n", I2S_DOUT);
  Serial.printf("  SD CS: GPIO%d\n", SD_CS);
  
  Serial.println("\n🔊 Audio:");
  Serial.printf("  Volume: 18/21\n");
  Serial.printf("  Format: WAV\n");
  
  Serial.println("\n🎮 Controls:");
  Serial.println("  SW1: Select Sound 1");
  Serial.println("  SW2: Select Sound 2");
  Serial.println("  Piezo: Play (tap sensor)\n");
  
  Serial.println("═══════════════════════════════════════════\n");
}

// ============================================================
// AUDIO EVENT CALLBACKS (required by Audio.h)
// ============================================================

void audio_info(const char *info) {
  Serial.print("audio_info: "); Serial.println(info);
}

void audio_id3data(const char *info) {
  Serial.print("audio_id3data: "); Serial.println(info);
}

void audio_eof_mp3(const char *info) {
  Serial.print("audio_eof_mp3: "); Serial.println(info);
}

void audio_showstation(const char *info) {
  Serial.print("audio_showstation: "); Serial.println(info);
}

void audio_showstreamtitle(const char *info) {
  Serial.print("audio_showstreamtitle: "); Serial.println(info);
}

void audio_bitrate(const char *info) {
  Serial.print("audio_bitrate: "); Serial.println(info);
}

void audio_commercial(const char *info) {
  Serial.print("audio_commercial: "); Serial.println(info);
}

void audio_icyurl(const char *info) {
  Serial.print("audio_icyurl: "); Serial.println(info);
}

void audio_icygenre(const char *info) {
  Serial.print("audio_icygenre: "); Serial.println(info);
}

void audio_icylogurl(const char *info) {
  Serial.print("audio_icylogurl: "); Serial.println(info);
}

void audio_icylanguage(const char *info) {
  Serial.print("audio_icylanguage: "); Serial.println(info);
}

void audio_icyicqquality(const char *info) {
  Serial.print("audio_icyicqquality: "); Serial.println(info);
}

void audio_metaconnectionhandle(uint16_t something) {
  Serial.printf("audio_metaconnectionhandle: %d\n", something);
}

void audio_icynotification(const char *info) {
  Serial.print("audio_icynotification: "); Serial.println(info);
}
