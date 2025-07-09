#ifndef SIGNAL_GENERATOR_H
#define SIGNAL_GENERATOR_H

#include <Arduino.h>

class SignalGenerator {
public:
  SignalGenerator(int outputPin, int ledPin);
  void begin();
  int nextSample();          // Generate next PWM sample value
  void outputSample(int val); // Output PWM on pin and update LED
private:
  int _outputPin;
  int _ledPin;
  int _sampleCount;
  int _currentIndex;
  int _amplitude;
  int _offset;
  float _twoPi;
};

#endif
