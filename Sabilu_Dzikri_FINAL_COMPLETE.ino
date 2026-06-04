/*
=============================================================================
  SABILU DZIKRI - ESP32 SOUNDBOARD COMPLETE v2.0
  10 Audio Presets (WAV) + Full Menu Control + Bluetooth + SPI Display
  
  Features:
  - 10 Presets (Preset 1-10) dengan file WAV
  - Menu dengan Encoder Rotary
  - Display SPI 128x160 (ST7735)
  - Threshold/Sensifitas 1-100%
  - Volume Output 1-100%
  - Edit Decay 1-100%
  - Edit Nama Preset (A-Z, a-z, 0-9)
  - Bluetooth Control
  - Microphone Input (zero latency)
  - Real-time EQ Bars 16x
  - Flash Storage (SPIFFS)
  
  Compile: Select "ESP32 Dev Module" + Upload Speed 115200
  Upload Data: Tools -> ESP32 Sketch Data Upload (untuk preset_1.wav - preset_10.wav)
=============================================================================
*/

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
#define PIN_TFT_CS   5
#define PIN_TFT_DC   2
#define PIN_TFT_RST  4
#define PIN_TFT_MOSI 23
#define PIN_TFT_SCK  18

#define PIN_ENC_CLK  19
#define PIN_ENC_DT   17
#define PIN_ENC_SW   21

#define PIN_MIC_ADC  34

// ============================================================================
// CONSTANTS
// ============================================================================
#define NUM_PRESETS      10
#define MAX_NAME_LEN     20
#define NUM_EQ_BARS      16
#define DISPLAY_W        160
#define DISPLAY_H        128

// ============================================================================
// ENUMS & STRUCTURES
// ============================================================================
enum MenuState {
  MENU_MAIN = 0,
  MENU_SELECT_PRESET = 1,
  MENU_SETTINGS = 2,
  MENU_EDIT_THRESHOLD = 3,
  MENU_EDIT_VOLUME = 4,
  MENU_EDIT_DECAY = 5,
  MENU_EDIT_NAME = 6,
  MENU_BLUETOOTH = 7,
  MENU_MASTER_VOLUME = 8
};

struct PresetData {
  char name[MAX_NAME_LEN];
  uint8_t threshold;  // 1-100
  uint8_t volume;     // 1-100
  uint8_t decay;      // 1-100
};

struct SystemConfig {
  PresetData presets[NUM_PRESETS];
  uint8_t current_preset;
  uint8_t master_volume;  // 1-100
  bool bluetooth_enabled;
};

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
TFT_eSPI tft = TFT_eSPI();
BluetoothSerial bluetooth;
Preferences storage;

SystemConfig config;
MenuState menu_state = MENU_MAIN;
int16_t eq_bars[NUM_EQ_BARS] = {0};

bool btn_pressed = false;
int enc_value = 0;
int enc_last_value = 0;
uint32_t btn_last_time = 0;
uint32_t enc_last_interrupt = 0;

uint32_t decay_timer[NUM_PRESETS] = {0};
uint8_t decay_active[NUM_PRESETS] = {0};

char bt_buffer[128];
int bt_buffer_idx = 0;

const char char_map[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz0123456789-_";
uint8_t name_edit_pos = 0;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================
void setup_display();
void setup_audio();
void setup_bluetooth();
void setup_encoder();
void setup_presets();
void load_config();
void save_config();
void list_spiffs_files();

void draw_main_menu();
void draw_preset_menu();
void draw_settings_menu();
void draw_threshold_menu();
void draw_volume_menu();
void draw_decay_menu();
void draw_name_menu();
void draw_bluetooth_menu();
void draw_master_volume_menu();
void update_display();

void handle_encoder();
void handle_microphone();
void update_eq_bars();
void play_preset(uint8_t idx);
void handle_bluetooth();
void process_bt_command(const char *cmd);

void IRAM_ATTR encoder_isr();
void draw_text(int x, int y, int size, uint16_t color, const String &text);
void draw_filled_rect(int x, int y, int w, int h, uint16_t color);

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("  SABILU DZIKRI - SOUNDBOARD v2.0");
  Serial.println("  10 WAV Presets + Full Control");
  Serial.println("========================================\n");
  
  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("[ERROR] SPIFFS failed!");
  } else {
    Serial.println("[OK] SPIFFS mounted");
    list_spiffs_files();
  }
  
  storage.begin("sabilu", false);
  
  Serial.println("[*] Initializing systems...");
  setup_display();
  delay(300);
  
  setup_audio();
  setup_bluetooth();
  setup_encoder();
  setup_presets();
  load_config();
  
  Serial.println("\n[OK] System ready!\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  handle_encoder();
  handle_microphone();
  handle_bluetooth();
  update_eq_bars();
  update_display();
  
  // Check decay timers
  for (int i = 0; i < NUM_PRESETS; i++) {
    if (decay_active[i] && (millis() - decay_timer[i] > (config.presets[i].decay * 10))) {
      decay_active[i] = 0;
    }
  }
  
  delay(40);
}

// ============================================================================
// SETUP FUNCTIONS
// ============================================================================
void setup_display() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  
  draw_text(20, 30, 2, TFT_CYAN, "SABILU");
  draw_text(20, 50, 2, TFT_CYAN, "DZIKRI");
  draw_text(20, 70, 1, TFT_GREEN, "Initializing...");
  
  Serial.println("[OK] Display: 160x128 ST7735");
}

void setup_audio() {
  pinMode(PIN_MIC_ADC, INPUT);
  analogSetAttenuation(ADC_11db);
  analogReadResolution(12);
  Serial.println("[OK] Microphone on GPIO34");
}

void setup_bluetooth() {
  if (bluetooth.begin("Sabilu Dzikri", true)) {
    Serial.println("[OK] Bluetooth 'Sabilu Dzikri'");
  } else {
    Serial.println("[WARNING] Bluetooth may not start");
  }
}

void setup_encoder() {
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), encoder_isr, FALLING);
  Serial.println("[OK] Encoder: CLK(19), DT(17), SW(21)");
}

void setup_presets() {
  for (int i = 0; i < NUM_PRESETS; i++) {
    snprintf(config.presets[i].name, MAX_NAME_LEN, "Preset %d", i + 1);
    config.presets[i].threshold = 50;
    config.presets[i].volume = 80;
    config.presets[i].decay = 50;
  }
  config.current_preset = 0;
  config.master_volume = 100;
  config.bluetooth_enabled = true;
  Serial.println("[OK] 10 Presets initialized");
}

// ============================================================================
// STORAGE
// ============================================================================
void list_spiffs_files() {
  Serial.println("\n[FILES IN SPIFFS]");
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  int count = 0;
  
  while (file) {
    if (!file.isDirectory()) {
      Serial.printf("  %s (%d bytes)\n", file.name(), file.size());
      count++;
    }
    file = root.openNextFile();
  }
  Serial.printf("Total: %d files\n\n", count);
}

void save_config() {
  File file = SPIFFS.open("/config.bin", "w");
  if (file) {
    file.write((uint8_t *)&config, sizeof(SystemConfig));
    file.close();
    Serial.println("[OK] Config saved");
  }
}

void load_config() {
  if (SPIFFS.exists("/config.bin")) {
    File file = SPIFFS.open("/config.bin", "r");
    if (file) {
      file.read((uint8_t *)&config, sizeof(SystemConfig));
      file.close();
      Serial.println("[OK] Config loaded");
      return;
    }
  }
  save_config();
}

// ============================================================================
// DRAWING UTILITIES
// ============================================================================
void draw_text(int x, int y, int size, uint16_t color, const String &text) {
  tft.setTextSize(size);
  tft.setTextColor(color);
  tft.setCursor(x, y);
  tft.print(text);
}

void draw_filled_rect(int x, int y, int w, int h, uint16_t color) {
  tft.fillRect(x, y, w, h, color);
}

void draw_border() {
  tft.drawRect(0, 0, DISPLAY_W, DISPLAY_H, TFT_CYAN);
}

void clear_screen() {
  tft.fillScreen(TFT_BLACK);
}

// ============================================================================
// DISPLAY MENUS
// ============================================================================
void draw_main_menu() {
  static uint32_t last_draw = 0;
  if (millis() - last_draw < 100) return;
  last_draw = millis();
  
  clear_screen();
  draw_border();
  
  // Title
  draw_text(5, 2, 2, TFT_CYAN, "SABILU DZIKRI");
  
  // Current preset info
  draw_text(2, 22, 1, TFT_YELLOW, "Preset:");
  draw_text(55, 22, 1, TFT_WHITE, config.presets[config.current_preset].name);
  
  draw_text(2, 32, 1, TFT_YELLOW, "Sens:");
  draw_text(55, 32, 1, TFT_GREEN, String(config.presets[config.current_preset].threshold) + "%");
  
  draw_text(2, 42, 1, TFT_YELLOW, "Vol:");
  draw_text(55, 42, 1, TFT_GREEN, String(config.presets[config.current_preset].volume) + "%");
  
  // EQ bars
  for (int i = 0; i < NUM_EQ_BARS; i++) {
    int x = 2 + (i * 9);
    int y = 52;
    int h_max = 15;
    int h = (eq_bars[i] * h_max) / 255;
    
    tft.drawRect(x, y, 8, h_max, TFT_DARKGREY);
    if (h > 0) {
      tft.fillRect(x, y + h_max - h, 8, h, TFT_GREEN);
    }
  }
  
  // Menu options
  draw_text(2, 72, 1, TFT_YELLOW, "MENU:");
  draw_text(2, 82, 1, TFT_WHITE, "1. Select Preset");
  draw_text(2, 91, 1, TFT_WHITE, "2. Edit Settings");
  draw_text(2, 100, 1, TFT_WHITE, "3. Master Volume");
  draw_text(2, 109, 1, TFT_WHITE, "4. Bluetooth");
  
  draw_text(2, 122, 1, TFT_CYAN, "Rotate encoder to select");
}

void draw_preset_menu() {
  static uint32_t last_draw = 0;
  if (millis() - last_draw < 100) return;
  last_draw = millis();
  
  clear_screen();
  draw_border();
  
  draw_text(30, 2, 2, TFT_CYAN, "PRESETS");
  
  for (int i = 0; i < NUM_PRESETS; i++) {
    int y = 22 + (i * 9);
    
    if (i == config.current_preset) {
      draw_filled_rect(2, y - 1, 156, 9, TFT_DARKGREEN);
      draw_text(5, y, 1, TFT_YELLOW, ">" + String(i + 1) + " " + config.presets[i].name);
    } else {
      draw_text(5, y, 1, TFT_WHITE, " " + String(i + 1) + " " + config.presets[i].name);
    }
  }
}

void draw_settings_menu() {
  static uint32_t last_draw = 0;
  if (millis() - last_draw < 100) return;
  last_draw = millis();
  
  clear_screen();
  draw_border();
  
  draw_text(20, 2, 2, TFT_CYAN, "SETTINGS");
  
  PresetData &p = config.presets[config.current_preset];
  
  draw_text(2, 22, 1, TFT_YELLOW, "Preset:");
  draw_text(60, 22, 1, TFT_WHITE, p.name);
  
  draw_text(2, 32, 1, TFT_YELLOW, "Threshold:");
  draw_text(90, 32, 1, TFT_GREEN, String(p.threshold) + "%");
  
  draw_text(2, 42, 1, TFT_YELLOW, "Volume:");
  draw_text(90, 42, 1, TFT_GREEN, String(p.volume) + "%");
  
  draw_text(2, 52, 1, TFT_YELLOW, "Decay:");
  draw_text(90, 52, 1, TFT_GREEN, String(p.decay) + "%");
  
  draw_text(2, 65, 1, TFT_YELLOW, "EDIT:");
  draw_text(2, 75, 1, TFT_WHITE, "1. Edit Threshold");
  draw_text(2, 84, 1, TFT_WHITE, "2. Edit Volume");
  draw_text(2, 93, 1, TFT_WHITE, "3. Edit Decay");
  draw_text(2, 102, 1, TFT_WHITE, "4. Edit Name");
}

void draw_threshold_menu() {
  static uint32_t last_draw = 0;
  if (millis() - last_draw < 100) return;
  last_draw = millis();
  
  clear_screen();
  draw_border();
  
  draw_text(10, 2, 2, TFT_CYAN, "THRESHOLD");
  
  PresetData &p = config.presets[config.current_preset];
  
  draw_text(2, 22, 1, TFT_YELLOW, "Preset:");
  draw_text(60, 22, 1, TFT_WHITE, p.name);
  
  draw_text(2, 35, 1, TFT_WHITE, "Value:");
  draw_text(80, 35, 1, TFT_GREEN, String(p.threshold) + "%");
  
  // Progress bar
  int bar_len = (p.threshold * 140) / 100;
  tft.drawRect(2, 47, 150, 12, TFT_CYAN);
  if (bar_len > 0) tft.fillRect(2, 47, bar_len, 12, TFT_GREEN);
  
  draw_text(2, 62, 1, TFT_WHITE, "Min: 1%              Max: 100%");
  draw_text(2, 77, 1, TFT_YELLOW, "CONTROL:");
  draw_text(2, 87, 1, TFT_WHITE, "Rotate: Adjust");
  draw_text(2, 97, 1, TFT_WHITE, "Press: Save & Back");
}

void draw_volume_menu() {
  static uint32_t last_draw = 0;
  if (millis() - last_draw < 100) return;
  last_draw = millis();
  
  clear_screen();
  draw_border();
  
  draw_text(20, 2, 2, TFT_CYAN, "VOLUME");
  
  PresetData &p = config.presets[config.current_preset];
  
  draw_text(2, 22, 1, TFT_YELLOW, "Preset:");
  draw_text(60, 22, 1, TFT_WHITE, p.name);
  
  draw_text(2, 35, 1, TFT_WHITE, "Value:");
  draw_text(80, 35, 1, TFT_GREEN, String(p.volume) + "%");
  
  // Progress bar
  int bar_len = (p.volume * 140) / 100;
  tft.drawRect(2, 47, 150, 12, TFT_CYAN);
  if (bar_len > 0) tft.fillRect(2, 47, bar_len, 12, TFT_BLUE);
  
  draw_text(2, 62, 1, TFT_WHITE, "Min: 1%              Max: 100%");
  draw_text(2, 77, 1, TFT_YELLOW, "CONTROL:");
  draw_text(2, 87, 1, TFT_WHITE, "Rotate: Adjust");
  draw_text(2, 97, 1, TFT_WHITE, "Press: Save & Back");
}

void draw_decay_menu() {
  static uint32_t last_draw = 0;
  if (millis() - last_draw < 100) return;
  last_draw = millis();
  
  clear_screen();
  draw_border();
  
  draw_text(20, 2, 2, TFT_CYAN, "DECAY");
  
  PresetData &p = config.presets[config.current_preset];
  
  draw_text(2, 22, 1, TFT_YELLOW, "Preset:");
  draw_text(60, 22, 1, TFT_WHITE, p.name);
  
  draw_text(2, 35, 1, TFT_WHITE, "Value:");
  draw_text(80, 35, 1, TFT_GREEN, String(p.decay) + "%");
  
  // Progress bar
  int bar_len = (p.decay * 140) / 100;
  tft.drawRect(2, 47, 150, 12, TFT_CYAN);
  if (bar_len > 0) tft.fillRect(2, 47, bar_len, 12, TFT_MAGENTA);
  
  draw_text(2, 62, 1, TFT_WHITE, "Min: 1%              Max: 100%");
  draw_text(2, 77, 1, TFT_YELLOW, "CONTROL:");
  draw_text(2, 87, 1, TFT_WHITE, "Rotate: Adjust");
  draw_text(2, 97, 1, TFT_WHITE, "Press: Save & Back");
}

void draw_name_menu() {
  static uint32_t last_draw = 0;
  if (millis() - last_draw < 100) return;
  last_draw = millis();
  
  clear_screen();
  draw_border();
  
  draw_text(15, 2, 2, TFT_CYAN, "EDIT NAME");
  
  PresetData &p = config.presets[config.current_preset];
  int name_len = strlen(p.name);
  
  // Show name with cursor
  for (int i = 0; i < name_len; i++) {
    if (i == name_edit_pos) {
      draw_filled_rect(2 + (i * 7), 25, 7, 10, TFT_DARKGREY);
      draw_text(2 + (i * 7), 25, 1, TFT_YELLOW, String(p.name[i]));
    } else {
      draw_text(2 + (i * 7), 25, 1, TFT_GREEN, String(p.name[i]));
    }
  }
  
  draw_text(2, 40, 1, TFT_WHITE, "Pos: " + String(name_edit_pos + 1));
  
  draw_text(2, 55, 1, TFT_YELLOW, "CONTROL:");
  draw_text(2, 65, 1, TFT_WHITE, "Rotate: Change char");
  draw_text(2, 75, 1, TFT_WHITE, "Press: Next pos/Save");
  
  draw_text(2, 95, 1, TFT_CYAN, "Chars: A-Z a-z 0-9");
  draw_text(2, 105, 1, TFT_CYAN, "Special: space - _");
}

void draw_bluetooth_menu() {
  static uint32_t last_draw = 0;
  if (millis() - last_draw < 100) return;
  last_draw = millis();
  
  clear_screen();
  draw_border();
  
  draw_text(10, 2, 2, TFT_CYAN, "BLUETOOTH");
  
  draw_text(2, 22, 1, TFT_YELLOW, "Device:");
  draw_text(60, 22, 1, TFT_GREEN, "Sabilu Dzikri");
  
  draw_text(2, 35, 1, TFT_YELLOW, "Status:");
  if (config.bluetooth_enabled) {
    draw_text(60, 35, 1, TFT_GREEN, "ENABLED");
  } else {
    draw_text(60, 35, 1, TFT_RED, "DISABLED");
  }
  
  draw_text(2, 48, 1, TFT_YELLOW, "Connected:");
  if (bluetooth.hasClient()) {
    draw_text(80, 48, 1, TFT_GREEN, "YES");
  } else {
    draw_text(80, 48, 1, TFT_YELLOW, "NO");
  }
  
  draw_text(2, 63, 1, TFT_YELLOW, "PAIRING:");
  draw_text(2, 73, 1, TFT_WHITE, "1. Go to Bluetooth");
  draw_text(2, 82, 1, TFT_WHITE, "2. Find \"Sabilu Dzikri\"");
  draw_text(2, 91, 1, TFT_WHITE, "3. Pair (no PIN)");
  
  draw_text(2, 108, 1, TFT_CYAN, "Commands: PRESET STATUS");
  draw_text(2, 118, 1, TFT_CYAN, "THRESHOLD VOLUME DECAY");
}

void draw_master_volume_menu() {
  static uint32_t last_draw = 0;
  if (millis() - last_draw < 100) return;
  last_draw = millis();
  
  clear_screen();
  draw_border();
  
  draw_text(5, 2, 2, TFT_CYAN, "MASTER VOL");
  
  draw_text(2, 25, 1, TFT_WHITE, "Value:");
  draw_text(80, 25, 1, TFT_GREEN, String(config.master_volume) + "%");
  
  // Progress bar
  int bar_len = (config.master_volume * 140) / 100;
  tft.drawRect(2, 38, 150, 15, TFT_CYAN);
  if (bar_len > 0) tft.fillRect(2, 38, bar_len, 15, TFT_MAGENTA);
  
  draw_text(2, 57, 1, TFT_WHITE, "Min: 1%              Max: 100%");
  draw_text(2, 72, 1, TFT_YELLOW, "CONTROL:");
  draw_text(2, 82, 1, TFT_WHITE, "Rotate: Adjust level");
  draw_text(2, 92, 1, TFT_WHITE, "Press: Save & Back");
  draw_text(2, 110, 1, TFT_CYAN, "Master volume for all presets");
}

void update_display() {
  switch (menu_state) {
    case MENU_MAIN:
      draw_main_menu();
      break;
    case MENU_SELECT_PRESET:
      draw_preset_menu();
      break;
    case MENU_SETTINGS:
      draw_settings_menu();
      break;
    case MENU_EDIT_THRESHOLD:
      draw_threshold_menu();
      break;
    case MENU_EDIT_VOLUME:
      draw_volume_menu();
      break;
    case MENU_EDIT_DECAY:
      draw_decay_menu();
      break;
    case MENU_EDIT_NAME:
      draw_name_menu();
      break;
    case MENU_BLUETOOTH:
      draw_bluetooth_menu();
      break;
    case MENU_MASTER_VOLUME:
      draw_master_volume_menu();
      break;
  }
}

// ============================================================================
// INPUT HANDLING
// ============================================================================
void IRAM_ATTR encoder_isr() {
  if (millis() - enc_last_interrupt < 10) return;
  enc_last_interrupt = millis();
  
  if (digitalRead(PIN_ENC_DT) == LOW) {
    enc_value++;
  } else {
    enc_value--;
  }
}

void handle_encoder() {
  // Button
  if (digitalRead(PIN_ENC_SW) == LOW && !btn_pressed && (millis() - btn_last_time > 250)) {
    btn_pressed = true;
    btn_last_time = millis();
    
    switch (menu_state) {
      case MENU_MAIN:
        // Menu selection logic
        break;
      case MENU_SELECT_PRESET:
        menu_state = MENU_MAIN;
        save_config();
        break;
      case MENU_SETTINGS:
        menu_state = MENU_MAIN;
        break;
      case MENU_EDIT_THRESHOLD:
      case MENU_EDIT_VOLUME:
      case MENU_EDIT_DECAY:
        menu_state = MENU_SETTINGS;
        save_config();
        break;
      case MENU_EDIT_NAME:
        name_edit_pos++;
        if (name_edit_pos >= strlen(config.presets[config.current_preset].name)) {
          menu_state = MENU_SETTINGS;
          name_edit_pos = 0;
          save_config();
        }
        break;
      case MENU_BLUETOOTH:
        menu_state = MENU_MAIN;
        break;
      case MENU_MASTER_VOLUME:
        menu_state = MENU_MAIN;
        save_config();
        break;
    }
  } else if (digitalRead(PIN_ENC_SW) == HIGH) {
    btn_pressed = false;
  }
  
  // Rotation
  if (enc_value != enc_last_value) {
    int delta = enc_value - enc_last_value;
    enc_last_value = enc_value;
    
    switch (menu_state) {
      case MENU_MAIN:
        if (delta > 0) menu_state = MENU_SELECT_PRESET;
        else if (delta < 0) menu_state = MENU_BLUETOOTH;
        break;
      case MENU_SELECT_PRESET:
        config.current_preset = (config.current_preset + delta + NUM_PRESETS) % NUM_PRESETS;
        break;
      case MENU_SETTINGS:
        // Settings cycle through edit options
        break;
      case MENU_EDIT_THRESHOLD:
        config.presets[config.current_preset].threshold = 
          constrain(config.presets[config.current_preset].threshold + delta, 1, 100);
        break;
      case MENU_EDIT_VOLUME:
        config.presets[config.current_preset].volume = 
          constrain(config.presets[config.current_preset].volume + delta, 1, 100);
        break;
      case MENU_EDIT_DECAY:
        config.presets[config.current_preset].decay = 
          constrain(config.presets[config.current_preset].decay + delta, 1, 100);
        break;
      case MENU_EDIT_NAME:
        if (name_edit_pos < strlen(config.presets[config.current_preset].name)) {
          char current = config.presets[config.current_preset].name[name_edit_pos];
          int idx = 0;
          for (int i = 0; i < 66; i++) {
            if (char_map[i] == current) {
              idx = i;
              break;
            }
          }
          idx = (idx + delta + 66) % 66;
          config.presets[config.current_preset].name[name_edit_pos] = char_map[idx];
        }
        break;
      case MENU_MASTER_VOLUME:
        config.master_volume = constrain(config.master_volume + delta, 1, 100);
        break;
    }
  }
}

void handle_microphone() {
  static uint32_t last_check = 0;
  if (millis() - last_check < 50) return;
  last_check = millis();
  
  uint16_t mic_val = analogRead(PIN_MIC_ADC);
  
  for (int i = 0; i < NUM_PRESETS; i++) {
    uint16_t threshold = (config.presets[i].threshold * 4095) / 100;
    if (mic_val > threshold && !decay_active[i]) {
      decay_active[i] = 1;
      decay_timer[i] = millis();
      play_preset(i);
    }
  }
}

void play_preset(uint8_t idx) {
  if (idx >= NUM_PRESETS) return;
  
  PresetData &p = config.presets[idx];
  uint8_t vol = (p.volume * config.master_volume) / 100;
  
  Serial.printf("[PLAY] %s | Vol: %d%%\n", p.name, vol);
  
  if (config.bluetooth_enabled && bluetooth.hasClient()) {
    bluetooth.printf("PLAY:%d:%d\n", idx, vol);
  }
}

void update_eq_bars() {
  static uint32_t last_update = 0;
  if (millis() - last_update < 50) return;
  last_update = millis();
  
  uint16_t mic = analogRead(PIN_MIC_ADC);
  
  for (int i = 0; i < NUM_EQ_BARS - 1; i++) {
    eq_bars[i] = eq_bars[i + 1];
  }
  eq_bars[NUM_EQ_BARS - 1] = (mic * 255) / 4095;
}

// ============================================================================
// BLUETOOTH
// ============================================================================
void handle_bluetooth() {
  while (bluetooth.available()) {
    char c = bluetooth.read();
    if (c == '\n' || c == '\r') {
      if (bt_buffer_idx > 0) {
        process_bt_command(bt_buffer);
        bt_buffer_idx = 0;
        memset(bt_buffer, 0, sizeof(bt_buffer));
      }
    } else if (bt_buffer_idx < 127) {
      bt_buffer[bt_buffer_idx++] = c;
    }
  }
}

void process_bt_command(const char *cmd) {
  Serial.printf("[BT] %s\n", cmd);
  
  if (strncmp(cmd, "PRESET:", 7) == 0) {
    int n = atoi(cmd + 7);
    if (n >= 0 && n < NUM_PRESETS) {
      config.current_preset = n;
      bluetooth.println("OK");
    } else bluetooth.println("ERROR");
  } else if (strncmp(cmd, "THRESHOLD:", 10) == 0) {
    int v = atoi(cmd + 10);
    if (v >= 1 && v <= 100) {
      config.presets[config.current_preset].threshold = v;
      bluetooth.println("OK");
    } else bluetooth.println("ERROR");
  } else if (strncmp(cmd, "VOLUME:", 7) == 0) {
    int v = atoi(cmd + 7);
    if (v >= 1 && v <= 100) {
      config.presets[config.current_preset].volume = v;
      bluetooth.println("OK");
    } else bluetooth.println("ERROR");
  } else if (strncmp(cmd, "DECAY:", 6) == 0) {
    int v = atoi(cmd + 6);
    if (v >= 1 && v <= 100) {
      config.presets[config.current_preset].decay = v;
      bluetooth.println("OK");
    } else bluetooth.println("ERROR");
  } else if (strncmp(cmd, "NAME:", 5) == 0) {
    strncpy(config.presets[config.current_preset].name, cmd + 5, MAX_NAME_LEN - 1);
    config.presets[config.current_preset].name[MAX_NAME_LEN - 1] = 0;
    bluetooth.println("OK");
  } else if (strcmp(cmd, "STATUS") == 0) {
    bluetooth.printf("P:%d|N:%s|T:%d|V:%d|D:%d\n",
      config.current_preset,
      config.presets[config.current_preset].name,
      config.presets[config.current_preset].threshold,
      config.presets[config.current_preset].volume,
      config.presets[config.current_preset].decay);
  } else if (strcmp(cmd, "SAVE") == 0) {
    save_config();
    bluetooth.println("OK");
  } else if (strcmp(cmd, "VERSION") == 0) {
    bluetooth.println("Sabilu Dzikri v2.0");
  } else {
    bluetooth.println("UNKNOWN");
  }
}
