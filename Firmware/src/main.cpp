/* This main is used to read data samples form the AD7276 adc on the miniscope, and display it on the serial print
  the current method is to use the SPI library and uses hardware CS pin to automaticly toggle it instead of manually toggling it
  16/06/2025
*/

#include <Arduino.h>
#include <SPI.h>

#define CS_PIN   10
#define SCLK_PIN 12
#define MISO_PIN 13
#define VREF 3.3
// #define SPI_SPEED 48000000  // 48 MHz
#define TRIGGER_PIN 0  // Pin for manually triggering


#define BUFFER_SIZE (1 * 1000)  // 1M samples for MVP - CHANGE if NEEDED - chaged to 1000
#define SAMPLE_RATE 3000000  // 3 MSPS
#define SAMPLE_T 2000  // trigger voltage threshold - CHANGE if NEEDED
#define CAPTURE_SIZE   (BUFFER_SIZE / 2)  // total samples to save on trigger
#define PRINT_AROUND   32                 // how many samples around the trigger to print automatically
#define SPI_HZ 20000000  // SPI clock speed



uint16_t *ringBuffer;
volatile size_t writeIndex = 0;
bool triggered = false;
bool captureReady = false;
size_t triggerIndex = 0;
size_t samplesAfterTrigger = 0;
const size_t HALF_WINDOW = BUFFER_SIZE / 2;
bool button_pressed = false;  // for manual trigger reset

SPIClass SPI_ADC(FSPI);  // Use FSPI for ESP32-S3

void setup() {
  Serial.begin(115200);

  // pinMode(CS_PIN, OUTPUT);
  // digitalWrite(CS_PIN, HIGH);
  SPI_ADC.begin(SCLK_PIN, MISO_PIN, -1, CS_PIN);
  SPI_ADC.setHwCs(1);
  SPI_ADC.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE0));  // 20 MHz, Mode 0 - best results, try 40Mhz. tried 40Mhz and Mode 1
  delay(1000);  // wait for serial to be ready
  Serial.println("allocating ring buffer");
  ringBuffer = (uint16_t*)ps_malloc(BUFFER_SIZE * sizeof(uint16_t));
  if (!ringBuffer) {
    Serial.println("Failed to allocate ring buffer in PSRAM!");
  }
    // optional debug: zero ring so unwritten region is known
  memset(ringBuffer, 0, BUFFER_SIZE * sizeof(uint16_t));
  
  // --- setup trigger pin ---
  pinMode(TRIGGER_PIN, INPUT_PULLUP);

  Serial.println("---finished setup---");
}

/**
 * @brief Read a sample from the AD7276 ADC
 *        using SPI transfer.
 * 
 * @return uint16_t - sample value between 0 and 4095
 */
uint16_t readADC() {
  // digitalWrite(CS_PIN, LOW);   // Start conversion
  uint16_t adc_raw = SPI_ADC.transfer16(0x0000);  // Send 16 dummy bits, receive ADC data
  // digitalWrite(CS_PIN, HIGH);  // End frame
  // Extract the 12-bit ADC result from bits [13:2] - WHY?
  return (adc_raw >> 2) & 0x0FFF;
  // Extract the 12-bit ADC result from bits [11:0] - TESTING
  // return (adc_raw) & 0x0FFF;
}

/**
 * @brief prints the captured data around the trigger point
 *        in the ring buffer.
 */
void printCapture() {
  size_t start = (triggerIndex + BUFFER_SIZE - HALF_WINDOW/2) % BUFFER_SIZE;
  Serial.println("=== Triggered Window (raw values) ===");
  for (size_t i = 0; i < HALF_WINDOW; ++i) {
    size_t idx = (start + i) % BUFFER_SIZE;
    Serial.print(ringBuffer[idx]);
    Serial.print(",");
  }
  Serial.println("=== End Window ===");
}


void loop() {
  uint16_t sample = readADC();
  ringBuffer[writeIndex] = sample;

  if (!triggered && sample > SAMPLE_T) {
    triggered = true;
    triggerIndex = writeIndex;
    samplesAfterTrigger = 0;
    Serial.printf("Trigger at idx=%u raw=%u\n", (unsigned)triggerIndex, sample);
  }

  if (triggered && !captureReady) {
    samplesAfterTrigger++;
    if (samplesAfterTrigger >= HALF_WINDOW / 2) { 
      captureReady = true;
      Serial.println("Capture ready: pre-trigger + post-trigger window complete.");
      printCapture();

    }
  }

  if (digitalRead(TRIGGER_PIN) == LOW && !button_pressed) {  
    button_pressed = true;  // Set flag to prevent multiple resets
    triggered = false;
    captureReady = false;
    samplesAfterTrigger = 0;
    triggerIndex = 0;
    writeIndex = 0;  // Reset write index
    memset(ringBuffer, 0, BUFFER_SIZE * sizeof(uint16_t));
    Serial.println("Manual trigger reset! Waiting for next signal...");
    delay(10);  // Debounce delay
    button_pressed = false;  // Reset button pressed flag
  }

  writeIndex = (writeIndex + 1) % BUFFER_SIZE;
}


// void loop() {

//   // --- sampling and storing the data in the ring buffer ---

//   uint16_t sample = readADC();
//   // float voltage = ((float)sample / 4095.0) * VREF;
//   ringBuffer[writeIndex++ %(BUFFER_SIZE)] = sample;
//   if (sample > SAMPLE_T && !triggered)
//   {
//     triggered = true;

  
//   }
  

//   if (writeIndex % 1000 == 0) {  // print every 1000 samples
//     Serial.printf("Sample %u: %u\n", writeIndex, voltage);
//   }

  // --- sampling and printing the data - uncomment for debugging ---

  // // static unsigned long lastMicros = 0;
  // // unsigned long now = micros();
  // uint16_t sample = readADC();
  // float voltage = ((float)sample / 4095.0) * VREF;

  // Serial.print("ADC Raw: ");
  // Serial.println(sample);
  // Serial.print(" | Voltage: ");
  // Serial.println(voltage, 4);  // Print with 4 decimal places
  // // Serial.print(" | Δt (us): ");
  // // Serial.println(now - lastMicros);
  // // lastMicros = now;
// }


/*  the version below uses the esp-idf version of the spi_master driver, which is more low level and requires manual toggling of the CS pin
   this version is not used in the current implementation, but is kept for reference
   it can be used to read samples from the AD7276 adc on the miniscope, and display it on the serial print
   maybe used for later more advanced features like using DMA
*/
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