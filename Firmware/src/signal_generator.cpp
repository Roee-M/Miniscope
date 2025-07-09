#include "signal_generator.h"
#include <math.h>

SignalGenerator::SignalGenerator(int outputPin, int ledPin)
  : _outputPin(outputPin),
    _ledPin(ledPin),
    _sampleCount(100),
    _currentIndex(0),
    _amplitude(127),
    _offset(128),
    _twoPi(2 * PI) {}

void SignalGenerator::begin() {
  pinMode(_outputPin, OUTPUT);
  pinMode(_ledPin, OUTPUT);
  analogWriteResolution(8); // PWM 0-255
}

int SignalGenerator::nextSample() {
  float angle = (_twoPi * _currentIndex) / _sampleCount;
  int pwmValue = (int)(_offset + _amplitude * sin(angle));
  _currentIndex = (_currentIndex + 1) % _sampleCount;
  return pwmValue;
}

void SignalGenerator::outputSample(int val) {
  analogWrite(_outputPin, val);
  digitalWrite(_ledPin, val > 128 ? HIGH : LOW);
}
