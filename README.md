# 💧 ESP-NOW Water Level Monitoring System  
### Wireless Tank Level Indicator using ESP-12F (Sender) and NodeMCU (Receiver)

---

## 📖 Overview

This project implements a **wireless water-level monitoring system** using two ESP8266 boards communicating via **ESP-NOW** protocol.  
It measures and displays the water tank level, triggers alarms for high/low levels, and requires **no Wi-Fi connection**.

- **ESP-12F (Sender)** — Reads water level using AJ-SR04M waterproof ultrasonic sensor.  
- **NodeMCU (Receiver)** — Displays level on LCD, activates LEDs & buzzer, and stores settings in EEPROM.

---

## ⚙️ Features

✅ ESP-NOW wireless communication (no router needed)  
✅ Real-time LCD display  
✅ High/Low water level alarms  
✅ EEPROM memory for saved thresholds  
✅ MAC address pairing for security  
✅ Data averaging filter for smooth readings  
✅ Optional relay for motor/pump control  

---

## 📁 Folder Structure

| Folder | Description |
|--------|--------------|
| `ESP-12F_SENDER/` | Code for ESP-12F Sender Node (with Blynk integration) |
| `Arduino_WebApp_Sender/` | Code for ESP8266 Sender with Supabase web integration |
| `Nodemcu_Receiver/` | Code for NodeMCU Receiver Node |
| `app/` | React web dashboard for monitoring water levels |
| `supabase/` | Supabase backend configuration and functions |
| `README.md` | Documentation (this file) |

---

## 🔧 Hardware Required

| Component | Description |
|------------|-------------|
| ESP-12F | Sender board |
| NodeMCU ESP8266 | Receiver board |
| AJ-SR04M Ultrasonic Sensor | Waterproof distance measurement |
| 16x2 I²C LCD | Displays water level |
| Buzzer | Sound alert |
| LEDs | High/Low water indicators |
| EEPROM | Stores configuration |

---

## 🪛 Pin Configuration

### 🔹 ESP-12F (Sender)
| Function | Pin |
|-----------|-----|
| Trigger (AJ-SR04M) | D1 (GPIO5) |
| Echo (AJ-SR04M) | D2 (GPIO4) |
| Status LED | D0 (GPIO16) |

### 🔹 NodeMCU (Receiver)
| Function | Pin |
|-----------|-----|
| LCD SDA | D2 (GPIO4) |
| LCD SCL | D1 (GPIO5) |
| High Level LED | D6 (GPIO12) |
| Low Level LED | D7 (GPIO13) |
| Buzzer | D5 (GPIO14) |

---

## 💧 Water Level Formula

Water Level (%) = ((Tank Height - Distance) / Tank Height) * 100

Example:  
Tank Height = 127 cm  
Measured Distance = 25 cm  
→ Water Level = 80.3%

---

## ⚠️ Alarm Logic

| Condition | Water Level | Action |
|------------|--------------|--------|
| Low Level | < 20% | Red LED + Buzzer ON |
| Normal | 20–95% | Green LED ON |
| High Level | > 95% | Green LED Blink + Buzzer Beep |

---

## 📡 ESP-NOW Communication

- **Sender** transmits readings periodically via ESP-NOW.  
- **Receiver** accepts packets only from a paired MAC address.  
- Works without Wi-Fi or internet.

---

## 🧰 Installation

### 🔐 Configuration (IMPORTANT - First Time Setup)

Before uploading the code to your devices, you **MUST** configure the following credentials:

#### For NodeMCU Receiver (`Nodemcu_Receiver/Nodemcu_Receiver.ino`)
1. **WiFi Credentials** (lines 35-37)
   - Replace `YOUR_WIFI_SSID_HERE` with your WiFi network name
   - Replace `YOUR_WIFI_PASSWORD_HERE` with your WiFi password
   
2. **Sender MAC Address** (line 41)
   - Replace `{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}` with your ESP-12F sender's MAC address
   - You can find the MAC address by uploading the sender code first and checking Serial Monitor
   
3. **OTA Password** (line 46)
   - Replace `YOUR_OTA_PASSWORD_HERE` with a secure password for Over-The-Air updates

#### For ESP-12F Sender (`ESP-12F_SENDER/ESP-12F_SENDER.ino`)
1. **WiFi Credentials** (lines 27-32)
   - Replace `YOUR_WIFI_SSID_HERE` with your WiFi network name
   - Replace `YOUR_WIFI_PASSWORD_HERE` with your WiFi password
   
2. **Blynk Credentials** (lines 15-24)
   - Get your credentials from [Blynk.Cloud](https://blynk.cloud)
   - Replace `YOUR_BLYNK_TEMPLATE_ID` with your template ID
   - Replace `YOUR_BLYNK_TEMPLATE_NAME` with your template name
   - Replace `YOUR_BLYNK_AUTH_TOKEN` with your auth token
   
3. **Receiver MAC Address** (line 49)
   - Replace `{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}` with your NodeMCU receiver's MAC address
   
4. **OTA Password** (line 99)
   - Replace `YOUR_OTA_PASSWORD_HERE` with a secure password

#### For Web App Sender (`Arduino_WebApp_Sender/Arduino_WebApp_Sender.ino`)
1. **WiFi Credentials** (lines 12-13)
   - Replace `YOUR_WIFI_SSID_HERE` with your WiFi network name
   - Replace `YOUR_WIFI_PASSWORD_HERE` with your WiFi password
   
2. **Supabase Credentials** (lines 26-27)
   - Create a free account at [Supabase](https://supabase.com)
   - Replace `YOUR_SUPABASE_URL_HERE` with your Supabase project URL
   - Replace `YOUR_SUPABASE_ANON_KEY_HERE` with your Supabase anonymous key
   - Find these at: `https://supabase.com/dashboard/project/_/settings/api`

#### For Web Dashboard (`app/`)
1. **Copy the environment template**
   ```bash
   cd app
   cp .env.example .env
   ```
   
2. **Edit `.env` file** with your Supabase credentials
   - Replace `your-project-id` in the URL
   - Replace `your-supabase-anon-key-here` with your actual key

⚠️ **Security Note**: Never commit the `.env` file or files with real credentials to Git!

---

### 🔧 Required Libraries
Install these in Arduino IDE:
- `ESP8266WiFi.h`
- `espnow.h`
- `LiquidCrystal_I2C.h`
- `EEPROM.h`

### 🖥️ Board Settings
| Setting | Value |
|----------|--------|
| Board | NodeMCU 1.0 / Generic ESP8266 |
| Flash Size | 4MB |
| Upload Speed | 115200 |
| Flash Mode | DOUT or QIO |

### 🚀 Upload
1. Open `ESP-12F_SENDER/ESP-12F_SENDER.ino` → Upload to ESP-12F  
2. Open `Nodemcu_Receiver/Nodemcu_Receiver.ino` → Upload to NodeMCU  
3. Power both boards and enjoy wireless water-level monitoring 🎉

---

## 🧠 Future Upgrades

- 🌤 Add DHT11 (temperature/humidity)  
- ☁️ Integrate with Blynk or MQTT  
- 🗣 Google Assistant voice control  
- 🔋 Deep-sleep battery optimization  
- 📲 Mobile app dashboard  

---

## 👨‍💻 Developer

**Madeshwaran M**  
💡 IoT Developer | Embedded Systems Enthusiast  
📍 India  

---

## 🪪 License

This project is released under the **MIT License** —  
You can freely use, modify, and distribute with credit.

---

## ⭐ Support

If you like this project:
- Give it a ⭐ on GitHub  
- Share it with your maker friends  
- Suggest new features or improvements 🚀

