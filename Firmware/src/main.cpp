#include <Arduino.h>
#include "signal_generator.h"

#define LED_PIN 8
#define LB_ADC_PIN 4
#define LB_OUT_PIN 5

SignalGenerator signalGen(LB_OUT_PIN, LED_PIN);

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  signalGen.begin();
  Serial.println("Finished setup");
}

void loop() {
  int pwmValue = signalGen.nextSample();
  signalGen.outputSample(pwmValue);

  delay(10);

  int val = analogRead(LB_ADC_PIN);
  Serial.print("Sine PWM: ");
  Serial.print(pwmValue);
  Serial.print(" ADC: ");
  Serial.println(val);
}

// TODO
// Other ESP:
//   Connect LB
  //? read_ADC() (-> API, implement as actual ADC SPI reads later)
  //? store in ESP memory
  //? Add trigger functionality
  //? send over Wifi/USB/mail
  //? simple server to plot incoming data

// Miniscope itself:  
  //? implement as actual ADC SPI reads
  //? Print the reads (Serial)

// Frontend:
  //? Upgrade debug server to be able to display and plot the data over the phone/web
  //? Add trigger functionality via phone