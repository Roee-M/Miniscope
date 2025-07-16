#include <Arduino.h>
#include <SPI.h>

#define CS_PIN   10
#define SCLK_PIN 12
#define MISO_PIN 13
#define VREF 3.3

SPIClass SPI_ADC(FSPI);  // Use FSPI for ESP32-S3

void setup() {
  Serial.begin(115200);

  // pinMode(CS_PIN, OUTPUT);
  // digitalWrite(CS_PIN, HIGH);
  SPI_ADC.begin(SCLK_PIN, MISO_PIN, -1, CS_PIN);
  SPI_ADC.setHwCs(1);
  SPI_ADC.beginTransaction(SPISettings(48000000, MSBFIRST, SPI_MODE1));  // 48 MHz, Mode 1
  

  Serial.println("finished setup ");
}

uint16_t readADC() {
  // digitalWrite(CS_PIN, LOW);   // Start conversion
  uint16_t adc_raw = SPI_ADC.transfer16(0x0000);  // Send 16 dummy bits, receive ADC data
  // digitalWrite(CS_PIN, HIGH);  // End frame
  // Extract the 12-bit ADC result from bits [13:2]
  return (adc_raw >> 2) & 0x0FFF;
}

void loop() {

  // static unsigned long lastMicros = 0;
  // unsigned long now = micros();
  uint16_t sample = readADC();
  float voltage = ((float)sample / 4095.0) * VREF;

  Serial.print("ADC Raw: ");
  Serial.println(sample);
  Serial.print(" | Voltage: ");
  Serial.println(voltage, 4);  // Print with 4 decimal places
  // Serial.print(" | Δt (us): ");
  // Serial.println(now - lastMicros);
  // lastMicros = now;
}


// #include "Arduino.h"
// #include "driver/spi_master.h"
// #include "driver/gpio.h"


// // #define CS_PIN   10
// // #define SCLK_PIN 12
// // #define MISO_PIN 13
// // #define VREF 3.3

// #define PIN_NUM_MISO 13
// #define PIN_NUM_MOSI -1
// #define PIN_NUM_CLK  12
// #define PIN_NUM_CS   10

// spi_device_handle_t spi;

// void spi_init() {
//     spi_bus_config_t buscfg = {};
//     buscfg.miso_io_num = PIN_NUM_MISO;
//     buscfg.mosi_io_num = PIN_NUM_MOSI;
//     buscfg.sclk_io_num = PIN_NUM_CLK;
//     buscfg.quadwp_io_num = -1;
//     buscfg.quadhd_io_num = -1;
//     buscfg.max_transfer_sz = 2;  // 2 bytes per transfer

//     spi_device_interface_config_t devcfg = {};
//     devcfg.clock_speed_hz = 48000000;  // 48 MHz SPI
//     devcfg.mode = 0;                   // SPI mode 0
//     devcfg.spics_io_num = PIN_NUM_CS;  // Use hardware CS (optional)
//     devcfg.queue_size = 1;
//     devcfg.flags = 0;

//     esp_err_t ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
//     if (ret != ESP_OK) {
//         Serial.printf("SPI bus init failed: %d\n", ret);
//     }

//     ret = spi_bus_add_device(SPI3_HOST, &devcfg, &spi);
//     if (ret != ESP_OK) {
//         Serial.printf("SPI add device failed: %d\n", ret);
//     }
// }

// uint16_t read_ad7276_sample() {
//     uint8_t rx_data[2] = {0};

//     spi_transaction_t t = {};
//     t.length = 14;  // bits
//     t.rx_buffer = rx_data;
//     t.tx_buffer = NULL;
//     t.flags = SPI_TRANS_USE_RXDATA;

//     esp_err_t ret = spi_device_transmit(spi, &t);
//     if (ret != ESP_OK) {
//         Serial.printf("SPI transmit failed: %d\n", ret);
//     }

//     uint16_t raw = (rx_data[0] << 8) | rx_data[1];
//     return raw >> 2;  // discard 2 trailing zeros
// }

// void setup() {
//     Serial.begin(115200);
//     spi_init();
// }

// void loop() {
//     uint16_t sample = read_ad7276_sample();
//     Serial.printf("Sample: %u\n", sample);
// }