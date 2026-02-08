/*************************************************************
  NodeMCU ESP-NOW Receiver (LCD, Alarms, EEPROM)
  *** LEGACY VERSION for old 2.5.0 board package ***

  Receives data from the "nodemcu_water_monitor_LEGACY.ino" sender.

  Features:
  - 16x2 I2C LCD Display
  - High/Low level alarms (LEDs + Buzzer)
  - MAC Address filtering for security
  - Data averaging to prevent jitter
  - Watchdog timer for reliability
  - EEPROM for saving alarm settings
  - OTA (Over-the-Air) Update support

  Hardware:
  - NodeMCU ESP8266
  - 16x2 I2C LCD Display (SDA=D2, SCL=D1)
  - Green LED (High Alarm) on D5
  - Red LED (Low Alarm) on D6
  - Passive Buzzer on D7
*************************************************************/

#include <ESP8266WiFi.h>
#include <espnow.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Ticker.h>
#include <ESP8266mDNS.h>  // Added for OTA
#include <WiFiUdp.h>      // Added for OTA
#include <ArduinoOTA.h>   // Added for OTA

// --- WiFi Credentials (MUST BE FILLED IN FOR OTA) ---
// TODO: Replace with your WiFi credentials before uploading
#define WIFI_SSID "YOUR_WIFI_SSID_HERE"
#define WIFI_PASS "YOUR_WIFI_PASSWORD_HERE"

// --- ESP-NOW Configuration ---
// TODO: Replace with your sender's MAC address (check sender device MAC)
// This MAC Address MUST match the sender's MAC address
uint8_t senderMacAddress[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
#define WIFI_CHANNEL 1

// --- OTA Configuration ---
#define OTA_HOSTNAME "NodeMCU-WaterReceiver"
// TODO: Replace with your own OTA password for secure updates
#define OTA_PASSWORD "YOUR_OTA_PASSWORD_HERE"

// --- Hardware Pins ---
#define GREEN_LED_PIN D5 // GPIO14
#define RED_LED_PIN   D6 // GPIO12
#define BUZZER_PIN    D7 // GPIO13

// --- I2C LCD Setup ---
// Adjust 0x27 and 16, 2 if your display is different
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Alarm Configuration ---
#define ALARM_BLINK_INTERVAL 500 // Blink/Beep every 500ms
#define DATA_TIMEOUT 30000UL     // 30 seconds before "OFFLINE"
#define WATCHDOG_TIMEOUT 30      // 30 seconds

// --- EEPROM Configuration ---
#define EEPROM_MAGIC 0xFEEDC0DE
struct AlarmSettings {
  uint32_t magic;
  float high;
  float low;
};
#define EEPROM_SETTINGS_ADDR 0
#define EEPROM_SIZE sizeof(AlarmSettings)

// --- Data Averaging ---
#define AVG_BUFFER_SIZE 5
float levelHistory[AVG_BUFFER_SIZE];
int historyIndex = 0;

// --- Global State Variables ---
float highAlarmThreshold = 90.0;
float lowAlarmThreshold = 20.0;
float currentWaterLevel = -1.0;
bool highAlarmActive = false;
bool lowAlarmActive = false;
bool senderOffline = true;
bool alarmBlinkerState = false;
unsigned long lastDataPacket = 0;
String lastLcdLine0 = "";
String lastLcdLine1 = "";

// --- Watchdog ---
Ticker watchdog;
void resetModule() {
  Serial.println("Watchdog triggered! Restarting...");
  ESP.restart();
}

// --- Data Structure (MUST match sender) ---
typedef struct {
  float waterLevel;
  float distance;
  unsigned long timestamp;
} SensorData;

// --- Function Declarations ---
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len); // LEGACY signature
void loadAlarmSettings();
void saveAlarmSettings(float high, float low);
void checkAlarms(float level);
void manageAlarms();
void updateLCD();
void updateLCDLine(int row, String newText);
float getAverageLevel(float newReading);
void setupOTA();

// =================================================================
//  SETUP
// =================================================================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n=== NodeMCU ESP-NOW Receiver (Legacy Mode) ===");

  // Setup watchdog
  watchdog.attach(WATCHDOG_TIMEOUT, resetModule);

  // Setup Hardware Pins
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, HIGH); // Turn off (active low)
  digitalWrite(RED_LED_PIN, HIGH);   // Turn off (active low)
  noTone(BUZZER_PIN);

  // Initialize LCD
  Wire.begin(D2, D1); // SDA=D2, SCL=D1
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Initializing...");
  Serial.println("LCD Initialized.");

  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  loadAlarmSettings();

  // Show startup info on LCD
  lcd.clear();
  lcd.print("Water Monitor v1.0");
  lcd.setCursor(0, 1);
  lcd.print("H:" + String(highAlarmThreshold, 0) + "% L:" + String(lowAlarmThreshold, 0) + "%");
  Serial.println("Startup screen displayed.");
  delay(2500);

  // Initialize moving average buffer
  for (int i = 0; i < AVG_BUFFER_SIZE; i++) {
    levelHistory[i] = -1.0;
  }

  // Initialize WiFi
  Serial.print("Setting WiFi mode STA...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS); // Connect to WiFi for OTA
  
  // Set WiFi channel for ESP-NOW
  WiFi.channel(WIFI_CHANNEL);
  Serial.print("\nESP-NOW configured for WiFi Channel: ");
  Serial.print(WIFI_CHANNEL);
  Serial.print(" (Actual: ");
  Serial.print(WiFi.channel());
  Serial.println(")");

  Serial.println("Initializing ESP-NOW...");
  int retries = 3;
  while (esp_now_init() != 0 && retries-- > 0) {
    Serial.println("ESP-NOW init failed, retrying...");
    delay(1000);
  }

  if (retries <= 0) {
    Serial.println("ESP-NOW FATAL ERROR!");
    updateLCDLine(0, "ESP-NOW FAILED!");
    updateLCDLine(1, "Please Reset");
    while (true) delay(100);
  }

  Serial.println("ESP-NOW Initialized.");
  
  // ** LEGACY FIX **
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);
  
  // Add peer for reliability
  Serial.println("Registering sender as peer (Legacy Mode)...");
  if (esp_now_add_peer(senderMacAddress, ESP_NOW_ROLE_CONTROLLER, WIFI_CHANNEL, NULL, 0) == 0) {
    Serial.println("Peer added successfully.");
  } else {
    Serial.println("Failed to add peer.");
  }

  // --- Start OTA After ESP-NOW ---
  Serial.print("Connecting to WiFi for OTA...");
  int wifi_retries = 20;
  while (WiFi.status() != WL_CONNECTED && wifi_retries-- > 0) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected for OTA!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    setupOTA(); // Initialize OTA
  } else {
    Serial.println("\nWiFi Connection Failed. OTA will not be available.");
    // Note: ESP-NOW will still work!
  }

  Serial.println("Waiting for data...");
  updateLCDLine(0, "Waiting for Data");
  updateLCDLine(1, "Status: OFFLINE");
  lcd.backlight(); // Keep light on
}

// =================================================================
//  MAIN LOOP
// =================================================================
void loop() {
  // Feed the watchdog
  watchdog.detach();
  watchdog.attach(WATCHDOG_TIMEOUT, resetModule);

  // Handle OTA updates if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
  }

  // Check if sender has gone offline
  if (!senderOffline && (millis() - lastDataPacket > DATA_TIMEOUT)) {
    senderOffline = true;
    highAlarmActive = false;
    lowAlarmActive = false;
    currentWaterLevel = -1.0; // Set to invalid
    Serial.println("Sender timed out. STATUS: OFFLINE");
    
    // Clear the average buffer
    for (int i = 0; i < AVG_BUFFER_SIZE; i++) {
      levelHistory[i] = -1.0;
    }
  }

  // Handle blinking alarms
  manageAlarms();

  // Update LCD display
  updateLCD();
}

// =================================================================
//  ESP-NOW DATA RECEIVE CALLBACK
// =================================================================
void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  // 1. Check MAC Address
  if (memcmp(mac, senderMacAddress, 6) != 0) {
    Serial.println("Packet from unknown sender ignored.");
    return;
  }

  // 2. Check data size
  SensorData localData;
  if (len != sizeof(localData)) {
    Serial.println("Packet size mismatch ignored.");
    return;
  }

  // 3. Copy data
  memcpy(&localData, incomingData, sizeof(localData));

  // 4. Check if this is the first packet
  static bool firstPacket = true;
  if (senderOffline || firstPacket) {
    Serial.println("Sender (re)connected!");
    updateLCDLine(0, "Sender Connected");
    delay(1000); // Show message briefly
    firstPacket = false;
  }

  // 5. Update state
  senderOffline = false;
  lastDataPacket = millis();
  
  // 6. Apply filter and check alarms
  currentWaterLevel = getAverageLevel(localData.waterLevel);
  checkAlarms(currentWaterLevel);
}

// =================================================================
//  CORE FUNCTIONS
// =================================================================

float getAverageLevel(float newReading) {
  // Add new reading to buffer
  levelHistory[historyIndex] = newReading;
  historyIndex = (historyIndex + 1) % AVG_BUFFER_SIZE;

  // Calculate average of valid readings in buffer
  float sum = 0;
  int count = 0;
  for (int i = 0; i < AVG_BUFFER_SIZE; i++) {
    if (levelHistory[i] >= 0) {
      sum += levelHistory[i];
      count++;
    }
  }

  if (count == 0) return -1.0;
  return sum / count;
}

void checkAlarms(float level) {
  if (level < 0) return; // Do nothing if level is invalid

  if (level >= highAlarmThreshold) {
    highAlarmActive = true;
    lowAlarmActive = false;
  } else if (level <= lowAlarmThreshold) {
    highAlarmActive = false;
    lowAlarmActive = true;
  } else {
    highAlarmActive = false;
    lowAlarmActive = false;
  }
}

void manageAlarms() {
  static unsigned long lastAlarmToggle = 0;

  // This function controls the blinking/beeping
  if (millis() - lastAlarmToggle > ALARM_BLINK_INTERVAL) {
    alarmBlinkerState = !alarmBlinkerState;
    lastAlarmToggle = millis();
  }

  // --- Green LED (High Alarm) ---
  if (highAlarmActive) {
    digitalWrite(GREEN_LED_PIN, alarmBlinkerState ? LOW : HIGH); // Blink
    if (alarmBlinkerState) {
      tone(BUZZER_PIN, 2000); // High beep
    } else {
      noTone(BUZZER_PIN);
    }
  } else {
    digitalWrite(GREEN_LED_PIN, HIGH); // Force OFF
  }

  // --- Red LED (Low Alarm) ---
  if (lowAlarmActive) {
    digitalWrite(RED_LED_PIN, alarmBlinkerState ? LOW : HIGH); // Blink
    if (alarmBlinkerState) {
      tone(BUZZER_PIN, 800); // Low beep
    } else {
      noTone(BUZZER_PIN);
    }
  } else {
    digitalWrite(RED_LED_PIN, HIGH); // Force OFF
  }
  
  // --- No Alarm State ---
  if (!highAlarmActive && !lowAlarmActive) {
    noTone(BUZZER_PIN); // Ensure buzzer is off
  }
}

void updateLCD() {
  String line0, line1;

  if (senderOffline) {
    lcd.backlight(); // Turn on light for warning
    line0 = "Water Level: --";
    line1 = "Status: OFFLINE";
  } else if (highAlarmActive) {
    lcd.backlight(); // Turn on light for alarm
    line0 = "Level: " + String(currentWaterLevel, 1) + "%";
    line1 = "ALARM: HIGH LEVEL";
  } else if (lowAlarmActive) {
    lcd.backlight(); // Turn on light for alarm
    line0 = "Level: " + String(currentWaterLevel, 1) + "%";
    line1 = "ALARM: LOW LEVEL";
  } else {
    lcd.noBacklight(); // Turn off light for normal
    line0 = "Level: " + String(currentWaterLevel, 1) + "%";
    line1 = "Status: NORMAL";
  }

  // Only update the LCD if the text has changed
  updateLCDLine(0, line0);
  updateLCDLine(1, line1);
}

void updateLCDLine(int row, String newText) {
  String* lastLine = (row == 0) ? &lastLcdLine0 : &lastLcdLine1;
  
  if (newText != *lastLine) {
    lcd.setCursor(0, row);
    lcd.print("                "); // Clear line
    lcd.setCursor(0, row);
    lcd.print(newText);
    *lastLine = newText;
  }
}

// =================================================================
//  EEPROM FUNCTIONS
// =================================================================

void loadAlarmSettings() {
  AlarmSettings settings;
  EEPROM.get(EEPROM_SETTINGS_ADDR, settings);

  // Check for "magic number"
  if (settings.magic == EEPROM_MAGIC) 
  {
    highAlarmThreshold = settings.high;
    lowAlarmThreshold = settings.low;
    Serial.println("Loaded alarm settings from EEPROM.");
  } else {
    Serial.println("Invalid EEPROM settings. Saving defaults.");
    // Save defaults
    saveAlarmSettings(highAlarmThreshold, lowAlarmThreshold);
  }
  Serial.print("High Alarm: "); Serial.print(highAlarmThreshold);
  Serial.print("%, Low Alarm: "); Serial.println(lowAlarmThreshold);
}

void saveAlarmSettings(float high, float low) {
  // Check if values actually changed to reduce EEPROM wear
  AlarmSettings oldSettings;
  EEPROM.get(EEPROM_SETTINGS_ADDR, oldSettings);
  
  if (oldSettings.magic != EEPROM_MAGIC || 
      oldSettings.high != high || 
      oldSettings.low != low)
  {
    Serial.println("Saving new settings to EEPROM...");
    AlarmSettings newSettings;
    newSettings.magic = EEPROM_MAGIC;
    newSettings.high = high;
    newSettings.low = low;
    EEPROM.put(EEPROM_SETTINGS_ADDR, newSettings);
    EEPROM.commit();
  }
}

// =================================================================
//  OTA FUNCTIONS
// =================================================================
void setupOTA() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    Serial.println("OTA Update Starting...");
    lcd.clear();
    lcd.print("OTA Update...");
    lcd.setCursor(0, 1);
    lcd.print("Receiving...");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Complete!");
    lcd.clear();
    lcd.print("OTA Complete!");
    lcd.setCursor(0, 1);
    lcd.print("Rebooting...");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    String percent = String(progress / (total / 100));
    updateLCDLine(1, "Progress: " + percent + "%");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    lcd.clear();
    lcd.print("OTA FAILED!");
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

