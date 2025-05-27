#ifndef ADC_MOCK_HPP
#define ADC_MOCK_HPP

#include <Arduino.h>

class ADCMock {
public:
    ADCMock();
    void begin();
    uint16_t readSample();
    void fillSquareWaveBuffer(uint8_t* buf, size_t num_samples, uint16_t high, uint16_t low, size_t period, size_t offset = 0);
private:
    uint16_t counter;
};

#endif // ADC_MOCK_HPP
