

// /* 07/08/2025 - multi-task bare minumin ADC Sampling and printing
//     - fastest sampling logic - bare minimum
//     - 1.27MSPS
//     - double buffer for printing , both buffers in PSRAM
//     - manual trigger reset button
//     - formated printing in chunks, no grubled data
//     - using semphores only when capture ready coping and printing
//     - core1 : ADC sampling coping when capture ready
//     - core0 : printing
// */
// #include <Arduino.h>
// #include <SPI.h>
// #include "soc/spi_struct.h"
// #include "esp_task_wdt.h" // for esp_task_wdt_deinit() function
// #include <WiFi.h> // for udp server setup
// #include <WiFiUdp.h>

// #define CS_PIN 10
// #define SCLK_PIN 12
// #define MISO_PIN 13
// #define VREF 3.3
// #define TRIGGER_PIN 0 // Pin for manually triggering

// #define BUFFER_SIZE (1 * 1000)         // 1M samples for MVP - CHANGE if NEEDED - chaged to 1000
// #define SAMPLE_RATE 3000000            // 3 MSPS
// #define SAMPLE_T 2000                  // trigger voltage threshold - CHANGE if NEEDED
// #define CAPTURE_SIZE (BUFFER_SIZE / 2) // total samples to save on trigger
// #define SPI_HZ 48000000                // SPI clock speed

// // SPI defines
// #define MSB_16_SET(var, val)                                     \
//     {                                                            \
//         (var) = (((val) & 0xFF00) >> 8) | (((val) & 0xFF) << 8); \
//     }
// #define SPI2 ((spi_dev_t *)DR_REG_SPI2_BASE)

// // use manually toggeling cs insted of  SPI_ADC.setHwCs(1); in setup so it will be faster
// #define CS_LOW() GPIO.out_w1tc = (1 << CS_PIN)  // Clear bit
// #define CS_HIGH() GPIO.out_w1ts = (1 << CS_PIN) // Set bit

// uint16_t *ringBuffer;
// volatile size_t writeIndex = 0;
// volatile bool triggered = false;
// volatile bool captureReady = false;
// int pre_trigger_samples = 0;
// size_t triggerIndex = 0;
// size_t samplesAfterTrigger = 0;
// const size_t HALF_WINDOW = BUFFER_SIZE / 2;
// bool button_pressed = false; // for manual trigger reset
// volatile uint16_t data;
// SPIClass SPI_ADC(FSPI); // Use FSPI for ESP32-S3

// volatile bool printed = false;   // Flag to indicate if capture has been printed
// uint16_t *printingBuffer = NULL; // Buffer for printing capture data

// // --- Task handles ---
// TaskHandle_t adcTaskHandle = NULL;
// TaskHandle_t udpTaskHandle = NULL;
// TaskHandle_t printingTaskHandle = NULL;
// SemaphoreHandle_t bufferMutex;

// // --- Global variables for UDP ---
// const char* ssid = "MINISCOPE_ESP32";
// const char* password = "12345678";
// WiFiUDP udp;
// const int localPort = 4210;
// IPAddress remoteIP;
// uint16_t remotePort;
// bool clientKnown = false;
// volatile bool dataSent = false;

// // --- function declerations ---
// void printCapture();
// void printingTask(void *pvParameters);
// void udpSetup(); // UDP functions
// void udpTask(void *pvParameters);


// /**
//  * @brief the main ADC task for reading samples
//  * running on core1 for best results
//  * handles ADC sampling, trigger detection, and buffer management.
//  * 
//  * @param pvParameters 
//  * keep NULL as called on setup
//  */
// void IRAM_ATTR adcTask(void *pvParameters)
// {
//     // esp_task_wdt_deinit();
//     delay(1000); // Allow time for Serial to initialize
//     Serial.print("starting ADC Task on core: ");
//     Serial.println(xPortGetCoreID());
//     while (true)
//     {
//         SPI2->cmd.usr = 1;
//         // while(SPI2->cmd.usr);
//         data = SPI2->data_buf[0] & 0xFFFF;

//         // add a flag that knows when the pre-trigger samples are ready
//         // uint16_t sample = readADC();
//         MSB_16_SET(data, data);
//         data = (data >> 2) & 0x0FFF;

//         ringBuffer[writeIndex] = data;

//         if (pre_trigger_samples < HALF_WINDOW)
//         {
//             pre_trigger_samples++;
//         }

//         if (!triggered && data > SAMPLE_T && pre_trigger_samples >= HALF_WINDOW)
//         {
//             triggered = true;
//             triggerIndex = writeIndex;
//             pre_trigger_samples = 0; // reset pre-trigger samples
//             samplesAfterTrigger = 0;
//             Serial.printf("Trigger at idx=%u raw=%u\n", (unsigned)triggerIndex, data);
//         }

//         if (triggered && !captureReady)
//         {
//             samplesAfterTrigger++;
//             if (samplesAfterTrigger >= HALF_WINDOW)
//             {
//                 captureReady = true;
//                 Serial.println("Capture ready: pre-trigger + post-trigger window complete.");
//                 xSemaphoreTake(bufferMutex, portMAX_DELAY);
//                 memcpy(printingBuffer, ringBuffer, BUFFER_SIZE * sizeof(uint16_t));
//                 xSemaphoreGive(bufferMutex);
//                 delay(1000); // Allow time for Serial to print
//                 // printCapture();

//                 // send over UDP in future
//             }
//         }

//         writeIndex = (writeIndex + 1) % BUFFER_SIZE;

//         // Manual trigger reset button - after pressing waiting for (Voltage > V_T) sample > SAMPLE_T
//         if (digitalRead(TRIGGER_PIN) == LOW && !button_pressed)
//         {
//             xSemaphoreTake(bufferMutex, portMAX_DELAY);
//             button_pressed = true; // Set flag to prevent multiple resets
//             triggered = false;
//             captureReady = false;
//             pre_trigger_samples = 0;
//             samplesAfterTrigger = 0;
//             triggerIndex = 0;
//             writeIndex = 0; // Reset write index - needed?
//             memset(ringBuffer, 0, BUFFER_SIZE * sizeof(uint16_t));
//             memset(printingBuffer, 0, BUFFER_SIZE * sizeof(uint16_t)); // Clear printing buffer
//             Serial.println("Manual trigger reset! Waiting for next signal...");
//             delay(500);             // Debounce delay
//             button_pressed = false; // Reset button pressed flag
//             printed = false;        // Reset printed flag
//             Serial.println("reset bools done, waiting for next trigger...");
//             delay(500); // Allow time for Serial to print
//             xSemaphoreGive(bufferMutex);
//         }
//     }
// }
// void setup()
// {
//     Serial.begin(115200);         // Initialize Serial at 115200 baud
//     Serial.setTxBufferSize(1024); // increase buffer sizes for printing purposes, default is 265
//     Serial.setRxBufferSize(1024); // increase buffer sizes for printing purposes, default is 265
//     delay(1000);
//     Serial.println("---Starting setup---");
//     pinMode(CS_PIN, OUTPUT);
//     digitalWrite(CS_PIN, HIGH);
//     SPI_ADC.begin(SCLK_PIN, MISO_PIN, -1, CS_PIN);
//     SPI_ADC.setHwCs(1);
//     // Serial.println("SPI initialized using manual toggeling CS");
//     // Serial.print("initiating SPI with ADC at speed:");
//     // Serial.println(SPI_HZ);
//     // Serial.println("SPI initialized using HwCs");

//     // ---  Minimal SPI ADC communication setup ---
//     SPI_ADC.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE0)); // 40 MHz, Mode 0 - best results, try 40Mhz. tried 40Mhz and Mode 1

//     SPI2->ms_dlen.ms_data_bitlen = 15;
//     SPI2->cmd.update = 1;
//     while (SPI2->cmd.update)
//     delay(1000); // wait for serial to be ready
//     // --- Buffer Setup ---
//     Serial.println("allocating ring buffer...");
//     // ringBuffer = (uint16_t *)malloc(BUFFER_SIZE * sizeof(uint16_t));
//     ringBuffer = (uint16_t *)ps_malloc(BUFFER_SIZE * sizeof(uint16_t));
//     if (!ringBuffer)
//     {
//         Serial.println("Failed to allocate ring buffer in PSRAM!");
//     }
//     memset(ringBuffer, 0, BUFFER_SIZE * sizeof(uint16_t));

//     // allocating printBuffer
//     Serial.println("allocating printing buffer...");
//     printingBuffer = (uint16_t *)ps_malloc(BUFFER_SIZE * sizeof(uint16_t));
//     if (!printingBuffer)
//     {
//         Serial.println("Failed to allocate Printing buffer in PSRAM!");
//     }
//     memset(printingBuffer, 0, BUFFER_SIZE * sizeof(uint16_t));

//     // Create a mutex for buffer only used when coping data to printingBuffer
//     bufferMutex = xSemaphoreCreateMutex();
//     if (!bufferMutex)
//     {
//         Serial.println("Failed to create buffer mutex!");
//         while (true);
//     }
//     Serial.println("bufferMutex created successfully");

//     // --- setup trigger pin ---
//     pinMode(TRIGGER_PIN, INPUT_PULLUP);

//     udpSetup(); // setup UDP server
//     Serial.println("UDP setup done");

//     Serial.println("---finished setup---");

//     esp_task_wdt_deinit(); // Disable Task Watchdog for all tasks

//     disableCore0WDT();                                                                             // disabling the watch dog for core 0, since we are using it for the ADC task
//     xTaskCreatePinnedToCore(adcTask, "ADC Task", 4096, NULL, 3, &adcTaskHandle, 1);                // Core 1
//     xTaskCreatePinnedToCore(printingTask, "Printing Task", 4096, NULL, 1, &printingTaskHandle, 0); // Core 0
//     xTaskCreatePinnedToCore(udpTask, "UDP Task", 4096, NULL, 1, &udpTaskHandle, 0); // Core 0
// }

// /**
//  * @brief Read a sample from the AD7276 ADC
//  *        using SPI transfer.
//  *
//  * @return uint16_t - sample value between 0 and 4095
//  */
// inline uint16_t readADC()
// {
//     // uint16_t adc_raw = SPI_ADC.transfer16(0x0000); // Send 16 dummy bits, receive ADC data

//     SPI2->data_buf[0] = 0x0000;
//     SPI2->cmd.usr = 1;
//     // while (SPI2->cmd.usr);
//     data = SPI2->data_buf[0] & 0xFFFF;
//     return data;
// }

// /**
//  * @brief prints the captured data around the trigger point
//  *        in the ring buffer.
//  */
// // void printCapture()
// // {
// //     xSemaphoreTake(bufferMutex, portMAX_DELAY);
// //     Serial.print("Printing Capture from core: ");
// //     Serial.println(xPortGetCoreID());
// //     delay(100); // Allow time for Serial to print
// //     size_t start = (triggerIndex + BUFFER_SIZE - HALF_WINDOW) % BUFFER_SIZE;
// //     Serial.println("=== Triggered Window (raw values) ===");
// //     for (size_t i = 0; i < BUFFER_SIZE; ++i)
// //     {
// //         size_t idx = (start + i) % BUFFER_SIZE;
// //         Serial.print(printingBuffer[idx]);
// //         if (printingBuffer[idx] > 4095)
// //         {
// //             Serial.print("[ERR]");
// //         }
// //         Serial.print(", ");
// //         delay(2);
// //     }
// //     Serial.println("=== End Window ===");
// //     xSemaphoreGive(bufferMutex);
// // }

// // debugging printCapture function
// void printCapture()
// {
//     xSemaphoreTake(bufferMutex, portMAX_DELAY);

//     char buffer[128];
//     size_t start = (triggerIndex + BUFFER_SIZE - HALF_WINDOW) % BUFFER_SIZE;

//     // Print header once with longer delay
//     Serial.println(F("\n----START DEBUG INFO----"));
//     Serial.printf("Core: %d, Trigger: %d, Size: %d\n",
//                   xPortGetCoreID(), triggerIndex, BUFFER_SIZE);
//     delay(10);
//     Serial.flush();
//     Serial.println(F("=== Triggered Window (raw values) ==="));
//     Serial.flush();
//     delay(50); // One longer delay after header

//     // Print 10 values per line with fewer Serial calls
//     for (size_t i = 0; i < BUFFER_SIZE; i += 10)
//     {
//         int len = 0;
//         for (size_t j = 0; j < 10 && (i + j) < BUFFER_SIZE; j++)
//         {
//             size_t idx = (start + i + j) % BUFFER_SIZE;
//             if (printingBuffer[idx] > 4095)
//             {
//                 Serial.print("[ERR]");
//             }
//             len += sprintf(buffer + len, "%d, ", printingBuffer[idx]); // Changed format to use comma + space
//         }
//         buffer[len] = '\n';
//         buffer[len + 1] = '\0';

//         Serial.print(buffer);
//         delay(20); // Larger delay between lines, but fewer total delays
//     }
//     delay(20);
//     Serial.println(F("=== End Window ==="));
//     delay(20);
//     Serial.flush();

//     xSemaphoreGive(bufferMutex);
// }
// void loop()
// {
//     // delay(10000); // Just to avoid busy loop, can be removed or adjusted
//     // // Main loop does nothing, all work is done in adcTask
//     // // This is to keep the main loop responsive and allow other tasks to run
//     // // If you want to add more functionality, you can do so here
//     // Serial.print("main loop on core: ");
//     // Serial.println(xPortGetCoreID());
//     // if(captureReady)
//     // {
//     //     Serial.println("Capture found on loop!");
//     //     delay(1000); // Allow time for Serial to print

//     // }
// }

// void printingTask(void *pvParameters)
// {
//     delay(1000); // Allow time for Serial to initialize
//     Serial.println("Printing Task started");
//     while (1)
//     {
//         if (captureReady && !printed)
//         {
//             printed = true; // Set flag to indicate capture has been printed
//             printCapture();
//         }
//         vTaskDelay(1000 / portTICK_PERIOD_MS);
//     }
// }

// // - printing debugging helper functions:

// // Add these helper functions
// void clearSerialBuffer()
// {
//     while (Serial.available())
//     {
//         Serial.read();
//     }
// }

// bool isSerialBufferFull()
// {
//     return Serial.availableForWrite() == 0;
// }

// void waitForSerialToClear()
// {
//     Serial.flush();
//     delay(10);
//     clearSerialBuffer();
// }

// void resetADC(){
//             button_pressed = true; // Set flag to prevent multiple resets
//             triggered = false;
//             captureReady = false;
//             pre_trigger_samples = 0;
//             samplesAfterTrigger = 0;
//             triggerIndex = 0;
//             writeIndex = 0; // Reset write index - needed?
//             memset(ringBuffer, 0, BUFFER_SIZE * sizeof(uint16_t));
//             memset(printingBuffer, 0, BUFFER_SIZE * sizeof(uint16_t)); // Clear printing buffer
//             Serial.println("Manual trigger reset! Waiting for next signal...");
//             delay(500);             // Debounce delay
//             button_pressed = false; // Reset button pressed flag
//             printed = false;        // Reset printed flag
//             Serial.println("reset bools done, waiting for next trigger...");
//             delay(500); // Allow time for Serial to print
// }
// // --- UDP SERVER FUNCTIONS ---


// void udpSetup() {
//   // Serial.begin(115200);
//   delay(1000);
//   Serial.println("Starting MiniScope UDP Server...");

//   WiFi.softAP(ssid, password);
//   IPAddress IP = WiFi.softAPIP();
//   Serial.print("Access Point IP address: ");
//   Serial.println(IP);

//   udp.begin(localPort);
//   Serial.printf("UDP server listening on port %d\n", localPort);
// }

// // void udpTask(void *pvParameters) {
// //     while(true){
 
// //     int packetSize = udp.parsePacket();
// //     if (packetSize > 0) {
// //       char incoming[packetSize + 1];
// //       udp.read(incoming, packetSize);
// //       incoming[packetSize] = '\0';

// //       remoteIP = udp.remoteIP();
// //       remotePort = udp.remotePort();
// //       clientKnown = true;

// //       Serial.printf("Received command: '%s' from %s:%d\n", incoming, remoteIP.toString().c_str(), remotePort);

// //       if (strcmp(incoming, "t") == 0 && !triggered){ //Trigger
// //         Serial.println("Trigger activated - filling buffer");
// //         // fillBuffer();
// //         triggered = true;
// //         dataSent = false;
// //         Serial.println("Trigger activated - ready to send data");
// //       }
// //       else if ((digitalRead(TRIGGER_PIN) == LOW && !button_pressed) || strcmp(incoming, "r") == 0) { // Reset
// //         xSemaphoreTake(bufferMutex, portMAX_DELAY);
// //         resetADC();
// //         dataSent = false;
// //         xSemaphoreGive(bufferMutex);
// //         Serial.println("Reset command received - ready for new trigger");
// //       }
// //       else{
// //         Serial.printf("Unknown command: '%s'\n", incoming);
// //       }
// //     }

// //     if (clientKnown && triggered && !dataSent) {
// //       const size_t chunkSize = 1024;
// //       size_t sent = 0;

// //       Serial.println("Sending data buffer over UDP...");
      

// //       size_t totalBytes = BUFFER_SIZE * sizeof(uint16_t);
// //       xSemaphoreTake(bufferMutex, portMAX_DELAY);
// //       while (sent < totalBytes) {
// //         Serial.print(".");

// //         size_t toSend = min(chunkSize, totalBytes - sent);
// //         udp.beginPacket(remoteIP, remotePort);
// //         udp.write((uint8_t*)ringBuffer + sent, toSend);
// //         udp.endPacket();
// //         sent += toSend;
// //         delayMicroseconds(200);  // tweak as needed

// //         }
// //         xSemaphoreGive(bufferMutex);

// //       // while (sent < BUFFER_SIZE) {
// //       //   Serial.print(".");
// //       //   size_t toSend = min(chunkSize, BUFFER_SIZE - sent);

// //       //   udp.beginPacket(remoteIP, remotePort);
// //       //   udp.write(ringBuffer + sent, toSend);
// //       //   udp.endPacket();
// //       //   sent += toSend;
// //       //   delayMicroseconds(200);  // tweak as needed
// //       // }
// //       Serial.printf("Finished sending %u bytes over UDP\n", BUFFER_SIZE);
// //       dataSent = true;
// //     }
// //       vTaskDelay(1); // Yield to other tasks
// //   }
// // }