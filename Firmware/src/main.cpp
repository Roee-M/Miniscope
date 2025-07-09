#include <Arduino.h>
#define LED_PIN 48
#define LB_ADC_PIN 4  // GPIO4 for analogRead
#define LB_OUT_PIN 5  // GPIO5 outputs simulated signal

int val = 1;

void setup() {
  Serial.begin(115200); 
  pinMode(LED_PIN, OUTPUT);
  pinMode(LB_OUT_PIN, OUTPUT);
  digitalWrite(LB_OUT_PIN, HIGH);  // Simulate signal
  analogReadResolution(12);
  Serial.print("Finished setup");
}

void loop() {
  digitalWrite(LED_PIN, HIGH); // Turn LED on   

  digitalWrite(LB_OUT_PIN, HIGH);  // simulate signal
  delay(1);
  val = analogRead(LB_ADC_PIN);
  Serial.print("HIGH: ");
  Serial.println(val);

  digitalWrite(LED_PIN, LOW);  // Turn LED off

  digitalWrite(LB_OUT_PIN, LOW);  // simulate signal
  delay(1);
  val = analogRead(LB_ADC_PIN);
  Serial.print("LOW: ");
  Serial.println(val);

  delay(100);
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