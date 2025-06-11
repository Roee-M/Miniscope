#include <Arduino.h>

#define BUFFER_SIZE 1024
const uint16_t triggerThreshold = 1241; // ~1V trigger for 12-bit ADC (3.3V ref)

uint16_t buffer[BUFFER_SIZE];
volatile bool capturing = false;
volatile uint16_t captureIndex = 0;
bool started = false;

uint16_t mockSample = 0;
uint16_t clockCounter = 0;
const uint16_t clockPeriod = 100; // change for clock speed (samples per half period)

void setup() {
  Serial.begin(115200);
  while (!Serial); // wait for serial connection
  Serial.println("MiniScope Triggered Capture (Mock Clock) Ready");
}

void loop() {
  // Check start/stop commands
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'S') {
      started = true;
      capturing = false;
      captureIndex = 0;
      Serial.println("Started triggered capture");
    } else if (cmd == 'P') {
      started = false;
      capturing = false;
      captureIndex = 0;
      Serial.println("Stopped");
    }
  }

  if (!started) return;

  // Generate mock clock signal: toggles between 0 and 4095 every clockPeriod samples
  if (clockCounter < clockPeriod) {
    mockSample = 0;
  } else {
    mockSample = 4095;
  }
  clockCounter++;
  if (clockCounter >= 2 * clockPeriod) clockCounter = 0;

  // Trigger when voltage > 1V → when mockSample is high (4095)
  if (!capturing) {
    if (mockSample > triggerThreshold) {
      capturing = true;
      captureIndex = 0;
    }
  }

  if (capturing) {
    buffer[captureIndex++] = mockSample;

    if (captureIndex >= BUFFER_SIZE) {
      Serial.write((uint8_t*)buffer, BUFFER_SIZE * 2);
      capturing = false;
      captureIndex = 0;
    }
  }
}
