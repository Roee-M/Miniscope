#include <Arduino.h>
#include "SignalGenerator.h"
#include "ADCSampler.h"
#include "WifiSender.h"

#define LED_PIN 8
#define LB_ADC_PIN 4
#define LB_OUT_PIN 5

const size_t BUFFER_SIZE = 100;

SignalGenerator signalGen(LB_OUT_PIN, LED_PIN);
ADCSampler adcSampler(LB_ADC_PIN, BUFFER_SIZE);
WifiSender wifiSender;

void setup() {
  Serial.begin(115200);
  signalGen.begin();
  adcSampler.begin();
  wifiSender.begin("", "");  // replace with real creds
}

void loop() {
  int pwmValue = signalGen.nextSample();
  signalGen.outputSample(pwmValue);

  if (adcSampler.sample()) {
    wifiSender.sendBuffer(adcSampler.getBuffer(), adcSampler.getBufferSize());
    delay(5000); // Wait before allowing another trigger
  }

  delay(10);
}


// TODO
// Other ESP:
//   Connect LB
  //? read_ADC() (-> API, implement as actual ADC SPI reads later)
  //? store in ESP memory - fill out buffer
  //? Add trigger functionality
  //? send over Wifi/USB/mail
  //? simple server to plot incoming data

// Miniscope itself:  
  //? implement as actual ADC SPI reads
  //? Print the reads (Serial)

// Frontend:
  //? Upgrade debug server to be able to display and plot the data over the phone/web
  //? Add trigger functionality via phone