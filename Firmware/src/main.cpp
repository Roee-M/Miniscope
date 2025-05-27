#include <Arduino.h>
#include "adc_mock.hpp"
#include "usb_comm.hpp"
#include "controller.hpp"


#define SAMPLE_RATE 1000000 // 1 MSPS
#define WINDOW_SEC 0.5
#define SAMPLES_PER_WINDOW (size_t)(SAMPLE_RATE * WINDOW_SEC)
#define BUFFER_SIZE (SAMPLES_PER_WINDOW * 2) // 2 bytes per sample

#define CHUNK_SAMPLES 1024  // Number of samples per chunk (adjust for RAM)
#define CHUNK_SIZE (CHUNK_SAMPLES * 2) // 2 bytes per sample

uint8_t buffer[CHUNK_SIZE];


volatile size_t buffer_index = 0;
volatile bool buffer_ready = false;


ADCMock adc;
USBComm usb;
Controller controller;


unsigned long lastSampleTime = 0;

void setup() {
    usb.begin();
    adc.begin();
    controller.begin();
}


void loop() {
    controller.handleInput();

    if (controller.isRunning()) {
        // Simulate continuous DMA-style streaming
        static size_t sample_offset = 0;
        // Fill buffer with square wave chunk
        adc.fillSquareWaveBuffer(buffer, CHUNK_SAMPLES, 0x0FFF, 0x0000, 40, sample_offset);
        // Send chunk via USB
        usb.sendData(buffer, CHUNK_SIZE);
        // Advance offset for waveform continuity
        sample_offset += CHUNK_SAMPLES;
        // Keep offset within one period for correct waveform
        if (sample_offset > 1000000) sample_offset = 0; // Prevent overflow

        // Wait for the time it would take to acquire this chunk at 2 MSPS
        delayMicroseconds((CHUNK_SAMPLES * 1000000) / SAMPLE_RATE);

        unsigned long now = millis();
        if (now - lastSampleTime >= 1000) {
            controller.logStatus();
            lastSampleTime = now;
        }
    }
}