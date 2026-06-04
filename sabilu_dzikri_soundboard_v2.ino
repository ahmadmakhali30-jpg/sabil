#include <Arduino.h>
#include <BluetoothSerial.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <Wire.h>
#include <Preferences.h>
#include <Ticker.h>
#include <vector>
#include <TFT_eSPI.h>

// ============================================================================
// PIN DEFINITIONS - ESP32 DEV MODULE
// ============================================================================
#define TFT_CS 5
#define TFT_DC 2
#define TFT_RST 4
#define TFT_MOSI 23
#define TFT_SCK 18

#define ENCODER_CLK 19
#define ENCODER_DT 17
#define ENCODER_SW 21

#define MIC_PIN 34

// I2S Audio Output Pins (Optional - untuk audio playback)
#define I2S_BCK 26
#define I2S_LRCK 25
#define I2S_DIN 22

// ============================================================================
// DISPLAY DEFINITIONS
// ============================================================================
#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 128
#define PRESET_COUNT 10
#define MAX_PRESET_NAME_LENGTH 20
#define NUM_EQ_BARS 16

// ============================================================================
// DATA STRUCTURES
// ============================================================================
struct PresetSettings {
  char name[MAX_PRESET_NAME_LENGTH];
  char filename[32];  // Nama file audio di SPIFFS
  uint8_t threshold;  // 1-100 sensitivity
  uint8_t volume;     // 1-100 output volume
  uint8_t decay;      // 1-100 sound decay time
};

struct SystemSettings {
  PresetSettings presets[PRESET_COUNT];
  uint8_t currentPreset;
  bool bluetoothEnabled;
  uint8_t masterVolume;
};

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
TFT_eSPI tft = TFT_eSPI();
BluetoothSerial SerialBT;
Preferences preferences;

SystemSettings systemSettings;

// Menu states
uint8_t currentMenu = 0; // 0=Main, 1=Preset Select, 2=Settings, 3=Edit Threshold, 4=Edit Volume, 5=Edit Decay, 6=Edit Name
uint8_t selectedEditField = 0;

// Display variables
int16_t eqBars[NUM_EQ_BARS] = {0};

// Input variables
bool encoderPressed = false;
int lastEncoderValue = 0;
volatile int encoderValue = 0;
uint32_t lastEncoderInterrupt = 0;

// Audio variables
uint32_t decayTimer[PRESET_COUNT] = {0};
uint8_t decayActive[PRESET_COUNT] = {0};

// Bluetooth command buffer
char btBuffer[128] = {0};
uint8_t btBufferIndex = 0;

// Character map untuk edit nama
const char charMap[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz0123456789-_";
int charMapSize = strlen(charMap);
uint8_t nameEditIndex = 0;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================
void initDisplay();
void initAudio();
void initBluetooth();
void initEncoder();
void initPresets();
void loadPresetsFromFlash();
void savePresetsToFlash();
void listSPIFFSFiles();
void drawMainMenu();
void drawPresetMenu();
void drawSettingsMenu();
void drawEditThresholdMenu();
void drawEditVolumeMenu();
void drawEditDecayMenu();
void drawEditNameMenu();
void handleEncoderInput();
void handleMicInput();
void updateEQBars();
void playSound(uint8_t presetIndex);
void handleBluetoothCommand();
void processBluetoothCommand(const char *cmd);
void updateDisplay();
void IRAM_ATTR encoderISR();
void printInfo(const char* msg);

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n============================================");
  Serial.println("     SABILU DZIKRI - ESP32 SOUNDBOARD");
  Serial.println("     With 10 Audio Presets from SPIFFS");
  Serial.println("============================================");
  
  // Initialize SPIFFS for preset storage and audio files
  if (!SPIFFS.begin(true)) {
    Serial.println("[ERROR] SPIFFS Mount Failed");
    printInfo("SPIFFS Mount Failed");
  } else {
    Serial.println("[OK] SPIFFS Mounted Successfully");
    listSPIFFSFiles();
  }
  
  // Initialize Preferences (NVS)
  preferences.begin("soundboard", false);
  
  // Initialize display first
  Serial.println("[*] Initializing Display...");
  initDisplay();
  printInfo("Initializing...");
  delay(500);
  
  Serial.println("[*] Initializing Audio...");
  initAudio();
  
  Serial.println("[*] Initializing Bluetooth...");
  initBluetooth();
  
  Serial.println("[*] Initializing Encoder...");
  initEncoder();
  
  Serial.println("[*] Initializing Presets...");
  initPresets();
  loadPresetsFromFlash();
  
  Serial.println("\n[OK] Setup Complete!");
  Serial.println("[OK] Sabilu Dzikri Soundboard Ready!");
  Serial.println("============================================\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  handleEncoderInput();
  handleMicInput();
  handleBluetoothCommand();
  updateEQBars();
  updateDisplay();
  
  // Check for decay timeout
  for (int i = 0; i < PRESET_COUNT; i++) {
    if (decayActive[i] && (millis() - decayTimer[i] > (systemSettings.presets[i].decay * 10))) {
      decayActive[i] = 0;
    }
  }
  
  delay(50);
}

// ============================================================================
// INITIALIZATION FUNCTIONS
// ============================================================================
void initDisplay() {
  tft.init();
  tft.setRotation(1);  // Landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.println("Display initialized");
  Serial.println("[OK] Display initialized (160x128)");
  delay(500);
}

void initAudio() {
  // Configure ADC for microphone input
  pinMode(MIC_PIN, INPUT);
  analogSetAttenuation(ADC_11db);
  
  // I2S pins configuration for audio output (optional)
  pinMode(I2S_BCK, OUTPUT);
  pinMode(I2S_LRCK, OUTPUT);
  pinMode(I2S_DIN, OUTPUT);
  
  Serial.println("[OK] Audio system initialized");
}

void initBluetooth() {
  if (SerialBT.begin("Sabilu Dzikri")) {
    Serial.println("[OK] Bluetooth initialized as: 'Sabilu Dzikri'");
  } else {
    Serial.println("[ERROR] Bluetooth initialization failed");
  }
}

void initEncoder() {
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), encoderISR, FALLING);
  Serial.println("[OK] Encoder initialized (CLK=19, DT=17, SW=21)");
}

void initPresets() {
  // Initialize default preset names and settings
  for (int i = 0; i < PRESET_COUNT; i++) {
    sprintf(systemSettings.presets[i].name, "Preset %d", i + 1);
    sprintf(systemSettings.presets[i].filename, "/preset_%d.wav", i + 1);
    systemSettings.presets[i].threshold = 50;
    systemSettings.presets[i].volume = 80;
    systemSettings.presets[i].decay = 50;
  }
  systemSettings.currentPreset = 0;
  systemSettings.bluetoothEnabled = true;
  systemSettings.masterVolume = 100;
  Serial.println("[OK] Presets initialized (10 presets)");
}

// ============================================================================
// FLASH STORAGE FUNCTIONS
// ============================================================================
void listSPIFFSFiles() {
  Serial.println("\n[*] Files in SPIFFS:");
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  
  int fileCount = 0;
  while (file) {
    Serial.print("    ");
    Serial.print(file.name());
    Serial.print(" - ");
    Serial.print(file.size());
    Serial.println(" bytes");
    fileCount++;
    file = root.openNextFile();
  }
  Serial.print("[OK] Total files: ");
  Serial.println(fileCount);
}

void savePresetsToFlash() {
  File file = SPIFFS.open("/config.bin", "w");
  if (!file) {
    Serial.println("[ERROR] Failed to open config file for writing");
    return;
  }
  
  file.write((uint8_t *)&systemSettings, sizeof(SystemSettings));
  file.close();
  Serial.println("[OK] Presets configuration saved to flash");
}

void loadPresetsFromFlash() {
  if (!SPIFFS.exists("/config.bin")) {
    Serial.println("[INFO] No config file found, using defaults");
    savePresetsToFlash();
    return;
  }
  
  File file = SPIFFS.open("/config.bin", "r");
  if (!file) {
    Serial.println("[ERROR] Failed to open config file for reading");
    return;
  }
  
  file.read((uint8_t *)&systemSettings, sizeof(SystemSettings));
  file.close();
  Serial.println("[OK] Presets configuration loaded from flash");
}

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================
void drawMainMenu() {
  tft.fillScreen(TFT_BLACK);
  
  // Draw border
  tft.drawRect(0, 0, 160, 128, TFT_CYAN);
  
  // Title
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(15, 5);
  tft.println("Sabilu Dzikri");
  
  // Current Preset
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(5, 25);
  tft.print("Preset: ");
  tft.setTextColor(TFT_WHITE);
  tft.println(systemSettings.presets[systemSettings.currentPreset].name);
  
  // Threshold/Sensitivity
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(5, 35);
  tft.print("Sensitivity: ");
  tft.setTextColor(TFT_GREEN);
  tft.print(systemSettings.presets[systemSettings.currentPreset].threshold);
  tft.println("%");
  
  // EQ Bars (16 bars from left to right)
  int barStartX = 10;
  int barStartY = 50;
  int barWidth = 8;
  int barSpacing = 2;
  int maxBarHeight = 25;
  
  for (int i = 0; i < NUM_EQ_BARS; i++) {
    int x = barStartX + (i * (barWidth + barSpacing));
    int barHeight = (eqBars[i] * maxBarHeight) / 255;
    int y = barStartY + maxBarHeight - barHeight;
    
    // Draw bar
    if (barHeight > 0) {
      tft.fillRect(x, y, barWidth, barHeight, TFT_GREEN);
    }
    // Draw background outline
    tft.drawRect(x, barStartY, barWidth, maxBarHeight, TFT_DARKGREY);
  }
  
  // Menu options
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(5, 85);
  tft.println("[MENU OPTIONS]");
  
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(5, 95);
  tft.println("1. Select Preset");
  
  tft.setCursor(5, 103);
  tft.println("2. Edit Settings");
  
  tft.setCursor(5, 111);
  tft.println("3. Bluetooth Status");
  
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(5, 121);
  tft.print("Rotate encoder to select");
}

void drawPresetMenu() {
  tft.fillScreen(TFT_BLACK);
  
  // Draw border
  tft.drawRect(0, 0, 160, 128, TFT_CYAN);
  
  // Title
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(20, 5);
  tft.println("SELECT PRESET");
  
  tft.setTextSize(1);
  
  // Display all 10 presets with selection indicator
  for (int i = 0; i < PRESET_COUNT; i++) {
    int yPos = 25 + (i * 10);
    
    if (i == systemSettings.currentPreset) {
      // Highlight current selection
      tft.fillRect(5, yPos - 1, 150, 9, TFT_DARKGREEN);
      tft.setTextColor(TFT_YELLOW, TFT_DARKGREEN);
      tft.setCursor(10, yPos);
      tft.print(">> [");
      tft.print(i + 1);
      tft.print("] ");
      tft.println(systemSettings.presets[i].name);
    } else {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setCursor(10, yPos);
      tft.print("   [");
      tft.print(i + 1);
      tft.print("] ");
      tft.println(systemSettings.presets[i].name);
    }
  }
}

void drawSettingsMenu() {
  tft.fillScreen(TFT_BLACK);
  
  // Draw border
  tft.drawRect(0, 0, 160, 128, TFT_CYAN);
  
  // Title
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(35, 5);
  tft.println("SETTINGS");
  
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  
  PresetSettings &preset = systemSettings.presets[systemSettings.currentPreset];
  
  tft.setCursor(5, 25);
  tft.print("Preset: ");
  tft.setTextColor(TFT_YELLOW);
  tft.println(preset.name);
  
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(5, 35);
  tft.print("Threshold: ");
  tft.setTextColor(TFT_GREEN);
  tft.print(preset.threshold);
  tft.println("%");
  
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(5, 45);
  tft.print("Volume: ");
  tft.setTextColor(TFT_GREEN);
  tft.print(preset.volume);
  tft.println("%");
  
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(5, 55);
  tft.print("Decay: ");
  tft.setTextColor(TFT_GREEN);
  tft.print(preset.decay);
  tft.println("%");
  
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(5, 65);
  tft.print("Bluetooth: ");
  tft.setTextColor(TFT_CYAN);
  tft.println(systemSettings.bluetoothEnabled ? "ON" : "OFF");
  
  // Menu options
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(5, 80);
  tft.println("[EDIT OPTIONS]");
  
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(5, 90);
  tft.println("1. Edit Threshold");
  
  tft.setCursor(5, 100);
  tft.println("2. Edit Volume");
  
  tft.setCursor(5, 110);
  tft.println("3. Edit Decay");
}

void drawEditThresholdMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0, 0, 160, 128, TFT_CYAN);
  
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(15, 5);
  tft.println("THRESHOLD");
  
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(5, 25);
  tft.print("Preset: ");
  tft.setTextColor(TFT_YELLOW);
  tft.println(systemSettings.presets[systemSettings.currentPreset].name);
  
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(5, 40);
  tft.print("Current Value: ");
  tft.setTextColor(TFT_GREEN);
  tft.print(systemSettings.presets[systemSettings.currentPreset].threshold);
  tft.println("%");
  
  // Draw progress bar
  int barWidth = (systemSettings.presets[systemSettings.currentPreset].threshold * 140) / 100;
  tft.drawRect(5, 55, 150, 15, TFT_CYAN);
  if (barWidth > 0) {
    tft.fillRect(5, 55, barWidth, 15, TFT_GREEN);
  }
  
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(5, 75);
  tft.println("Min: 1%       Max: 100%");
  
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(5, 90);
  tft.println("Rotate to adjust");
  
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(5, 105);
  tft.println("Press to save & exit");
  
  tft.setCursor(5, 120);
  tft.print("Range: 1-100%");
}

void drawEditVolumeMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0, 0, 160, 128, TFT_CYAN);
  
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(30, 5);
  tft.println("VOLUME");
  
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(5, 25);
  tft.print("Preset: ");
  tft.setTextColor(TFT_YELLOW);
  tft.println(systemSettings.presets[systemSettings.currentPreset].name);
  
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(5, 40);
  tft.print("Current Value: ");
  tft.setTextColor(TFT_GREEN);
  tft.print(systemSettings.presets[systemSettings.currentPreset].volume);
  tft.println("%");
  
  // Draw progress bar
  int barWidth = (systemSettings.presets[systemSettings.currentPreset].volume * 140) / 100;
  tft.drawRect(5, 55, 150, 15, TFT_CYAN);
  if (barWidth > 0) {
    tft.fillRect(5, 55, barWidth, 15, TFT_BLUE);
  }
  
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(5, 75);
  tft.println("Min: 1%       Max: 100%");
  
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(5, 90);
  tft.println("Rotate to adjust");
  
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(5, 105);
  tft.println("Press to save & exit");
  
  tft.setCursor(5, 120);
  tft.print("Range: 1-100%");
}

void drawEditDecayMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0, 0, 160, 128, TFT_CYAN);
  
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(35, 5);
  tft.println("DECAY");
  
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(5, 25);
  tft.print("Preset: ");
  tft.setTextColor(TFT_YELLOW);
  tft.println(systemSettings.presets[systemSettings.currentPreset].name);
  
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(5, 40);
  tft.print("Current Value: ");
  tft.setTextColor(TFT_GREEN);
  tft.print(systemSettings.presets[systemSettings.currentPreset].decay);
  tft.println("%");
  
  // Draw progress bar
  int barWidth = (systemSettings.presets[systemSettings.currentPreset].decay * 140) / 100;
  tft.drawRect(5, 55, 150, 15, TFT_CYAN);
  if (barWidth > 0) {
    tft.fillRect(5, 55, barWidth, 15, TFT_MAGENTA);
  }
  
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(5, 75);
  tft.println("Min: 1%       Max: 100%");
  
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(5, 90);
  tft.println("Rotate to adjust");
  
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(5, 105);
  tft.println("Press to save & exit");
  
  tft.setCursor(5, 120);
  tft.print("Range: 1-100%");
}

void updateDisplay() {
  static uint32_t lastUpdate = 0;
  if (millis() - lastUpdate < 150) return; // Update every 150ms
  lastUpdate = millis();
  
  switch (currentMenu) {
    case 0:
      drawMainMenu();
      break;
    case 1:
      drawPresetMenu();
      break;
    case 2:
      drawSettingsMenu();
      break;
    case 3:
      drawEditThresholdMenu();
      break;
    case 4:
      drawEditVolumeMenu();
      break;
    case 5:
      drawEditDecayMenu();
      break;
  }
}

// ============================================================================
// INPUT HANDLING FUNCTIONS
// ============================================================================
void IRAM_ATTR encoderISR() {
  if (millis() - lastEncoderInterrupt < 15) return; // Debounce
  lastEncoderInterrupt = millis();
  
  if (digitalRead(ENCODER_DT) == LOW) {
    encoderValue++;
  } else {
    encoderValue--;
  }
}

void handleEncoderInput() {
  // Check button press
  if (digitalRead(ENCODER_SW) == LOW && !encoderPressed) {
    encoderPressed = true;
    
    // Handle button press based on current menu
    switch (currentMenu) {
      case 0: // Main menu - go to preset selection
        currentMenu = 1;
        lastEncoderValue = 0;
        encoderValue = 0;
        break;
      case 1: // Preset selection - confirm and go to main
        currentMenu = 0;
        lastEncoderValue = 0;
        encoderValue = 0;
        savePresetsToFlash();
        break;
      case 2: // Settings - go back to main
        currentMenu = 0;
        lastEncoderValue = 0;
        encoderValue = 0;
        break;
      case 3: // Edit Threshold - save and go back
        currentMenu = 2;
        lastEncoderValue = 0;
        encoderValue = 0;
        savePresetsToFlash();
        break;
      case 4: // Edit Volume - save and go back
        currentMenu = 2;
        lastEncoderValue = 0;
        encoderValue = 0;
        savePresetsToFlash();
        break;
      case 5: // Edit Decay - save and go back
        currentMenu = 2;
        lastEncoderValue = 0;
        encoderValue = 0;
        savePresetsToFlash();
        break;
    }
    
    delay(200); // Debounce delay
  } else if (digitalRead(ENCODER_SW) == HIGH) {
    encoderPressed = false;
  }
  
  // Handle rotation
  if (encoderValue != lastEncoderValue) {
    int delta = encoderValue - lastEncoderValue;
    lastEncoderValue = encoderValue;
    
    switch (currentMenu) {
      case 0: // Main menu - select which menu to enter
        selectedEditField = (selectedEditField + delta + 3) % 3;
        if (selectedEditField == 0 && delta > 0) {
          currentMenu = 1; // Go to preset selection
          lastEncoderValue = 0;
          encoderValue = 0;
        } else if (selectedEditField == 1 && delta > 0) {
          currentMenu = 2; // Go to settings
          lastEncoderValue = 0;
          encoderValue = 0;
        }
        break;
      case 1: // Preset selection
        systemSettings.currentPreset = (systemSettings.currentPreset + delta + PRESET_COUNT) % PRESET_COUNT;
        Serial.print("[INFO] Selected Preset: ");
        Serial.println(systemSettings.currentPreset + 1);
        break;
      case 2: // Settings - cycle through settings
        selectedEditField = (selectedEditField + delta + 4) % 4;
        if (selectedEditField == 0) {
          currentMenu = 3; // Edit Threshold
          lastEncoderValue = 0;
          encoderValue = 0;
        } else if (selectedEditField == 1) {
          currentMenu = 4; // Edit Volume
          lastEncoderValue = 0;
          encoderValue = 0;
        } else if (selectedEditField == 2) {
          currentMenu = 5; // Edit Decay
          lastEncoderValue = 0;
          encoderValue = 0;
        }
        break;
      case 3: // Edit Threshold (1-100)
        systemSettings.presets[systemSettings.currentPreset].threshold = 
          constrain(systemSettings.presets[systemSettings.currentPreset].threshold + delta, 1, 100);
        Serial.print("[INFO] Threshold: ");
        Serial.println(systemSettings.presets[systemSettings.currentPreset].threshold);
        break;
      case 4: // Edit Volume (1-100)
        systemSettings.presets[systemSettings.currentPreset].volume = 
          constrain(systemSettings.presets[systemSettings.currentPreset].volume + delta, 1, 100);
        Serial.print("[INFO] Volume: ");
        Serial.println(systemSettings.presets[systemSettings.currentPreset].volume);
        break;
      case 5: // Edit Decay (1-100)
        systemSettings.presets[systemSettings.currentPreset].decay = 
          constrain(systemSettings.presets[systemSettings.currentPreset].decay + delta, 1, 100);
        Serial.print("[INFO] Decay: ");
        Serial.println(systemSettings.presets[systemSettings.currentPreset].decay);
        break;
    }
  }
}

void handleMicInput() {
  uint16_t micValue = analogRead(MIC_PIN);
  
  // Check if any preset threshold is crossed
  for (int i = 0; i < PRESET_COUNT; i++) {
    uint16_t threshold = (systemSettings.presets[i].threshold * 4095) / 100;
    if (micValue > threshold && !decayActive[i]) {
      decayActive[i] = 1;
      decayTimer[i] = millis();
      playSound(i);
    }
  }
}

// ============================================================================
// AUDIO PLAYBACK
// ============================================================================
void playSound(uint8_t presetIndex) {
  if (presetIndex >= PRESET_COUNT) return;
  
  PresetSettings &preset = systemSettings.presets[presetIndex];
  uint8_t volumeLevel = (preset.volume * systemSettings.masterVolume) / 100;
  
  Serial.print("[SOUND] Playing: ");
  Serial.print(preset.name);
  Serial.print(" | File: ");
  Serial.print(preset.filename);
  Serial.print(" | Volume: ");
  Serial.println(volumeLevel);
  
  // Check if preset file exists in SPIFFS
  if (SPIFFS.exists(preset.filename)) {
    Serial.print("[OK] Preset file found: ");
    Serial.println(preset.filename);
    
    // TODO: Implement audio playback from SPIFFS
    // This can be done using:
    // 1. I2S Audio Library for WAV playback
    // 2. ESP_IDF I2S driver
    // 3. PWM output for simple audio
  } else {
    Serial.print("[WARNING] Preset file not found: ");
    Serial.println(preset.filename);
  }
  
  // Send to Bluetooth if enabled
  if (systemSettings.bluetoothEnabled && SerialBT.hasClient()) {
    SerialBT.print("PLAY:");
    SerialBT.print(presetIndex);
    SerialBT.print(":");
    SerialBT.println(volumeLevel);
  }
}

void updateEQBars() {
  static uint32_t lastEQUpdate = 0;
  if (millis() - lastEQUpdate < 50) return;
  lastEQUpdate = millis();
  
  uint16_t micValue = analogRead(MIC_PIN);
  
  // Shift bars and add new value
  for (int i = 0; i < NUM_EQ_BARS - 1; i++) {
    eqBars[i] = eqBars[i + 1];
  }
  eqBars[NUM_EQ_BARS - 1] = (micValue * 255) / 4095;
}

// ============================================================================
// BLUETOOTH HANDLING
// ============================================================================
void handleBluetoothCommand() {
  while (SerialBT.available()) {
    char c = SerialBT.read();
    
    if (c == '\n' || c == '\r') {
      if (btBufferIndex > 0) {
        processBluetoothCommand(btBuffer);
        btBufferIndex = 0;
        memset(btBuffer, 0, sizeof(btBuffer));
      }
    } else if (btBufferIndex < sizeof(btBuffer) - 1) {
      btBuffer[btBufferIndex++] = c;
    }
  }
}

void processBluetoothCommand(const char *cmd) {
  Serial.print("[BT] Command: ");
  Serial.println(cmd);
  
  if (strncmp(cmd, "PRESET:", 7) == 0) {
    int presetNum = atoi(cmd + 7);
    if (presetNum >= 0 && presetNum < PRESET_COUNT) {
      systemSettings.currentPreset = presetNum;
      SerialBT.println("OK");
    } else {
      SerialBT.println("ERROR");
    }
  } else if (strncmp(cmd, "THRESHOLD:", 10) == 0) {
    int val = atoi(cmd + 10);
    if (val >= 1 && val <= 100) {
      systemSettings.presets[systemSettings.currentPreset].threshold = val;
      SerialBT.println("OK");
    } else {
      SerialBT.println("ERROR");
    }
  } else if (strncmp(cmd, "VOLUME:", 7) == 0) {
    int val = atoi(cmd + 7);
    if (val >= 1 && val <= 100) {
      systemSettings.presets[systemSettings.currentPreset].volume = val;
      SerialBT.println("OK");
    } else {
      SerialBT.println("ERROR");
    }
  } else if (strncmp(cmd, "DECAY:", 6) == 0) {
    int val = atoi(cmd + 6);
    if (val >= 1 && val <= 100) {
      systemSettings.presets[systemSettings.currentPreset].decay = val;
      SerialBT.println("OK");
    } else {
      SerialBT.println("ERROR");
    }
  } else if (strncmp(cmd, "NAME:", 5) == 0) {
    strncpy(systemSettings.presets[systemSettings.currentPreset].name, cmd + 5, MAX_PRESET_NAME_LENGTH - 1);
    systemSettings.presets[systemSettings.currentPreset].name[MAX_PRESET_NAME_LENGTH - 1] = '\0';
    SerialBT.println("OK");
  } else if (strcmp(cmd, "STATUS") == 0) {
    SerialBT.print("PRESET:");
    SerialBT.println(systemSettings.currentPreset);
    SerialBT.print("NAME:");
    SerialBT.println(systemSettings.presets[systemSettings.currentPreset].name);
    SerialBT.print("THRESHOLD:");
    SerialBT.println(systemSettings.presets[systemSettings.currentPreset].threshold);
    SerialBT.print("VOLUME:");
    SerialBT.println(systemSettings.presets[systemSettings.currentPreset].volume);
    SerialBT.print("DECAY:");
    SerialBT.println(systemSettings.presets[systemSettings.currentPreset].decay);
  } else if (strcmp(cmd, "SAVE") == 0) {
    savePresetsToFlash();
    SerialBT.println("OK");
  } else if (strcmp(cmd, "REBOOT") == 0) {
    SerialBT.println("OK");
    delay(500);
    ESP.restart();
  } else if (strcmp(cmd, "VERSION") == 0) {
    SerialBT.println("Sabilu Dzikri v1.0 with 10 Audio Presets");
  } else if (strcmp(cmd, "LISTFILES") == 0) {
    listSPIFFSFiles();
    SerialBT.println("Check Serial Monitor");
  } else {
    SerialBT.println("UNKNOWN");
  }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================
void printInfo(const char* msg) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 60);
  tft.println(msg);
}
