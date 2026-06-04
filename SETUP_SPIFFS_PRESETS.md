# SABILU DZIKRI - ESP32 SOUNDBOARD v2.0
## Dengan Support 10 Audio Preset dari SPIFFS

### ⚙️ SETUP SPIFFS - UPLOAD PRESET AUDIO

#### Langkah 1: Persiapkan File Audio
1. Ambil 10 file audio dari folder `sketch_jun2a/data/`
   - File harus bernama: `preset_1.wav`, `preset_2.wav`, ... `preset_10.wav`
   - Format: WAV (PCM, 16-bit, 44100Hz atau lebih rendah)
   - Ukuran: Kurang dari 1MB per file (agar muat di SPIFFS)

#### Langkah 2: Upload Preset ke SPIFFS

**METODE A: Menggunakan Arduino IDE (Recommended)**

1. Download SPIFFS Uploader Plugin:
   - https://github.com/me-no-dev/arduino-esp32fs-plugin
   
2. Ekstrak ke folder:
   - **Windows**: `Documents\Arduino\tools\ESP32FS\tool`
   - **Mac**: `~/Documents/Arduino/tools/ESP32FS/tool`
   - **Linux**: `~/Arduino/tools/ESP32FS/tool`

3. Restart Arduino IDE

4. Buat folder `data` di folder sketch:
   ```
   sketch_jun2a/
   ├── sabilu_dzikri_soundboard_v2.ino
   └── data/
       ├── preset_1.wav
       ├── preset_2.wav
       ├── preset_3.wav
       ├── preset_4.wav
       ├── preset_5.wav
       ├── preset_6.wav
       ├── preset_7.wav
       ├── preset_8.wav
       ├── preset_9.wav
       └── preset_10.wav
   ```

5. Buka file `sabilu_dzikri_soundboard_v2.ino`

6. Klik **Tools** → **ESP32 Sketch Data Upload**
   - Tunggu hingga selesai (lihat Serial Monitor: "SPIFFS uploaded")

7. Jika sukses, buka Serial Monitor (115200 baud)
   - Akan tampil:
   ```
   [*] Files in SPIFFS:
       /preset_1.wav - 123456 bytes
       /preset_2.wav - 234567 bytes
       ...
       /preset_10.wav - 456789 bytes
   [OK] Total files: 10
   ```

**METODE B: Menggunakan Web Upload (Jika Plugin Tidak Berfungsi)**

1. Ganti `#define` di sketch menjadi:
   ```cpp
   #define ENABLE_WEB_UPLOAD 1
   ```

2. Upload sketch biasa ke ESP32

3. ESP32 akan membuat WiFi AP "Sabilu-Upload"

4. Koneksi ke AP tersebut, buka browser ke `192.168.4.1`

5. Upload file audio satu per satu

#### Langkah 3: Verifikasi Upload

Buka Serial Monitor dan lihat output startup:
```
[OK] SPIFFS Mounted Successfully
[*] Files in SPIFFS:
    /preset_1.wav - 123456 bytes
    /preset_2.wav - 234567 bytes
    [OK] Total files: 10
```

### 🎮 FITUR UTAMA

✅ **10 Preset Audio** - Tersimpan di SPIFFS Flash Memory
✅ **Menu Lengkap:**
   - Select Preset (1-10)
   - Edit Threshold/Sensivity (1-100%)
   - Edit Volume Output (1-100%)
   - Edit Decay/Release (1-100%)
   
✅ **Visual Feedback:**
   - SPI Display 128x160
   - 16-bar EQ real-time
   - Progress bars untuk editing
   - Highlight selection

✅ **Bluetooth Control:**
   ```
   PRESET:0           // Select preset (0-9)
   THRESHOLD:50       // Set sensitivity
   VOLUME:80          // Set volume
   DECAY:50           // Set decay
   STATUS             // Get current status
   SAVE               // Save to flash
   REBOOT             // Restart ESP32
   VERSION            // Get version info
   LISTFILES          // List SPIFFS files
   ```

✅ **Encoder Control:**
   - Rotate untuk navigate
   - Press untuk select
   - Intuitive menu system

### 📍 PIN CONNECTIONS

**Display (SPI TFT 128x160):**
```
GPIO 5  → CS  (Chip Select)
GPIO 2  → DC  (Data/Command)
GPIO 4  → RST (Reset)
GPIO 23 → MOSI (SPI Data)
GPIO 18 → SCK (SPI Clock)
3.3V    → VCC
GND     → GND
```

**Encoder:**
```
GPIO 19 → CLK (Clock)
GPIO 17 → DT  (Data)
GPIO 21 → SW  (Button)
3.3V    → +
GND     → GND
```

**Microphone:**
```
GPIO 34 → AO (Analog Output)
3.3V    → VCC
GND     → GND
```

**Audio Output (Optional):**
```
GPIO 26 → BCK  (Bit Clock)
GPIO 25 → LRCK (Left/Right Clock)
GPIO 22 → DIN  (Data In)
3.3V    → VCC
GND     → GND
```

### 🔧 UPLOAD SKETCH KE ESP32

1. Buka `sabilu_dzikri_soundboard_v2.ino` di Arduino IDE
2. **Tools** → **Board** → Pilih **ESP32 Dev Module**
3. **Tools** → **Port** → Pilih COM port ESP32
4. **Tools** → **Upload Speed** → **115200**
5. Klik tombol **Upload** ⬆️
6. Tunggu selesai

### 📊 TROUBLESHOOTING

**Display tidak tampil:**
- Cek pin SPI connections
- Verifikasi TFT_eSPI library terinstall
- Test dengan sketch contoh TFT_eSPI

**File preset tidak terupload:**
- Pastikan folder `data/` ada di folder sketch
- Cek nama file: `preset_1.wav`, `preset_2.wav`, dst
- Ukuran file tidak boleh >1MB per file
- Gunakan METODE B (Web Upload) jika plugin bermasalah

**Preset tidak terdeteksi saat main:**
- Buka Serial Monitor (115200 baud)
- Lihat list file di startup
- Jika tidak ada, upload lagi dengan Tools → ESP32 Sketch Data Upload

**Encoder tidak merespons:**
- Cek GPIO pins (19, 17, 21)
- Verifikasi koneksi 3.3V (bukan 5V!)
- Test dengan sketch digitalRead() sederhana

**Bluetooth tidak connect:**
- Cari device "Sabilu Dzikri" di phone Bluetooth settings
- Pastikan SerialBT.begin() sukses (lihat Serial Monitor)
- Pair device terlebih dahulu

### 📝 NOTES

- Preset settings tersimpan di NVS (Non-Volatile Storage)
- Audio files tersimpan di SPIFFS (Flash File System)
- Semua perubahan otomatis tersimpan
- Bisa di-edit via encoder atau via Bluetooth
- Zero latency untuk audio triggering dari microphone

### 🚀 NEXT STEPS

1. ✅ Upload preset audio files ke SPIFFS
2. ✅ Test semua menu berfungsi
3. ✅ Calibrate microphone sensitivity
4. ✅ Custom preset names via encoder
5. ✅ Control via Bluetooth dari phone

---
**Version:** 1.0 with 10 Audio Presets  
**Board:** ESP32 Dev Module  
**Display:** 128x160 SPI TFT (ST7735)  
**Status:** Ready for Production ✓
