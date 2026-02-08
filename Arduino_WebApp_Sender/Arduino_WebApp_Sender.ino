/*
  Water Level Monitor - Web App Sender
  Sends water level data to Supabase via HTTPS

  This sketch sends water level readings from the sensor
  to the web dashboard through the Supabase Edge Function.
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

// TODO: Replace with your WiFi credentials before uploading
#define WIFI_SSID "YOUR_WIFI_SSID_HERE"
#define WIFI_PASS "YOUR_WIFI_PASSWORD_HERE"

#define TRIG_PIN 5        // GPIO5 (D1)
#define ECHO_PIN 4        // GPIO4 (D2)
#define LED_PIN LED_BUILTIN

#define DISTANCE_EMPTY 217.0
#define DISTANCE_FULL 21.0
#define TANK_HEIGHT (DISTANCE_EMPTY - DISTANCE_FULL)
#define MIN_DISTANCE 25.0
#define MAX_DISTANCE 450.0
#define ULTRASONIC_TIMEOUT 30000UL

// TODO: Replace with your Supabase project URL and anonymous key
// Get these from your Supabase project settings: https://supabase.com/dashboard/project/_/settings/api
#define SUPABASE_URL "YOUR_SUPABASE_URL_HERE"
#define SUPABASE_ANON_KEY "YOUR_SUPABASE_ANON_KEY_HERE"

float distanceHistory[5];
int historyIndex = 0;
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 30000; // Send every 30 seconds

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n=== Water Monitor Web Sender ===");

  pinMode(LED_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  for (int i = 0; i < 5; i++) {
    distanceHistory[i] = -1.0;
  }

  connectToWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  float distance = measureDistance();

  if (distance > MIN_DISTANCE && distance < MAX_DISTANCE) {
    distanceHistory[historyIndex] = distance;
    historyIndex = (historyIndex + 1) % 5;

    if (millis() - lastSend >= SEND_INTERVAL) {
      float medianDist = getMedianDistance();
      float level = calculateWaterLevel(medianDist);

      Serial.print("Level: ");
      Serial.print(level, 1);
      Serial.print("% | Distance: ");
      Serial.print(medianDist, 1);
      Serial.println(" cm");

      sendToWebApp(level, medianDist);
      lastSend = millis();
    }
  }

  delay(500);
}

void connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());
    digitalWrite(LED_PIN, LOW); // LED on
    delay(500);
    digitalWrite(LED_PIN, HIGH); // LED off
  } else {
    Serial.println("\nFailed to connect WiFi");
  }
}

float measureDistance() {
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, ULTRASONIC_TIMEOUT);

  if (duration == 0) {
    return -1.0;
  }

  return duration * 0.0343 / 2.0;
}

float getMedianDistance() {
  float sorted[5];
  int validCount = 0;

  for (int i = 0; i < 5; i++) {
    if (distanceHistory[i] > 0) {
      sorted[validCount++] = distanceHistory[i];
    }
  }

  if (validCount == 0) return -1.0;

  for (int i = 0; i < validCount - 1; i++) {
    for (int j = i + 1; j < validCount; j++) {
      if (sorted[i] > sorted[j]) {
        float temp = sorted[i];
        sorted[i] = sorted[j];
        sorted[j] = temp;
      }
    }
  }

  return sorted[validCount / 2];
}

float calculateWaterLevel(float distance) {
  float percentage = 100.0 * (DISTANCE_EMPTY - distance) / TANK_HEIGHT;
  if (percentage < 0) percentage = 0;
  if (percentage > 100) percentage = 100;
  return percentage;
}

void sendToWebApp(float level, float distance) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return;
  }

  HTTPClient http;

  String url = String(SUPABASE_URL) + "/functions/v1/save_water_reading";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_ANON_KEY));

  String payload = "{\"water_level\":" + String(level, 1) +
                   ",\"distance\":" + String(distance, 1) +
                   ",\"timestamp\":\"" + getISO8601Time() + "\"}";

  int httpCode = http.POST(payload);

  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Data sent successfully");
    digitalWrite(LED_PIN, LOW);
    delay(100);
    digitalWrite(LED_PIN, HIGH);
  } else {
    Serial.print("HTTP Error: ");
    Serial.println(httpCode);
  }

  http.end();
}

String getISO8601Time() {
  time_t now = time(nullptr);
  struct tm* timeinfo = gmtime(&now);

  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", timeinfo);

  return String(buffer);
}
