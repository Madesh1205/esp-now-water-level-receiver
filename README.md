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
| `ESP-12F_SENDER/` | Code for ESP-12F Sender Node |
| `Nodemcu_Receiver/` | Code for NodeMCU Receiver Node |
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

