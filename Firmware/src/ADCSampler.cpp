#include "ADCSampler.h"
#define THRESHOLD 2000

ADCSampler::ADCSampler(uint8_t pin, size_t bufferSize)
  : _pin(pin), _size(bufferSize), _index(0), _triggered(false) {
  _buffer = new uint16_t[_size];
}

void ADCSampler::begin() {
  analogReadResolution(12);
}

bool ADCSampler::sample() {
  if (_triggered) return true;

  uint16_t val = analogRead(_pin);
  _buffer[_index] = val;
  _index = (_index + 1) % _size;

  if (val > THRESHOLD) {
    _triggered = true;
    return true;
  }

  return false;
}

const uint16_t* ADCSampler::getBuffer() const {
  return _buffer;
}

size_t ADCSampler::getBufferSize() const {
  return _size;
}
