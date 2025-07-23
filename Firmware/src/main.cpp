/* This main is used to read data samples form the AD7276 adc on the miniscope, and display it on the serial print
  the current method is to use the SPI library and uses hardware CS pin to automaticly toggle it instead of manually toggling it
  23/07/2025 - UDP with ADC Sampling
*/

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_task_wdt.h>

// use manually toggeling cs insted of  SPI_ADC.setHwCs(1); in setup so it will be faster
#define CS_LOW() GPIO.out_w1tc = (1 << CS_PIN) // Clear bit 
#define CS_HIGH() GPIO.out_w1ts = (1 << CS_PIN) // Set bit

#define CS_PIN 10
#define SCLK_PIN 12
#define MISO_PIN 13
#define VREF 3.3
#define TRIGGER_PIN 0 // Pin for manually triggering

#define BUFFER_SIZE (1 * 1000)         // 1M samples for MVP - CHANGE if NEEDED - chaged to 1000
#define SAMPLE_RATE 3000000            // 3 MSPS
#define SAMPLE_T 2000                  // trigger voltage threshold - CHANGE if NEEDED
#define CAPTURE_SIZE (BUFFER_SIZE / 2) // total samples to save on trigger
#define PRINT_AROUND 32                // how many samples around the trigger to print automatically
#define SPI_HZ 48000000                // SPI clock speed

uint16_t *ringBuffer;
volatile size_t writeIndex = 0;
volatile bool triggered = false;
volatile bool captureReady = false;
int pre_trigger_samples = 0;

size_t triggerIndex = 0;
size_t samplesAfterTrigger = 0;
const size_t HALF_WINDOW = BUFFER_SIZE / 2;
bool button_pressed = false; // for manual trigger reset

SPIClass SPI_ADC(FSPI); // Use FSPI for ESP32-S3

//  ---- UDP server setup Parameters ----
const char* ssid = "MINISCOPE_ESP32";
const char* password = "12345678";
WiFiUDP udp;
const int localPort = 4210;
IPAddress remoteIP;
uint16_t remotePort;
bool clientKnown = false;

// #define BUFFER_SIZE (1 * 1000 * 1000 * 2)  // 6 MB for 3MSPS * 2 bytes/sample
// #define BUFFER_SIZE (1 * 1000 * 2)  // 2 MB for 3MSPS * 2 bytes/sample
// uint8_t* adcBuffer = nullptr;

// bool triggered = false;
volatile bool dataSent = false;

// --- Task handles
TaskHandle_t adcTaskHandle = NULL;
TaskHandle_t udpTaskHandle = NULL;
SemaphoreHandle_t bufferMutex;


/**
 * @brief Read a sample from the AD7276 ADC
 *        using SPI transfer.
 *
 * @return uint16_t - sample value between 0 and 4095
 */
uint16_t readADC()
{
  // CS_LOW();
  uint16_t adc_raw = SPI_ADC.transfer16(0x0000); // Send 16 dummy bits, receive ADC data
  // CS_HIGH();
  return (adc_raw >> 2) & 0x0FFF;
}

/**
 * @brief prints the captured data around the trigger point
 *        in the ring buffer.
 */
void printCapture()
{
  size_t start = (triggerIndex + BUFFER_SIZE - HALF_WINDOW) % BUFFER_SIZE;
  Serial.println("=== Triggered Window (raw values) ===");
  for (size_t i = 0; i < BUFFER_SIZE; ++i)
  {
    size_t idx = (start + i) % BUFFER_SIZE;
    Serial.print(ringBuffer[idx]);
    Serial.print(",");
  }
  Serial.println("=== End Window ===");
}

void resetADC()
{  
      xSemaphoreTake(bufferMutex, portMAX_DELAY);
      button_pressed = true;  // Set flag to prevent multiple resets
      triggered = false;
      captureReady = false;
      pre_trigger_samples = 0; // reset pre-trigger samples
      samplesAfterTrigger = 0;
      triggerIndex = 0;
      writeIndex = 0;  // Reset write index
      memset(ringBuffer, 0, BUFFER_SIZE * sizeof(uint16_t));
      Serial.println("Manual trigger reset! Waiting for next signal...");
      delay(500);              // Debounce delay
      button_pressed = false;  // Reset button pressed flag
      xSemaphoreGive(bufferMutex);

    }


// ADC task for reading samples - this was the main loop in the previous version
void adcTask(void *pvParameters) {
  esp_task_wdt_deinit();
  while(true){
      
    uint16_t sample = readADC();
    xSemaphoreTake(bufferMutex, portMAX_DELAY);
    ringBuffer[writeIndex] = sample;
    xSemaphoreGive(bufferMutex);
    
    if (pre_trigger_samples < HALF_WINDOW)
    {
      pre_trigger_samples++;
    }


  if (!triggered && sample > SAMPLE_T && pre_trigger_samples >= HALF_WINDOW)
    {
      triggered = true;
      triggerIndex = writeIndex;
      pre_trigger_samples = 0; // reset pre-trigger samples
      samplesAfterTrigger = 0;
      Serial.printf("Trigger at idx=%u raw=%u\n", (unsigned)triggerIndex, sample);
    }

    if (triggered && !captureReady) {
      samplesAfterTrigger++;
      if (samplesAfterTrigger >= HALF_WINDOW) { 
        captureReady = true;
        Serial.println("Capture ready: pre-trigger + post-trigger window complete.");
        printCapture();

      }
    }
    
    writeIndex = (writeIndex + 1) % BUFFER_SIZE;

    if (button_pressed) { 
      resetADC();
     }
  }
   vTaskDelay(1);
}

// -------------- UDP task for sending data --------------
// this is the last working firmware to handle the UDP server communication with the app.py server - 15/07/2025


// void fillBuffer() {
//   Serial.println("getting data buffer with actual data...");
//   xSemaphoreTake(bufferMutex, portMAX_DELAY);
//   for (size_t i = 0; i < BUFFER_SIZE * 2; i += 2) {
//     uint16_t sample = (i / 2) % 4096;  // ramp 0–4095
//     // uint16_t sample = ringBuffer[i / 2];  // use the ring buffer data
//     adcBuffer[i] = sample & 0xFF;        // LSB
//     adcBuffer[i+1] = (sample >> 8) & 0xFF; // MSB
//   }
//   Serial.println("Buffer filled");
//   xSemaphoreGive(bufferMutex);
// }

void udpSetup() {
  // Serial.begin(115200);
  delay(1000);
  Serial.println("Starting MiniScope UDP Server...");

  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Access Point IP address: ");
  Serial.println(IP);

  udp.begin(localPort);
  Serial.printf("UDP server listening on port %d\n", localPort);
}

void udpTask(void *pvParameters) {
    while(true){
 
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
      char incoming[packetSize + 1];
      udp.read(incoming, packetSize);
      incoming[packetSize] = '\0';

      remoteIP = udp.remoteIP();
      remotePort = udp.remotePort();
      clientKnown = true;

      Serial.printf("Received command: '%s' from %s:%d\n", incoming, remoteIP.toString().c_str(), remotePort);

      if (strcmp(incoming, "t") == 0 && !triggered){ //Trigger
        Serial.println("Trigger activated - filling buffer");
        // fillBuffer();
        triggered = true;
        dataSent = false;
        Serial.println("Trigger activated - ready to send data");
      }
      else if ((digitalRead(TRIGGER_PIN) == LOW && !button_pressed) || strcmp(incoming, "r") == 0) { // Reset
        resetADC();
        dataSent = false;
        Serial.println("Reset command received - ready for new trigger");
      }
      else{
        Serial.printf("Unknown command: '%s'\n", incoming);
      }
    }

    if (clientKnown && triggered && !dataSent) {
      const size_t chunkSize = 1024;
      size_t sent = 0;

      Serial.println("Sending data buffer over UDP...");
      

      size_t totalBytes = BUFFER_SIZE * sizeof(uint16_t);
      xSemaphoreTake(bufferMutex, portMAX_DELAY);
      while (sent < totalBytes) {
        Serial.print(".");

        size_t toSend = min(chunkSize, totalBytes - sent);
        udp.beginPacket(remoteIP, remotePort);
        udp.write((uint8_t*)ringBuffer + sent, toSend);
        udp.endPacket();
        sent += toSend;
        delayMicroseconds(200);  // tweak as needed

        }
        xSemaphoreGive(bufferMutex);

      // while (sent < BUFFER_SIZE) {
      //   Serial.print(".");
      //   size_t toSend = min(chunkSize, BUFFER_SIZE - sent);

      //   udp.beginPacket(remoteIP, remotePort);
      //   udp.write(ringBuffer + sent, toSend);
      //   udp.endPacket();
      //   sent += toSend;
      //   delayMicroseconds(200);  // tweak as needed
      // }
      Serial.printf("Finished sending %u bytes over UDP\n", BUFFER_SIZE);
      dataSent = true;
    }
      vTaskDelay(1); // Yield to other tasks
  }
}


// -------------- Main setup and Tasks Beginning --------------
void setup() {

  Serial.begin(115200);
  SPI_ADC.begin(SCLK_PIN, MISO_PIN, -1, CS_PIN);
  SPI_ADC.setHwCs(1);
  SPI_ADC.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE0));  // 20 MHz, Mode 0 - best results, try 40Mhz. tried 40Mhz and Mode 1
  delay(1000);  // wait for serial to be ready
  Serial.println("allocating ring buffer");
  Serial.printf("Free PSRAM before: %d bytes\n", ESP.getFreePsram());
  ringBuffer = (uint16_t*)ps_malloc(BUFFER_SIZE * sizeof(uint16_t));
  Serial.printf("Free PSRAM after: %d bytes\n", ESP.getFreePsram());
  if (!ringBuffer) {
    Serial.println("Failed to allocate ring buffer in PSRAM!");
  }
    // zero ring so unwritten region is known
  memset(ringBuffer, 0, BUFFER_SIZE * sizeof(uint16_t));
  
  // --- setup trigger pin ---
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  udpSetup();  // setup UDP server

  Serial.println("---finished setup---");
  // Create a mutex for buffer access
  bufferMutex = xSemaphoreCreateMutex();
  if (!bufferMutex) {
    Serial.println("Failed to create buffer mutex!");
    while(true);
  }
  
  Serial.println("--- starting ADC and UDP tasks ---");
  // esp_task_wdt_deinit();
  disableCore0WDT();
  xTaskCreatePinnedToCore(adcTask, "ADC Task", 4096, NULL, 1, &adcTaskHandle, 0); // Core 0
  xTaskCreatePinnedToCore(udpTask, "UDP Task", 8192, NULL, 1, &udpTaskHandle, 1); // Core 1
}
// empty loop for the main sketch
void loop() {
  delay(1000);
}