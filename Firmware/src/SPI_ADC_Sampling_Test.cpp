// #include <Arduino.h>
// #include <SPI.h>

// #define CS_PIN   10
// #define SCLK_PIN 12
// #define MISO_PIN 13
// #define VREF 3.3

// SPIClass SPI_ADC(FSPI);  // Use FSPI for ESP32-S3

// void setup() {
//   Serial.begin(115200);

//   pinMode(CS_PIN, OUTPUT);
//   digitalWrite(CS_PIN, HIGH);

//   SPI_ADC.begin(SCLK_PIN, MISO_PIN, -1, CS_PIN);
//   SPI_ADC.beginTransaction(SPISettings(48000000, MSBFIRST, SPI_MODE1));  // 48 MHz, Mode 1
// }

// uint16_t readADC() {
//   digitalWrite(CS_PIN, LOW);   // Start conversion

//   uint16_t adc_raw = SPI_ADC.transfer16(0x0000);  // Send 16 dummy bits, receive ADC data

//   digitalWrite(CS_PIN, HIGH);  // End frame

//   // Extract the 12-bit ADC result from bits [13:2]
//   return (adc_raw >> 2) & 0x0FFF;
// }

// void loop() {
//   uint16_t sample = readADC();
//   float voltage = ((float)sample / 4095.0) * VREF;

//   Serial.print("ADC Raw: ");
//   Serial.print(sample);
//   Serial.print(" | Voltage: ");
//   Serial.println(voltage, 4);  // Print with 4 decimal places
// }