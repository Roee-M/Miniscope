#include "adc_mock.hpp"

ADCMock::ADCMock() : counter(0) {}

void ADCMock::begin() {
    // Initialize mock ADC (simulate SPI init here)
    Serial.println("[ADCMock] Initialized.");
}

uint16_t ADCMock::readSample() {
    // Simulate an incrementing 12-bit ADC value
    counter = (counter + 1) & 0x0FFF;
    return counter;

}

void ADCMock::fillSquareWaveBuffer(uint8_t* buf, size_t num_samples, uint16_t high, uint16_t low, size_t period, size_t offset) {
    for (size_t i = 0; i < num_samples; ++i) {
        size_t idx = i + offset;
        uint16_t value = (idx % period < period / 2) ? high : low;
        buf[2 * i]     = value & 0xFF;
        buf[2 * i + 1] = (value >> 8) & 0x0F;
    }
}