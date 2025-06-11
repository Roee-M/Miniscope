#include <Arduino.h>

bool running = true;
bool signal_state = false;
unsigned long last_toggle = 0;
const unsigned long clock_period_ms = 100;  // 1 Hz clock = 1000 ms period (toggle every 500 ms)

void setup() {
  Serial.begin(115200);
  while (!Serial);  // Wait for serial connection
}

void loop() {
  // Check for Start/Stop commands
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'S') {
      running = true;
    } else if (cmd == 'P') {
      running = false;
    }
  }

  if (running) {
    unsigned long now = millis();

    // Toggle the signal every 500 ms (half period)
    if (now - last_toggle >= clock_period_ms / 2) {
      last_toggle = now;
      signal_state = !signal_state;
    }

    // Send the current signal value as a 2-byte sample (simulate ADC)
    uint16_t sample = signal_state ? 4095 : 0;  // 12-bit full scale or zero
    Serial.write((uint8_t*)&sample, 2);

    delayMicroseconds(100);  // Sampling rate (~10 kHz)
  }
}
