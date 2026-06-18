/*
 * ESP32 Drum Controller - LCD Display Module
 * Tampilan menu, pad mapping, EQ, Volume, dan Battery
 * Display: 16x2 atau 20x4 I2C LCD
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ============ LCD Configuration ============
// Address 0x27 untuk 16x2, bisa berbeda tergantung LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);  // (address, columns, rows)

// Uncomment untuk LCD 20x4
// LiquidCrystal_I2C lcd(0x27, 20, 4);

// ============ Pin Definitions ============
#define ENCODER_CLK 19
#define ENCODER_DT 18
#define BTN_SELECT 13
#define BTN_MODE 15

// ============ Custom LCD Characters ============
// Buat karakter custom untuk visualisasi
byte batteryFull[8] = {
  B01110,
  B10001,
  B10001,
  B10001,
  B10001,
  B10001,
  B10001,
  B11111
};

byte batteryEmpty[8] = {
  B01110,
  B10001,
  B10001,
  B10001,
  B10001,
  B10001,
  B10001,
  B11111
};

byte speaker[8] = {
  B00001,
  B00011,
  B01111,
  B01111,
  B01111,
  B00011,
  B00001,
  B00000
};

byte note[8] = {
  B00100,
  B00110,
  B00101,
  B00101,
  B00101,
  B01101,
  B11001,
  B01000
};

byte eq[8] = {
  B10001,
  B10101,
  B10101,
  B10101,
  B10101,
  B11111,
  B00000,
  B00000
};

// ============ Data Structure ============
struct PadSettings {
  String name;
  uint8_t volume;
  uint8_t bass;
  uint8_t mid;
  uint8_t treble;
};

struct DrumController {
  PadSettings pads[9];
  uint8_t masterVolume;
  uint8_t currentPad;
  uint8_t menuMode;  // 0: Home, 1: Pad Select, 2: EQ, 3: Volume, 4: Battery
  uint8_t encoderValue;
};

// ============ Global Variables ============
DrumController drum;
unsigned long lastDisplayUpdate = 0;
volatile int encoderPos = 0;
int lastEncoderPos = 0;

// ============ Setup ============
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========== LCD Display Module Init ==========");
  
  // Initialize I2C & LCD
  Wire.begin(21, 22);  // SDA=21, SCL=22
  lcd.init();
  lcd.backlight();
  
  // Load custom characters
  lcd.createChar(0, batteryFull);
  lcd.createChar(1, batteryEmpty);
  lcd.createChar(2, speaker);
  lcd.createChar(3, note);
  lcd.createChar(4, eq);
  
  // Initialize data
  initializeDrumData();
  
  // Initialize GPIO
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_MODE, INPUT_PULLUP);
  
  // Setup interrupts
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), encoderISR, CHANGE);
  
  Serial.println("✓ LCD System Ready!");
  
  // Display splash screen
  displaySplashScreen();
}

// ============ Initialize Drum Data ============
void initializeDrumData() {
  drum.masterVolume = 80;
  drum.currentPad = 0;
  drum.menuMode = 0;
  
  const char* padNames[9] = {
    "Kick", "Snare", "HiHat", 
    "Tom1", "Tom2", "Tom3", 
    "Clap", "Ride", "Crash"
  };
  
  for (int i = 0; i < 9; i++) {
    drum.pads[i].name = padNames[i];
    drum.pads[i].volume = 80;
    drum.pads[i].bass = 50;
    drum.pads[i].mid = 50;
    drum.pads[i].treble = 50;
  }
}

// ============ Encoder ISR ============
void encoderISR() {
  if (digitalRead(ENCODER_CLK) == digitalRead(ENCODER_DT)) {
    encoderPos++;
  } else {
    encoderPos--;
  }
}

// ============ Main Loop ============
void loop() {
  // Check buttons
  checkButtons();
  
  // Update display
  updateDisplay();
  
  delay(50);
}

// ============ Button Checking ============
void checkButtons() {
  static unsigned long lastButtonTime = 0;
  
  if (millis() - lastButtonTime < 200) return;
  
  // MODE Button - Switch menu
  if (digitalRead(BTN_MODE) == LOW) {
    drum.menuMode = (drum.menuMode + 1) % 5;
    lastButtonTime = millis();
    Serial.printf("Menu Mode: %d\n", drum.menuMode);
  }
  
  // SELECT Button
  if (digitalRead(BTN_SELECT) == LOW) {
    Serial.println("SELECT pressed");
    lastButtonTime = millis();
  }
}

// ============ Update Display ============
void updateDisplay() {
  static unsigned long lastUpdate = 0;
  
  if (millis() - lastUpdate < 200) return;
  
  // Handle encoder input
  if (encoderPos != lastEncoderPos) {
    handleEncoderInput(encoderPos - lastEncoderPos);
    lastEncoderPos = encoderPos;
  }
  
  // Display based on mode
  switch(drum.menuMode) {
    case 0:
      displayHomeScreen();
      break;
    case 1:
      displayPadSelector();
      break;
    case 2:
      displayEQScreen();
      break;
    case 3:
      displayVolumeScreen();
      break;
    case 4:
      displayBatteryScreen();
      break;
  }
  
  lastUpdate = millis();
}

// ============ Handle Encoder Input ============
void handleEncoderInput(int delta) {
  switch(drum.menuMode) {
    case 0:  // Home - kontrol master volume
      drum.masterVolume = constrain(drum.masterVolume + delta * 5, 0, 100);
      break;
      
    case 1:  // Pad Selector
      drum.currentPad = (drum.currentPad + delta + 9) % 9;
      break;
      
    case 2:  // EQ - cycle through bass/mid/treble
      // (akan ditangani di displayEQScreen)
      break;
      
    case 3:  // Volume - kontrol volume pad
      drum.pads[drum.currentPad].volume = 
        constrain(drum.pads[drum.currentPad].volume + delta * 5, 0, 100);
      break;
  }
}

// ============ SPLASH SCREEN ============
void displaySplashScreen() {
  lcd.clear();
  
  // Line 1
  lcd.setCursor(0, 0);
  lcd.print("  DRUM KIT v1.0");
  
  // Line 2
  lcd.setCursor(0, 1);
  lcd.print("   ESP32 Ready");
  
  delay(2000);
  lcd.clear();
}

// ============ HOME SCREEN ============
void displayHomeScreen() {
  lcd.clear();
  
  // Line 1: Master Volume dengan visualisasi
  lcd.setCursor(0, 0);
  lcd.write(2);  // Speaker icon
  lcd.print(" Master Vol: ");
  
  // Bar graph
  int barLength = 3;  // Max 3 characters untuk 16 column
  for (int i = 0; i < barLength; i++) {
    if (i < (drum.masterVolume / 33)) {
      lcd.write(255);  // Full block
    } else {
      lcd.write(254);  // Empty block
    }
  }
  
  // Line 2: Percentage dan Pads info
  lcd.setCursor(0, 1);
  lcd.print("Vol:");
  lcd.print(drum.masterVolume);
  lcd.print("% Pads:9");
}

// ============ PAD SELECTOR SCREEN ============
void displayPadSelector() {
  lcd.clear();
  
  // Show current pad
  lcd.setCursor(0, 0);
  lcd.write(3);  // Note icon
  lcd.print(" PAD SELECT");
  
  // Line 2: Show pads in group of 3
  lcd.setCursor(0, 1);
  
  int startPad = (drum.currentPad / 3) * 3;
  for (int i = 0; i < 3; i++) {
    int padNum = startPad + i;
    
    if (padNum == drum.currentPad) {
      lcd.print("[");
      lcd.print(drum.pads[padNum].name);
      lcd.print("]");
    } else {
      lcd.print(drum.pads[padNum].name);
    }
    
    if (i < 2) lcd.print(" ");
  }
  
  // Show selected pad info on 3rd line (jika ada)
  if (lcd.rows() > 2) {
    lcd.setCursor(0, 2);
    lcd.print("Pad #");
    lcd.print(drum.currentPad + 1);
    lcd.print(": ");
    lcd.print(drum.pads[drum.currentPad].name);
    
    // Show volume
    lcd.setCursor(0, 3);
    lcd.print("Vol: ");
    lcd.print(drum.pads[drum.currentPad].volume);
    lcd.print("%");
  }
}

// ============ EQ SCREEN ============
void displayEQScreen() {
  lcd.clear();
  
  PadSettings* pad = &drum.pads[drum.currentPad];
  
  // Line 1: Title
  lcd.setCursor(0, 0);
  lcd.write(4);  // EQ icon
  lcd.print(" EQ - ");
  lcd.print(pad->name);
  
  // Line 2: Bass, Mid, Treble bars
  lcd.setCursor(0, 1);
  lcd.print("B:");
  drawValueBar(pad->bass, 3);
  
  // Line 3 (jika 4 line display)
  if (lcd.rows() > 2) {
    lcd.setCursor(0, 2);
    lcd.print("M:");
    drawValueBar(pad->mid, 3);
    
    lcd.setCursor(0, 3);
    lcd.print("T:");
    drawValueBar(pad->treble, 3);
  } else {
    // Untuk 2 line - gunakan scroll atau rotate display
    static unsigned long lastScroll = 0;
    static int scrollMode = 0;
    
    if (millis() - lastScroll > 1500) {
      scrollMode = (scrollMode + 1) % 3;
      lastScroll = millis();
    }
    
    lcd.setCursor(7, 1);
    switch(scrollMode) {
      case 0:
        lcd.print("B:");
        drawValueBar(pad->bass, 2);
        break;
      case 1:
        lcd.print("M:");
        drawValueBar(pad->mid, 2);
        break;
      case 2:
        lcd.print("T:");
        drawValueBar(pad->treble, 2);
        break;
    }
  }
}

// ============ VOLUME SCREEN ============
void displayVolumeScreen() {
  lcd.clear();
  
  PadSettings* pad = &drum.pads[drum.currentPad];
  
  // Line 1: Pad name
  lcd.setCursor(0, 0);
  lcd.write(2);  // Speaker icon
  lcd.print(" ");
  lcd.print(pad->name);
  lcd.print(" Volume");
  
  // Line 2: Volume bar
  lcd.setCursor(0, 1);
  lcd.print("[");
  
  int barLength = 12;  // 12 karakter untuk bar
  int filledBars = (pad->volume * barLength) / 100;
  
  for (int i = 0; i < barLength; i++) {
    if (i < filledBars) {
      lcd.write(255);  // Full block
    } else {
      lcd.write(254);  // Empty block
    }
  }
  lcd.print("]");
  
  // Line 3: Percentage
  if (lcd.rows() > 2) {
    lcd.setCursor(0, 2);
    lcd.print("Volume: ");
    lcd.print(pad->volume);
    lcd.print("%");
    
    // Line 4: Master volume
    lcd.setCursor(0, 3);
    lcd.print("Master: ");
    lcd.print(drum.masterVolume);
    lcd.print("%");
  } else {
    // Untuk 2 line
    lcd.setCursor(13, 1);
    if (pad->volume < 100) {
      lcd.print(pad->volume);
    } else {
      lcd.print("MAX");
    }
  }
}

// ============ BATTERY SCREEN ============
void displayBatteryScreen() {
  lcd.clear();
  
  // Simulasi data battery
  float voltage = 3.7;  // Contoh
  uint8_t percentage = 75;
  
  // Line 1: Battery info
  lcd.setCursor(0, 0);
  lcd.write(0);  // Battery icon
  lcd.print(" ");
  lcd.print(voltage, 2);
  lcd.print("V ");
  
  // Battery bar
  int barLength = 8;
  int filledBars = (percentage * barLength) / 100;
  for (int i = 0; i < barLength; i++) {
    if (i < filledBars) {
      lcd.write(255);
    } else {
      lcd.write(254);
    }
  }
  
  // Line 2: Percentage dan status
  lcd.setCursor(0, 1);
  lcd.print(percentage);
  lcd.print("% ");
  
  if (percentage > 50) {
    lcd.print("GOOD");
  } else if (percentage > 20) {
    lcd.print("OK");
  } else {
    lcd.print("LOW!");
  }
}

// ============ HELPER: Draw Value Bar ============
void drawValueBar(uint8_t value, uint8_t maxChars) {
  int filledChars = (value * maxChars) / 100;
  
  for (int i = 0; i < maxChars; i++) {
    if (i < filledChars) {
      lcd.write(255);  // Full block
    } else {
      lcd.write(254);  // Empty block
    }
  }
  
  // Show percentage
  lcd.print(value);
  lcd.print("%");
}

// ============ ALTERNATIVE: Multiline Display dengan Scroll ============
void displayPadDetailsWithScroll() {
  static unsigned long lastScroll = 0;
  static int scrollLine = 0;
  
  if (millis() - lastScroll > 2000) {
    scrollLine = (scrollLine + 1) % 3;
    lastScroll = millis();
  }
  
  PadSettings* pad = &drum.pads[drum.currentPad];
  
  lcd.clear();
  
  switch(scrollLine) {
    case 0:
      // Show pad name dan volume
      lcd.setCursor(0, 0);
      lcd.print("Pad: ");
      lcd.print(pad->name);
      
      lcd.setCursor(0, 1);
      lcd.print("Vol: ");
      lcd.print(pad->volume);
      lcd.print("%");
      break;
      
    case 1:
      // Show Bass, Mid, Treble
      lcd.setCursor(0, 0);
      lcd.print("Bass:");
      lcd.print(pad->bass);
      lcd.print("% Mid:");
      lcd.print(pad->mid);
      lcd.print("%");
      
      lcd.setCursor(0, 1);
      lcd.print("Treble:");
      lcd.print(pad->treble);
      lcd.print("%");
      break;
      
    case 2:
      // Show status
      lcd.setCursor(0, 0);
      lcd.print("Pad #");
      lcd.print(drum.currentPad + 1);
      
      lcd.setCursor(0, 1);
      lcd.print("Ready to Play!");
      break;
  }
}

// ============ DEBUG: Print LCD Info ============
void printLCDDebugInfo() {
  Serial.println("\n========== LCD DEBUG INFO ==========");
  Serial.printf("Current Pad: %d (%s)\n", drum.currentPad, drum.pads[drum.currentPad].name.c_str());
  Serial.printf("Pad Volume: %d%%\n", drum.pads[drum.currentPad].volume);
  Serial.printf("Master Volume: %d%%\n", drum.masterVolume);
  Serial.printf("EQ - Bass: %d%%, Mid: %d%%, Treble: %d%%\n", 
                drum.pads[drum.currentPad].bass,
                drum.pads[drum.currentPad].mid,
                drum.pads[drum.currentPad].treble);
  Serial.printf("Menu Mode: %d\n", drum.menuMode);
  Serial.println("====================================\n");
}
