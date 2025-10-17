#ifndef ADC_SAMPLER_H
#define ADC_SAMPLER_H

#include <Arduino.h>

class ADCSampler {
public:
  ADCSampler(uint8_t pin, size_t bufferSize);
  void begin();
  bool sample();          // Returns true if trigger condition met
  const uint16_t* getBuffer() const;
  size_t getBufferSize() const;

private:
  uint8_t _pin;
  size_t _size;
  uint16_t* _buffer;
  size_t _index;
  bool _triggered;
};

#endif
