#include <Arduino.h>
#define LED_PIN 8
#define LB_ADC_PIN 4  // GPIO4 for analogRead
#define LB_OUT_PIN 5  // GPIO5 outputs simulated signal

const int amplitude = 127;   // PWM amplitude (0-127)
const int offset = 128;      // PWM offset (middle point)
const float frequency = 1;   // 1 Hz sine wave
const int sampleCount = 100; // number of steps per cycle
int val = 500;

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
  static int i = 0;
  float angle = (2 * PI * i) / sampleCount;
  int pwmValue = (int)(offset + amplitude * sin(angle));
  analogWrite(LB_OUT_PIN, pwmValue);

  digitalWrite(LED_PIN, pwmValue > 128 ? HIGH : LOW);

  delay(10);

  int val = analogRead(LB_ADC_PIN);
  Serial.print("Sine PWM: ");
  Serial.print(pwmValue);
  Serial.print(" ADC: ");
  Serial.println(val);

  i = (i + 1) % sampleCount;
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