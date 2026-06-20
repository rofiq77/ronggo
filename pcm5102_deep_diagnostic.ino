/*
 * ESP32 Super Mini + PCM5102 DAC - DEEP DIAGNOSTIC
 * Step-by-step troubleshooting untuk masalah "tidak bunyi"
 */

#include <driver/i2s.h>
#include <math.h>

// ============ PIN Configuration ============
#define I2S_BCLK_PIN 12
#define I2S_LRCK_PIN 13
#define I2S_DOUT_PIN 14
#define I2S_PORT I2S_NUM_0

#define LED_PIN 10
#define BTN_PIN 11

// ============ Audio Config ============
#define SAMPLE_RATE 44100
#define BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define BUFFER_SIZE 512

int16_t audio_buffer[BUFFER_SIZE];
bool i2s_ok = false;

// ============ SETUP ============
void setup() {
  Serial.begin(115200);
  delay(3000);
  
  Serial.println("\n\n╔════════════════════════════════════════╗");
  Serial.println("║   PCM5102 DEEP DIAGNOSTIC v2.0        ║");
  Serial.println("║   ESP32 Super Mini Troubleshooting     ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN, LOW);
  
  // Step 1: Hardware Check
  Serial.println("═══════════════════════════════════════");
  Serial.println("STEP 1: HARDWARE VERIFICATION");
  Serial.println("═══════════════════════════════════════\n");
  hardwareCheck();
  
  delay(2000);
  
  // Step 2: GPIO Test
  Serial.println("\n═══════════════════════════════════════");
  Serial.println("STEP 2: GPIO PIN TEST");
  Serial.println("═══════════════════════════════════════\n");
  gpioTest();
  
  delay(2000);
  
  // Step 3: I2S Init
  Serial.println("\n═══════════════════════════════════════");
  Serial.println("STEP 3: I2S INITIALIZATION");
  Serial.println("═══════════════════════════════════════\n");
  if (testI2SInit()) {
    i2s_ok = true;
    digitalWrite(LED_PIN, HIGH);
    Serial.println("✅ I2S READY!\n");
  } else {
    Serial.println("❌ I2S FAILED!\n");
  }
  
  delay(2000);
  
  // Step 4: PCM5102 Connection Test
  Serial.println("\n═══════════════════════════════════════");
  Serial.println("STEP 4: PCM5102 CONNECTION TEST");
  Serial.println("═══════════════════════════════════════\n");
  testPCM5102Connection();
  
  delay(2000);
  
  Serial.println("\n═══════════════════════════════════════");
  Serial.println("READY FOR AUDIO TEST");
  Serial.println("═══════════════════════════════════════\n");
  
  printCommandMenu();
}

// ============ MAIN LOOP ============
void loop() {
  // Button quick test
  if (digitalRead(BTN_PIN) == LOW) {
    delay(500);
    Serial.println("\n🔘 BUTTON: Playing 440Hz...\n");
    playTone(440, 2000);
    delay(1000);
  }
  
  // Serial command
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    handleCommand(cmd);
  }
  
  delay(100);
}

// ============ STEP 1: HARDWARE CHECK ============
void hardwareCheck() {
  Serial.println("Checking hardware specifications...\n");
  
  Serial.println("📌 CHIP INFO:");
  Serial.printf("  Chip: %s\n", CONFIG_IDF_TARGET);
  Serial.printf("  Revision: %d\n", ESP.getChipRevision());
  Serial.printf("  Cores: %d\n", ESP.getChipCores());
  Serial.printf("  Flash: %d MB\n", ESP.getFlashChipSize() / (1024*1024));
  
  Serial.println("\n📌 MEMORY INFO:");
  Serial.printf("  Total PSRAM: %d bytes\n", ESP.getPsramSize());
  Serial.printf("  Free Heap: %d bytes\n", esp_get_free_heap_size());
  Serial.printf("  Largest Free Block: %d bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
  
  Serial.println("\n📌 SUPPLY VOLTAGE:");
  uint32_t adc_reading = analogRead(35);  // ADC pin
  Serial.printf("  ADC Raw: %d\n", adc_reading);
  
  Serial.println("\n✅ Hardware check complete\n");
}

// ============ STEP 2: GPIO TEST ============
void gpioTest() {
  Serial.println("Testing GPIO pins...\n");
  
  Serial.println("Setting pins to OUTPUT mode...");
  pinMode(I2S_BCLK_PIN, OUTPUT);
  pinMode(I2S_LRCK_PIN, OUTPUT);
  pinMode(I2S_DOUT_PIN, OUTPUT);
  delay(100);
  
  Serial.println("\nTest 1: Set HIGH");
  digitalWrite(I2S_BCLK_PIN, HIGH);
  digitalWrite(I2S_LRCK_PIN, HIGH);
  digitalWrite(I2S_DOUT_PIN, HIGH);
  delay(100);
  
  Serial.printf("  GPIO12 (BCLK): %d (should be 1)\n", digitalRead(I2S_BCLK_PIN));
  Serial.printf("  GPIO13 (LRCK): %d (should be 1)\n", digitalRead(I2S_LRCK_PIN));
  Serial.printf("  GPIO14 (DOUT): %d (should be 1)\n", digitalRead(I2S_DOUT_PIN));
  
  Serial.println("\nTest 2: Set LOW");
  digitalWrite(I2S_BCLK_PIN, LOW);
  digitalWrite(I2S_LRCK_PIN, LOW);
  digitalWrite(I2S_DOUT_PIN, LOW);
  delay(100);
  
  Serial.printf("  GPIO12 (BCLK): %d (should be 0)\n", digitalRead(I2S_BCLK_PIN));
  Serial.printf("  GPIO13 (LRCK): %d (should be 0)\n", digitalRead(I2S_LRCK_PIN));
  Serial.printf("  GPIO14 (DOUT): %d (should be 0)\n", digitalRead(I2S_DOUT_PIN));
  
  Serial.println("\nTest 3: Toggle pattern");
  for (int i = 0; i < 5; i++) {
    digitalWrite(I2S_BCLK_PIN, HIGH);
    delayMicroseconds(100);
    digitalWrite(I2S_BCLK_PIN, LOW);
    delayMicroseconds(100);
  }
  Serial.println("  BCLK toggled 5x (should see blinking)\n");
  
  Serial.println("✅ GPIO test complete\n");
}

// ============ STEP 3: I2S INITIALIZATION TEST ============
bool testI2SInit() {
  Serial.println("Attempting I2S initialization...\n");
  
  // Config
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = BITS_PER_SAMPLE,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 32,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };
  
  Serial.println("Config:");
  Serial.printf("  Mode: Master TX\n");
  Serial.printf("  Sample Rate: %d Hz\n", SAMPLE_RATE);
  Serial.printf("  Bits: 16\n");
  Serial.printf("  Channels: 2 (Stereo)\n");
  Serial.printf("  DMA Buffers: 4 x 32 samples\n\n");
  
  Serial.println("Installing I2S driver...");
  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("  ❌ FAILED: Error code %d\n", err);
    Serial.println("  Possible causes:");
    Serial.println("    - I2S already installed");
    Serial.println("    - Memory not enough");
    Serial.println("    - Invalid configuration\n");
    return false;
  }
  Serial.println("  ✅ Driver installed\n");
  
  // Pin config
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK_PIN,
    .ws_io_num = I2S_LRCK_PIN,
    .data_out_num = I2S_DOUT_PIN,
    .data_in_num = I2S_PIN_NO_CHANGE,
    .mck_io_num = I2S_PIN_NO_CHANGE
  };
  
  Serial.println("Setting pins...");
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("  ❌ FAILED: Error code %d\n", err);
    i2s_driver_uninstall(I2S_PORT);
    return false;
  }
  Serial.println("  ✅ Pins configured\n");
  
  Serial.println("Clearing DMA buffer...");
  err = i2s_zero_dma_buffer(I2S_PORT);
  if (err != ESP_OK) {
    Serial.printf("  ❌ FAILED: Error code %d\n", err);
    i2s_driver_uninstall(I2S_PORT);
    return false;
  }
  Serial.println("  ✅ DMA cleared\n");
  
  return true;
}

// ============ STEP 4: PCM5102 CONNECTION TEST ============
void testPCM5102Connection() {
  Serial.println("Testing PCM5102 signal output...\n");
  
  if (!i2s_ok) {
    Serial.println("❌ I2S not initialized! Cannot test.\n");
    return;
  }
  
  Serial.println("Sending test signal to PCM5102...");
  Serial.println("Check with oscilloscope if available:\n");
  
  Serial.println("Test 1: Clock signal test (1ms)");
  Serial.println("  BCLK should show ~1.4 MHz square wave");
  Serial.println("  LRCK should show ~44 kHz square wave\n");
  
  uint32_t sample_count = SAMPLE_RATE / 1000;  // 1ms worth
  
  for (uint32_t i = 0; i < sample_count; i += BUFFER_SIZE) {
    for (int j = 0; j < BUFFER_SIZE; j++) {
      audio_buffer[j] = 16000;  // Mid-level DC
    }
    
    size_t bytes_written = 0;
    esp_err_t err = i2s_write(I2S_PORT, audio_buffer, 
                              BUFFER_SIZE * sizeof(int16_t),
                              &bytes_written, 10);
    
    if (err != ESP_OK) {
      Serial.printf("  ❌ I2S write failed: %d\n", err);
      return;
    }
  }
  
  Serial.println("  ✅ Test signal sent\n");
  
  Serial.println("Test 2: Sine wave test (1 sec)");
  Serial.println("  You should hear 440Hz tone from PCM5102\n");
  
  playTone(440, 1000);
}

// ============ PLAY TONE FUNCTION ============
void playTone(uint16_t freq, uint16_t duration_ms) {
  if (!i2s_ok) {
    Serial.println("❌ I2S not ready!\n");
    return;
  }
  
  uint32_t total_samples = (SAMPLE_RATE / 1000) * duration_ms;
  uint32_t samples_sent = 0;
  
  Serial.printf("Playing %dHz for %dms...\n", freq, duration_ms);
  Serial.println("Listening...\n");
  
  for (uint32_t i = 0; i < total_samples; i += BUFFER_SIZE) {
    // Generate sine wave
    for (int j = 0; j < BUFFER_SIZE; j++) {
      float phase = 2.0 * M_PI * freq * (i + j) / SAMPLE_RATE;
      float sample = sin(phase) * 30000;  // Amplitude
      audio_buffer[j] = (int16_t)sample;
    }
    
    // Send to I2S
    size_t bytes_written = 0;
    esp_err_t err = i2s_write(I2S_PORT, audio_buffer,
                              BUFFER_SIZE * sizeof(int16_t),
                              &bytes_written, portMAX_DELAY);
    
    if (err != ESP_OK) {
      Serial.printf("❌ I2S Error: %d\n\n", err);
      return;
    }
    
    samples_sent += bytes_written / sizeof(int16_t);
  }
  
  Serial.printf("✅ Complete! Sent %d samples\n\n", samples_sent);
}

// ============ HANDLE COMMAND ============
void handleCommand(String cmd) {
  Serial.println();
  
  if (cmd == "1") {
    Serial.println("▶ Playing 440Hz tone (2 sec)\n");
    playTone(440, 2000);
    
  } else if (cmd == "2") {
    Serial.println("▶ Playing scale\n");
    int freqs[] = {262, 294, 330, 349, 392, 440, 494, 523};
    for (int f : freqs) {
      playTone(f, 300);
      delay(50);
    }
    
  } else if (cmd == "3") {
    Serial.println("▶ GPIO Test");
    gpioTest();
    
  } else if (cmd == "4") {
    Serial.println("▶ I2S Test");
    if (testI2SInit()) {
      i2s_ok = true;
      Serial.println("✅ I2S OK\n");
    }
    
  } else if (cmd == "5") {
    Serial.println("▶ Reset I2S\n");
    i2s_driver_uninstall(I2S_PORT);
    delay(1000);
    if (testI2SInit()) {
      i2s_ok = true;
      digitalWrite(LED_PIN, HIGH);
    }
    
  } else if (cmd == "info") {
    Serial.println("▶ System Info");
    hardwareCheck();
    
  } else if (cmd == "help" || cmd == "h") {
    printCommandMenu();
    
  } else {
    Serial.println("❌ Unknown command\n");
  }
  
  printCommandMenu();
}

// ============ PRINT MENU ============
void printCommandMenu() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║         COMMAND MENU                   ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  Serial.println("AUDIO TESTS:");
  Serial.println("  '1' - Play 440Hz (2 sec)");
  Serial.println("  '2' - Play C major scale\n");
  
  Serial.println("DIAGNOSTIC TESTS:");
  Serial.println("  '3' - GPIO pin test");
  Serial.println("  '4' - I2S initialization test");
  Serial.println("  '5' - Reset I2S driver");
  Serial.println("  'info' - Show hardware info");
  Serial.println("  'help' - Show this menu\n");
  
  Serial.println("Or press BUTTON for instant 440Hz\n");
}

// ============ ERROR CODE GUIDE ============
void printErrorCodes() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║         ERROR CODES                    ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  Serial.println("I2S Error Codes:");
  Serial.println("  0   = ESP_OK (success)");
  Serial.println("  257 = ESP_ERR_NO_MEM (memory full)");
  Serial.println("  259 = ESP_ERR_INVALID_ARG (bad parameter)");
  Serial.println("  261 = ESP_ERR_NOT_FOUND (not installed)\n");
}
