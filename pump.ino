#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// ESP8266 GPIO Timer Firmware
// Reads input from GPIO pin, if high turns on another GPIO for 5 minutes, then turns it off

const char* ssid = "xxxxx";
const char* password = "xxxxx";
const char* webAppUrl = "xxxxx";

// Pin definitions
const int ON_PIN = 4;  // GPIO pin for output (change as needed)

// Timer variables
unsigned long timerStartTime = 0;
const unsigned long TIMER_DURATION = 5 * 60 * 1000; // 5 minutes in milliseconds
const unsigned long INTERVAL_DURATION = 12UL * 60UL * 60UL * 1000UL; // 12 hours in milliseconds
const unsigned int WIFI_RETRY_COUNT = 30;
bool timerActive = false;
unsigned int retries = 0;
unsigned long lastAutoStartTime = 0;

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);
  delay(1000);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && retries <= WIFI_RETRY_COUNT) {
    delay(500);
    Serial.print(".");
    Serial.print(retries);
    retries++;
  }
  Serial.println("\nWiFi connected.");

  // Configure output pin (relay is active-low, controlled by ground)
  pinMode(ON_PIN, OUTPUT);
  digitalWrite(ON_PIN, HIGH); // Start with relay OFF (HIGH = relay OFF, LOW = relay ON)

  Serial.println("ESP8266 GPIO Timer initialized");
  Serial.print("Output pin: GPIO ");
  Serial.println(ON_PIN);
  Serial.print("Timer duration: ");
  Serial.print(TIMER_DURATION / 1000);
  Serial.println(" seconds");
  Serial.println("Note: Relay is active-low (LOW = ON, HIGH = OFF)");

  // Trigger first run immediately on boot; set to millis() to wait full 12h before first run
  lastAutoStartTime = millis() - INTERVAL_DURATION;

  reportState("esp", "on");
}

void reportState(const char* device, const char* state) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    HTTPClient http;

    // Skip certificate validation for Google Apps Script
    // This is acceptable for non-sensitive data
    client.setInsecure();

    // Create JSON object
    String jsonStr;
    DynamicJsonDocument doc(1024);
    doc["device"] = device;
    doc["state"] = state;
    serializeJson(doc, jsonStr);

    http.begin(client, webAppUrl);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(jsonStr);

    if (httpResponseCode > 0) {
      Serial.printf("[HTTP] POST... code: %d\n", httpResponseCode);
      String response = http.getString();
      Serial.println(response);
    } else {
      Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    http.end();
  }
}

void loop() {
  // Scheduled trigger: turn ON_PIN every 12 hours for 5 minutes
  if (!timerActive) {
    unsigned long sinceLast = millis() - lastAutoStartTime;
    if (sinceLast >= INTERVAL_DURATION) {
      timerStartTime = millis();
      timerActive = true;
      lastAutoStartTime = timerStartTime;
      digitalWrite(ON_PIN, LOW); // active-low: LOW = ON
      Serial.println("Scheduled 12h trigger - Timer started, pump ON");
      reportState("pump", "on");
    }
  }

  // Check if timer is active and if 5 minutes have elapsed
  if (timerActive) {
    unsigned long elapsedTime = millis() - timerStartTime;

    if (elapsedTime >= TIMER_DURATION) {
      // 5 minutes have passed - turn off relay
      digitalWrite(ON_PIN, HIGH); // Set HIGH to turn relay OFF (active-low)
      timerActive = false;
      Serial.println("5 minutes elapsed - Timer stopped, pump OFF");

      reportState("pump", "off");
    } else {
      // Timer still running
      unsigned long remainingSeconds = (TIMER_DURATION - elapsedTime) / 1000;
    }
  }

  // Small delay to prevent excessive CPU usage
  delay(100);
}
