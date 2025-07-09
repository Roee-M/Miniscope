// Simple Blink Example for ESP32-S3 using GPIO8
#include <Arduino.h>
#define LED_PIN 8

void setup() {
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_PIN, HIGH); // Turn LED on
  delay(1000);                  // Wait 500ms
  digitalWrite(LED_PIN, LOW);  // Turn LED off
  delay(1000);                  // Wait 500ms
}