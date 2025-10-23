/*************************************************************
  NodeMCU Water Level Monitoring System (Paired Sender)
  *** LEGACY VERSION for old 2.5.0 board package ***
 
  Features:
  - AJ-SR04M waterproof ultrasonic sensor (Async)
  - Blynk cloud integration
  - ESP-NOW *unicast* (paired) broadcasting
  - EEPROM backup
  - Adaptive update intervals
  - OTA updates
*************************************************************/

// --- Build-Time Injectable Secrets ---
#ifndef BLYNK_TEMPLATE_ID
#define BLYNK_TEMPLATE_ID "TMPL3kCRser0g"
#endif

#ifndef BLYNK_TEMPLATE_NAME
#define BLYNK_TEMPLATE_NAME "Water Level Sensor"
#endif

#ifndef BLYNK_AUTH_TOKEN
#define BLYNK_AUTH_TOKEN "IsR6N7dR9chxhFR8zMyOpP3cIjQ4i1HJ"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "mohanraj"
#endif

#ifndef WIFI_PASS
#define WIFI_PASS "k70mohanraj"
#endif


#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <espnow.h>
#include <EEPROM.h>

// --- ESP-NOW Receiver's MAC Address ---
uint8_t receiverMacAddress[] = {0x24, 0xD7, 0xEB, 0xF9, 0x28, 0xAE};

// --- ESP-NOW Channel (ensure receiver uses same channel) ---
#define WIFI_CHANNEL 1

// --- Pin Definitions ---
#define TRIG_PIN 5        // GPIO5 (D1)
#define ECHO_PIN 4        // GPIO4 (D2)
#define LED_PIN LED_BUILTIN // GPIO2 (D4)

// --- Tank Dimensions (cm) ---
#define DISTANCE_EMPTY 217.0
#define DISTANCE_FULL 21.0
#define TANK_HEIGHT (DISTANCE_EMPTY - DISTANCE_FULL)
#define MIN_DISTANCE 25.0
#define MAX_DISTANCE 450.0

// --- Blynk Virtual Pins ---
#define VPIN_WATER_LEVEL V1
#define VPIN_DISTANCE V2
#define VPIN_MANUAL_REFRESH V3

// --- Intervals (ms) ---
#define NORMAL_INTERVAL 30000
#define FAST_INTERVAL 5000
#define READING_INTERVAL 2000 // How often to trigger a sensor read
#define ESPNOW_RETRY_INTERVAL 5000 // Retry failed ESP-NOW sends every 5s
#define ESPNOW_RETRY_TIMEOUT 30000UL // Give up retrying after 30s
#define EEPROM_SAVE_INTERVAL 300000UL // Save to EEPROM every 5 mins max

// --- Thresholds ---
#define LOW_LEVEL_THRESHOLD 20.0
#define CHANGE_THRESHOLD 2.0

// --- Sensor settings ---
#define MEDIAN_FILTER_SIZE 5 // Use 5 samples for the moving median
#define ULTRASONIC_TIMEOUT 30000UL // microseconds

// --- EEPROM ---
#define EEPROM_MAGIC 0xABCD1234 // "Magic number" to validate EEPROM data
struct EepromData {
  uint32_t magic;
  float level;
  float distance;
};
#define EEPROM_ADDR 0
#define EEPROM_SIZE sizeof(EepromData)

// --- OTA ---
#define OTA_HOSTNAME "NodeMCU-WaterMonitor"
#define OTA_PASSWORD "admin123"

// Globals
float lastWaterLevel = -1.0;
float currentWaterLevel = 0.0;
float currentDistance = 0.0;
unsigned long lastBlynkUpdate = 0;
unsigned long g_lastEepromSaveTime = 0;

BlynkTimer timer;

typedef struct {
  float waterLevel;
  float distance;
  unsigned long timestamp;
} SensorData;

// ESP-NOW Globals
bool g_espNowRetryPending = false;
SensorData g_lastEspNowPacket;
unsigned long g_espNowPacketStartTime = 0;

// --- Async Ultrasonic ---
volatile unsigned long echoStart = 0;
volatile unsigned long echoEnd = 0;
volatile bool echoReceived = false;
bool trigPending = false;
unsigned long trigStartTime = 0;
float asyncDistance = -1.0;
float distanceHistory[MEDIAN_FILTER_SIZE];
int historyIndex = 0;

// --- Async LED ---
// ** LEGACY FIX **
// Removed in-class initializers for old C++ compiler
struct AsyncLED {
  int pin;
  int state;
  unsigned long lastToggle;
  unsigned long interval;
  int remainingBlinks;
};
// Use C-style initialization
AsyncLED led = {LED_PIN, HIGH, 0, 0, 0};


// Forward declarations
void startUltrasonic();
void handleUltrasonic();
void blinkLED(int times, int blinkInterval);
void handleLED();
float calculateWaterLevel(float distance);
void sendToBlynk(float level, float distance);
void sendESPNOW(float level, float distance);
void saveToEEPROM(float level, float distance);
void loadFromEEPROM();
void sensorReadingTask();
void onDataSent(uint8_t *mac_addr, uint8_t status); // Changed from 2.5.0's old (uint8_t status)
void espNowRetryTask();
float getMedianDistance();

// -----------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n=== NodeMCU Water Monitor (Async Sender - LEGACY) ===");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED off (active low)

  // Initialize median filter history
  for (int i = 0; i < MEDIAN_FILTER_SIZE; i++) {
    distanceHistory[i] = -1.0; // Init with invalid
  }

  // EEPROM
  EEPROM.begin(EEPROM_SIZE);
  loadFromEEPROM();
  g_lastEepromSaveTime = millis(); // Don't save immediately on boot

  // Sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  // WiFi
  Serial.print("Setting WiFi mode STA...");
  WiFi.mode(WIFI_STA);
  delay(100);

  // Connect to WiFi (non-blocking)
  Serial.print("Connecting to WiFi ");
  Serial.print(WIFI_SSID);
  Serial.println(" ...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Configure Blynk (non-blocking)
  Blynk.config(BLYNK_AUTH_TOKEN);

  // Wait a short while for WiFi
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(200); // This blocking delay is OK inside setup()
    Serial.print(".");
  }
  Serial.println();
  
  // --- This logic now runs REGARDLESS of WiFi status ---

  WiFi.channel(WIFI_CHANNEL);
  Serial.print("\nESP-NOW configured for WiFi Channel: ");
  Serial.print(WIFI_CHANNEL);
  Serial.print(" (Actual: ");
  Serial.print(WiFi.channel());
  Serial.println(")");
  Serial.println("NOTE: Ensure receiver is on the same channel!");

  // Initialize ESP-NOW
  if (esp_now_init() == 0) {
    Serial.println("ESP-NOW Initialized");
    
    // ** LEGACY FIX **
    // Use old, deprecated functions for 2.5.0 board package
    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    esp_now_register_send_cb(onDataSent);

    // Use old peer add function (mac, role, channel, key, key_len)
    // This replaces the esp_now_peer_info_t struct
    if (esp_now_add_peer(receiverMacAddress, ESP_NOW_ROLE_SLAVE, WIFI_CHANNEL, NULL, 0) == 0) {
      Serial.println("Paired with receiver successfully (Legacy Mode)");
    } else {
      Serial.println("Failed to add peer - check MAC & channel (Legacy Mode)");
    }
  } else {
    Serial.println("ESP-NOW Init Failed");
  }
  
  // --- End of always-run block ---

  // Setup OTA only if WiFi connected on boot
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    blinkLED(3, 120); // Non-blocking blink

    // OTA
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.onStart([]() { Serial.println("OTA Update Starting..."); });
    ArduinoOTA.onEnd([]() { Serial.println("\nOTA Complete!"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("OTA Error[%u]:\r\n", error);
    });
    ArduinoOTA.begin();
    Serial.println("OTA Ready");
  } else {
    Serial.println("WiFi not connected after wait. Blynk/OTA will try in loop.");
  }

  // Timer
  timer.setInterval(ESPNOW_RETRY_INTERVAL, espNowRetryTask); // Timer for retries
  timer.setInterval(READING_INTERVAL, sensorReadingTask);

  Serial.println("=== Setup Complete ===\n");
}

// -----------------------------------------------------------------
void loop() {
  Blynk.run();
  
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
  }
  
  timer.run();
  
  handleUltrasonic(); // async distance
  handleLED();        // async LED

  static unsigned long lastTrigger = 0;
  if (millis() - lastTrigger >= READING_INTERVAL) {
    startUltrasonic(); // trigger ultrasonic
    lastTrigger = millis();
  }

  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnectLog = 0;
    if (millis() - lastReconnectLog > 10000) {
      Serial.println("WiFi disconnected (loop) - Blynk will attempt reconnect.");
      lastReconnectLog = millis();
    }
  }
}

// -----------------------------------------------------------------
void sensorReadingTask() {
  float currentReading = asyncDistance; // use latest async measurement

  if (currentReading > 0) {
    distanceHistory[historyIndex] = currentReading;
    historyIndex = (historyIndex + 1) % MEDIAN_FILTER_SIZE;
  }
  
  float distance = getMedianDistance(); // Get median from history

  if (distance > MIN_DISTANCE && distance < MAX_DISTANCE) {
    currentWaterLevel = calculateWaterLevel(distance);
    currentDistance = distance;
    float waterHeight = DISTANCE_EMPTY - distance;

    Serial.print("Median Distance: "); Serial.print(distance, 1);
    Serial.print(" cm | Water Height: "); Serial.print(waterHeight, 1);
    Serial.print(" cm | Level: "); Serial.print(currentWaterLevel, 1);
    Serial.println("%");

    unsigned long updateInterval = (currentWaterLevel < LOW_LEVEL_THRESHOLD) ? FAST_INTERVAL : NORMAL_INTERVAL;

    bool significantChange = (lastWaterLevel < 0) || (abs(currentWaterLevel - lastWaterLevel) >= CHANGE_THRESHOLD);
    bool intervalElapsed = (millis() - lastBlynkUpdate >= updateInterval);

    if (significantChange || intervalElapsed) {
      
      if (millis() - g_lastEepromSaveTime > EEPROM_SAVE_INTERVAL) {
        saveToEEPROM(currentWaterLevel, distance);
        g_lastEepromSaveTime = millis();
        Serial.println("EEPROM write performed (interval-based).");
      }

      if (Blynk.connected()) {
        sendToBlynk(currentWaterLevel, distance);
      } else {
        Serial.println("Blynk not connected - skipping virtualWrite");
      }

      blinkLED(1, 50);  // Non-blocking LED pulse

      sendESPNOW(currentWaterLevel, distance);
      
      lastBlynkUpdate = millis();
      lastWaterLevel = currentWaterLevel;
    }
  } else {
    Serial.print("Invalid median reading: "); Serial.print(distance); Serial.print(" cm");
    if (currentReading == -2) Serial.println(" (Last reading: Sensor Timeout)");
    else if (currentReading == -3) Serial.println(" (Last reading: Echo Rollover Error)");
    else if (distance <= 0) Serial.println(" (No valid readings in buffer)");
    else Serial.println(" (Out of Range)");
  }
}

// -----------------------------------------------------------------
float getMedianDistance() {
  float sortedReadings[MEDIAN_FILTER_SIZE];
  int validCount = 0;
  
  for(int i = 0; i < MEDIAN_FILTER_SIZE; i++) {
      if(distanceHistory[i] > 0) {
          sortedReadings[validCount++] = distanceHistory[i];
      }
  }

  if (validCount == 0) {
      return -1.0;
  }

  for (int i = 0; i < validCount - 1; i++) {
      for (int j = i + 1; j < validCount; j++) {
          if (sortedReadings[i] > sortedReadings[j]) {
              float temp = sortedReadings[i];
              sortedReadings[i] = sortedReadings[j];
              sortedReadings[j] = temp;
          }
      }
  }
  
  return sortedReadings[validCount / 2];
}

// -----------------------------------------------------------------
void startUltrasonic() {
  if (!trigPending) {
    digitalWrite(TRIG_PIN, HIGH);
    trigStartTime = micros();
    trigPending = true;
  }
}

// -----------------------------------------------------------------
void handleUltrasonic() {
  if (trigPending && micros() - trigStartTime >= 10) {
    digitalWrite(TRIG_PIN, LOW);
    trigPending = false;
  }

  static bool lastEcho = LOW;
  bool currEcho = digitalRead(ECHO_PIN);
  
  if (currEcho && !lastEcho) echoStart = micros();  // Rising edge
  if (!currEcho && lastEcho) {                     // Falling edge
    echoEnd = micros();
    if (echoEnd > echoStart) { // Ensure valid reading
      unsigned long duration = echoEnd - echoStart;
      if (duration < ULTRASONIC_TIMEOUT) {
        asyncDistance = duration * 0.0343 / 2.0;
      } else {
        asyncDistance = -2; // Timeout error
      }
    } else {
      asyncDistance = -3; // Rollover or noise error
    }
  }
  lastEcho = currEcho;
}

// -----------------------------------------------------------------
void blinkLED(int times, int blinkInterval) {
  led.remainingBlinks = times * 2; 
  led.interval = blinkInterval;
  led.lastToggle = millis();
  led.state = LOW; 
  digitalWrite(led.pin, led.state);
}

// -----------------------------------------------------------------
void handleLED() {
  if (led.remainingBlinks > 0 && millis() - led.lastToggle >= led.interval) {
    led.state = !led.state;
    digitalWrite(led.pin, led.state);
    led.lastToggle = millis();
    led.remainingBlinks--;
    
    if (led.remainingBlinks == 0) {
      led.state = HIGH; // Ensure LED is OFF at the end
      digitalWrite(led.pin, led.state);
    }
  }
}

// -----------------------------------------------------------------
float calculateWaterLevel(float distance) {
  float percentage = 100.0 * (DISTANCE_EMPTY - distance) / TANK_HEIGHT;
  if (percentage < 0) percentage = 0;
  if (percentage > 100) percentage = 100;
  return percentage;
}

// -----------------------------------------------------------------
void sendToBlynk(float level, float distance) {
  Blynk.virtualWrite(VPIN_WATER_LEVEL, level);
  Blynk.virtualWrite(VPIN_DISTANCE, distance);
  Serial.print("Sent to Blynk: "); Serial.print(level, 1); Serial.println("%");
}

// -----------------------------------------------------------------
void sendESPNOW(float level, float distance) {
  g_lastEspNowPacket.waterLevel = level;
  g_lastEspNowPacket.distance = distance;
  g_lastEspNowPacket.timestamp = millis();
  g_espNowRetryPending = true; 
  g_espNowPacketStartTime = millis(); 

  SensorData localPacket = g_lastEspNowPacket; 

  // Use old send function (no return value check)
  esp_now_send(receiverMacAddress, (uint8_t*)&localPacket, sizeof(localPacket));
  Serial.println("esp_now_send requested (Legacy Mode).");
}

// -----------------------------------------------------------------
void espNowRetryTask() {
  if (g_espNowRetryPending) {
    
    if (millis() - g_espNowPacketStartTime > ESPNOW_RETRY_TIMEOUT) {
      Serial.println("ESP-NOW: Gave up sending packet after 30s.");
      g_espNowRetryPending = false; // Give up
      return; 
    }

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Retrying ESP-NOW packet (WiFi disconnected, but will try anyway)...");
    } else {
      Serial.println("Retrying failed ESP-NOW packet...");
    }
    
    SensorData localPacket = g_lastEspNowPacket; 
    
    esp_now_send(receiverMacAddress, (uint8_t*)&localPacket, sizeof(localPacket));
    Serial.println("ESP-NOW retry send requested (Legacy Mode).");
  }
}

// -----------------------------------------------------------------
void saveToEEPROM(float level, float distance) {
  EepromData data;
  data.magic = EEPROM_MAGIC;
  data.level = level;
  data.distance = distance;
  
  EEPROM.put(EEPROM_ADDR, data);
  EEPROM.commit();
}

// -----------------------------------------------------------------
void loadFromEEPROM() {
  EepromData data;
  EEPROM.get(EEPROM_ADDR, data);

  if (data.magic == EEPROM_MAGIC && 
      !isnan(data.level) && 
      !isnan(data.distance) &&
      data.level >= 0.0 && data.level <= 100.0 && 
      data.distance > 0 && data.distance < 1000) 
  {
    lastWaterLevel = data.level;
    currentDistance = data.distance;
    Serial.print("Loaded from EEPROM - Level: ");
    Serial.print(lastWaterLevel, 1);
    Serial.print("%, Distance: ");
    Serial.print(currentDistance, 1);
    Serial.println(" cm");
  } else {
    Serial.println("EEPROM values invalid or not initialized. Using defaults.");
  }
}

// -----------------------------------------------------------------
void onDataSent(uint8_t *mac_addr, uint8_t status) {
  char macStr[18];
  if (mac_addr) {
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
      mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  } else {
    strcpy(macStr, "??:??:??:??:??:??");
  }

  Serial.print("ESP-NOW send to "); Serial.print(macStr);
  Serial.print(" -> ");
  if (status == 0) {
    Serial.println("Success");
    g_espNowRetryPending = false; // Clear flag on success
  } else {
    Serial.printf("Fail (status=%u)\n", status);
    g_espNowRetryPending = true; // Keep flag set for retry
  }
}

// -----------------------------------------------------------------
// Blynk handlers
BLYNK_CONNECTED() {
  Serial.println("Blynk Connected!");
  if (lastWaterLevel >= 0) {
    Blynk.virtualWrite(VPIN_WATER_LEVEL, lastWaterLevel);
    Blynk.virtualWrite(VPIN_DISTANCE, currentDistance);
  }
  blinkLED(2, 100); // Non-blocking blink
}

BLYNK_WRITE(VPIN_MANUAL_REFRESH) {
  if (param.asInt() == 1) {
    Serial.println("Manual refresh triggered");
    startUltrasonic(); 
    timer.setTimeout(250L, sensorReadingTask);
  }
}
