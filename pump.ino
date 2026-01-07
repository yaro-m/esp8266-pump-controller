#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// ESP8266 GPIO Timer Firmware
// Reads input from GPIO pin, if high turns on another GPIO for 5 minutes, then turns it off

const char* ssid = "XXXXX";
const char* password = "xxxxxx";
const char* webAppUrl = "xxxxxx";

// Pin definitions
const int INPUT_PIN = 16;   // GPIO pin for input (change as needed)
const int ON_PIN = 4;  // GPIO pin for output (change as needed)
const int OFF_PIN = 4;  // GPIO pin for output (change as needed)

// Timer variables
unsigned long timerStartTime = 0;
const unsigned long TIMER_DURATION = 5 * 60 * 1000; // 5 minutes in milliseconds
const usigned int WIFI_RETRY_COUNT = 30;
bool timerActive = false;
bool lastInputState = false;
unsigned int retries = 0;

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

  // Configure input pin (no pull-up/pull-down)
  pinMode(INPUT_PIN, INPUT);

  // Configure output pin (relay is active-low, controlled by ground)
  pinMode(ON_PIN, OUTPUT);
  pinMode(OFF_PIN, OUTPUT);
  digitalWrite(ON_PIN, HIGH); // Start with relay OFF (HIGH = relay OFF, LOW = relay ON)
  digitalWrite(OFF_PIN, HIGH); // Start with relay OFF (HIGH = relay OFF, LOW = relay ON)
  digitalWrite(INPUT_PIN, LOW);

  Serial.println("ESP8266 GPIO Timer initialized");
  Serial.print("Input pin: GPIO ");
  Serial.println(INPUT_PIN);
  Serial.print("Output pin: GPIO ");
  Serial.println(ON_PIN);
  Serial.print("Timer duration: ");
  Serial.print(TIMER_DURATION / 1000);
  Serial.println(" seconds");
  Serial.println("Note: Input triggers on HIGH (rising edge)");
  Serial.println("Note: Relay is active-low (LOW = ON, HIGH = OFF)");

  reportState("esp", "on");
}

void reportState(char* device, char* state) {
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
  // Read current input state
  bool currentInputState = digitalRead(INPUT_PIN);

  // Detect rising edge (transition from LOW to HIGH)
  if (currentInputState == HIGH && lastInputState == LOW) {
    // Input just went high - start timer
    if (!timerActive) {
      timerStartTime = millis();
      timerActive = true;
      digitalWrite(ON_PIN, LOW); // Set LOW to turn relay ON (active-low)
      Serial.println("Input detected HIGH - Timer started, relay ON");

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
      Serial.println("5 minutes elapsed - Timer stopped, relay OFF");

      reportState("pump", "off");
    } else {
      // Timer still running
      unsigned long remainingSeconds = (TIMER_DURATION - elapsedTime) / 1000;
      // if (elapsedTime % 10000 == 0) { // Print every 10 seconds
        Serial.print("Timer running - ");
        Serial.print(remainingSeconds);
        Serial.println(" seconds remaining"); 
      // }
    }
  }

  // Update last input state for edge detection
  lastInputState = currentInputState;

  // Small delay to prevent excessive CPU usage
  delay(100);
}
