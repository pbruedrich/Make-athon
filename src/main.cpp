#include <Arduino.h>

// Simple LED blink test for esp32-c6-dev-1
// Adjust LED_PIN if your board uses a different pin for the on-board LED.

const int LED_PIN = 2; // common on many ESP32 dev boards; change if needed

void setup() {
  Serial.begin(115200);
  delay(50);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("LED blink test starting");
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
  delay(500);
}