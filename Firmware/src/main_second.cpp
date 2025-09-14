

/* 04/09/2025 - ADC Sampling with Serial print, and app communication
    - fastest sampling logic - bare minimum
    - 1.212MSPS - TODO: Verify sampling speed after added commands
    - double buffer for printing , both buffers in PSRAM
    - manual trigger reset button
    - formated printing in chunks, no grubled data
    - using semphores only when capture ready copying and printing
    - TEST: testing now for larger buffer size (10k)
    - added trigger type (rising/falling) and threshold setting over serial
 *
*/

#include <Arduino.h>
#include <SPI.h>
#include "soc/spi_struct.h"
#include "esp_task_wdt.h" // for esp_task_wdt_deinit() function

#define DEBUG 1 // 1 - Debug prints enabled, 0 - Debug prints disabled
#define CS_PIN 10
#define SCLK_PIN 12
#define MISO_PIN 13
#define VREF 3.3
#define RESET_TRIGGER_PIN 0 // Pin for manually triggering

#define BUFFER_SIZE (1 * 1000 * 10)    // 1M samples for MVP - CHANGE if NEEDED - changed to 10K
#define CAPTURE_SIZE (BUFFER_SIZE / 2) // total samples to save on trigger
#define SPI_HZ 48000000                // SPI clock speed

// SPI defines
#define MSB_16_SET(var, val)                                     \
    {                                                            \
        (var) = (((val) & 0xFF00) >> 8) | (((val) & 0xFF) << 8); \
    }
#define SPI2 ((spi_dev_t *)DR_REG_SPI2_BASE)

// use manually toggeling cs insted of  SPI_ADC.setHwCs(1); in setup so it will be faster
#define CS_LOW() GPIO.out_w1tc = (1 << CS_PIN)  // Clear bit
#define CS_HIGH() GPIO.out_w1ts = (1 << CS_PIN) // Set bit

uint16_t *ringBuffer;
volatile size_t writeIndex = 0;
volatile bool triggered = false;
volatile bool captureReady = false;
int pre_trigger_samples = 0;
size_t triggerIndex = 0;
size_t samplesAfterTrigger = 0;
const size_t HALF_WINDOW = BUFFER_SIZE / 2;
bool button_pressed = false; // for manual trigger reset
volatile uint16_t data;
SPIClass SPI_ADC(FSPI); // Use FSPI for ESP32-S3

volatile int sample_v_threshold = 0;
volatile bool serialTriggerRequested = false;
volatile bool printed = false;   // Flag to indicate if capture has been printed
uint16_t *printingBuffer = NULL; // Buffer for printing capture data

// --- Task handles ---
TaskHandle_t adcTaskHandle = NULL;
TaskHandle_t udpTaskHandle = NULL;
TaskHandle_t printingTaskHandle = NULL;
SemaphoreHandle_t bufferMutex;

// --- function declerations ---
void printPreTriggerCapture();
void printCapture();
void printingTask(void *pvParameters);

// --- Trigger type definitions ---
enum TriggerType
{
    TRIGGER_RISING = 0,
    TRIGGER_FALLING = 1,
};

volatile TriggerType triggerType = TRIGGER_RISING; // default = rising

void IRAM_ATTR adcTask(void *pvParameters)
{
    // esp_task_wdt_deinit();
    delay(1000); // Allow time for Serial to initialize
    Serial.print("starting ADC Task on core: ");
    Serial.println(xPortGetCoreID());
    while (true)
    {
        SPI2->cmd.usr = 1;
        // while(SPI2->cmd.usr);
        data = SPI2->data_buf[0] & 0xFFFF;

        // add a flag that knows when the pre-trigger samples are ready
        // uint16_t sample = readADC();
        MSB_16_SET(data, data);
        data = (data >> 2) & 0x0FFF;

        ringBuffer[writeIndex] = data;

        if (pre_trigger_samples < HALF_WINDOW)
        {
            pre_trigger_samples++;
        }

        if (!triggered && pre_trigger_samples >= HALF_WINDOW &&
            ((triggerType == TRIGGER_RISING && data > sample_v_threshold) || (triggerType == TRIGGER_FALLING && data < sample_v_threshold)))
        {
            triggered = true;
            triggerIndex = writeIndex;
            pre_trigger_samples = 0; // reset pre-trigger samples
            samplesAfterTrigger = 0;
            if (DEBUG)
            {
                Serial.printf("Trigger at idx=%u raw=%u\n", (unsigned)triggerIndex, data);
            }
        }

        if (triggered && !captureReady)
        {
            samplesAfterTrigger++;
            if (samplesAfterTrigger >= HALF_WINDOW)
            {
                captureReady = true;
                Serial.println("Capture ready: pre-trigger + post-trigger window complete.");
                Serial.print("TRIGGER TYPE IS: ");
                Serial.println(triggerType == TRIGGER_RISING ? "RISING" : "FALLING");
                Serial.print("Sample threshold is: ");
                Serial.println(sample_v_threshold);
                xSemaphoreTake(bufferMutex, portMAX_DELAY);
                memcpy(printingBuffer, ringBuffer, BUFFER_SIZE * sizeof(uint16_t)); // TODO: check how much time it takes
                // printed = true; // Set flag to indicate capture has been printed
                // printPreTriggerCapture();
                // printCapture();
                xSemaphoreGive(bufferMutex);
                // delay(5000); // Allow time for Serial to print
                // printCapture();

                // send over UDP in future
            }
        }

        writeIndex = (writeIndex + 1) % BUFFER_SIZE;

        // Manual trigger reset button - after pressing waiting for (Voltage > V_T) sample > SAMPLE_T
        if (digitalRead(RESET_TRIGGER_PIN) == LOW && !button_pressed || serialTriggerRequested)
        {
            xSemaphoreTake(bufferMutex, portMAX_DELAY);
            button_pressed = true; // Set flag to prevent multiple resets
            serialTriggerRequested = false;
            triggered = false;
            captureReady = false;
            pre_trigger_samples = 0;
            samplesAfterTrigger = 0;
            triggerIndex = 0;
            writeIndex = 0; // Reset write index - needed?
            memset(ringBuffer, 0, BUFFER_SIZE * sizeof(uint16_t));
            memset(printingBuffer, 0, BUFFER_SIZE * sizeof(uint16_t)); // Clear printing buffer
            Serial.println("Manual trigger reset! Waiting for next signal...");
            delay(500);             // Debounce delay
            button_pressed = false; // Reset button pressed flag
            printed = false;        // Reset printed flag
            Serial.println("reset bools done, waiting for next trigger...");
            delay(500); // Allow time for Serial to print
            xSemaphoreGive(bufferMutex);
        }
    }
}
void setup()
{
    Serial.setTxBufferSize(32768); // increase buffer sizes for printing purposes, default is 265
    Serial.setRxBufferSize(8192);  // increase buffer sizes for printing purposes, default is 265
    Serial.begin(921600);          // Initialize Serial at 115200 baud - for statistics acquisition check if 921600 works better
    // Serial.setTxBufferSize(1024); // increase buffer sizes for printing purposes, default is 265
    // Serial.setRxBufferSize(1024); // increase buffer sizes for printing purposes, default is 265
    delay(1000);
    Serial.println("---Starting setup---");
    pinMode(CS_PIN, OUTPUT);
    digitalWrite(CS_PIN, HIGH);
    SPI_ADC.begin(SCLK_PIN, MISO_PIN, -1, CS_PIN);
    SPI_ADC.setHwCs(1);
    // Serial.println("SPI initialized using manual toggeling CS");
    // Serial.print("initiating SPI with ADC at speed:");
    // Serial.println(SPI_HZ);
    // Serial.println("SPI initialized using HwCs");

    // ---  Minimal SPI ADC communication setup ---
    SPI_ADC.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE0)); // 40 MHz, Mode 0 - best results, try 40Mhz. tried 40Mhz and Mode 1

    SPI2->ms_dlen.ms_data_bitlen = 15;
    SPI2->cmd.update = 1;
    while (SPI2->cmd.update)
        delay(1000); // wait for serial to be ready
    Serial.println("allocating ring buffer");
    // ringBuffer = (uint16_t *)malloc(BUFFER_SIZE * sizeof(uint16_t));
    ringBuffer = (uint16_t *)ps_malloc(BUFFER_SIZE * sizeof(uint16_t));
    if (!ringBuffer)
    {
        Serial.println("Failed to allocate ring buffer in PSRAM!");
    }
    memset(ringBuffer, 0, BUFFER_SIZE * sizeof(uint16_t));

    // allocating printBuffer
    printingBuffer = (uint16_t *)ps_malloc(BUFFER_SIZE * sizeof(uint16_t));
    if (!printingBuffer)
    {
        Serial.println("Failed to allocate Printing buffer in PSRAM!");
    }
    memset(printingBuffer, 0, BUFFER_SIZE * sizeof(uint16_t));

    // Create a mutex for buffer only used when coping data to printingBuffer
    bufferMutex = xSemaphoreCreateMutex();
    if (!bufferMutex)
    {
        Serial.println("Failed to create buffer mutex!");
        while (true)
            ;
    }
    Serial.println("bufferMutex created successfully");

    // --- setup trigger pin ---
    pinMode(RESET_TRIGGER_PIN, INPUT_PULLUP);

    Serial.println("---finished setup---");

    esp_task_wdt_deinit(); // Disable Task Watchdog for all tasks

    disableCore0WDT();                                                              // disabling the watch dog for core 0, since we are using it for the ADC task
    xTaskCreatePinnedToCore(adcTask, "ADC Task", 4096, NULL, 3, &adcTaskHandle, 1); // Core 1
    xTaskCreatePinnedToCore(printingTask, "Printing Task", 4096, NULL, 1, &printingTaskHandle, 0); // Core 0
}

/**
 * @brief Read a sample from the AD7276 ADC
 *        using SPI transfer.
 *
 * @return uint16_t - sample value between 0 and 4095
 */
inline uint16_t readADC()
{
    // uint16_t adc_raw = SPI_ADC.transfer16(0x0000); // Send 16 dummy bits, receive ADC data

    SPI2->data_buf[0] = 0x0000;
    SPI2->cmd.usr = 1;
    // while (SPI2->cmd.usr);
    data = SPI2->data_buf[0] & 0xFFFF;
    return data;
}

/**
 * @brief prints the captured data around the trigger point
 *        in the ring buffer.
 */
// void printCapture()
// {
//     xSemaphoreTake(bufferMutex, portMAX_DELAY);
//     Serial.print("Printing Capture from core: ");
//     Serial.println(xPortGetCoreID());
//     delay(100); // Allow time for Serial to print
//     size_t start = (triggerIndex + BUFFER_SIZE - HALF_WINDOW) % BUFFER_SIZE;
//     Serial.println("=== Triggered Window (raw values) ===");
//     for (size_t i = 0; i < BUFFER_SIZE; ++i)
//     {
//         size_t idx = (start + i) % BUFFER_SIZE;
//         Serial.print(printingBuffer[idx]);
//         if (printingBuffer[idx] > 4095)
//         {
//             Serial.print("[ERR]");
//         }
//         Serial.print(", ");
//         delay(2);
//     }
//     Serial.println("=== End Window ===");
//     xSemaphoreGive(bufferMutex);
// }

void printPreTriggerCapture()
{
    // xSemaphoreTake(bufferMutex, portMAX_DELAY);

    size_t start = (triggerIndex + BUFFER_SIZE - HALF_WINDOW) % BUFFER_SIZE;

    Serial.println(F("START"));
    for (size_t i = 0; i < HALF_WINDOW; i++)
    {
        size_t idx = (start + i) % BUFFER_SIZE;
        Serial.print(ringBuffer[idx]);
        delay(2);
        Serial.print(", ");
        delay(2);
    }
    Serial.println(F("\nEND"));

    // xSemaphoreGive(bufferMutex);
}

// debugging printCapture function
void printCapture()
{
    xSemaphoreTake(bufferMutex, portMAX_DELAY);

    char buffer[256];
    size_t start = (triggerIndex + BUFFER_SIZE - HALF_WINDOW) % BUFFER_SIZE;

    if (DEBUG)
    {
        Serial.println(F("\n----DEBUG INFO----"));
        Serial.printf("Core: %d, Trigger: %d, Size: %d\n",
                      xPortGetCoreID(), triggerIndex, BUFFER_SIZE);
    }

    delay(10);
    Serial.println(F("START"));
    delay(20); // short delay after header

    // Print 10 values per line
    for (size_t i = 0; i < BUFFER_SIZE; i += 10)
    {
        int len = 0;
        memset(buffer, 0, 256 * sizeof(char));
        for (size_t j = 0; j < 10 && (i + j) < BUFFER_SIZE; j++)
        {
            size_t idx = (start + i + j) % BUFFER_SIZE;

            if (printingBuffer[idx] > 4095)
                len += sprintf(buffer + len, "[ERR], ");
            else
                len += sprintf(buffer + len, "%d, ", printingBuffer[idx]);

            if (DEBUG && (i + j) == triggerIndex)
            {
                Serial.printf("END\n", (unsigned)triggerIndex, data);
            }
        }

        buffer[len] = '\n'; // replace last ", " with newline
        buffer[len + 1] = '\0';
        Serial.print(buffer);

        delay(5); // small delay, enough for UART buffer
    }

    Serial.println(F("END"));
    delay(10);

    xSemaphoreGive(bufferMutex);
}

// void printCapture()
// {
//     xSemaphoreTake(bufferMutex, portMAX_DELAY);

//     char buffer[256]; // was 128
//     size_t start = (triggerIndex + BUFFER_SIZE - HALF_WINDOW) % BUFFER_SIZE;

//     // Print header once with longer delay
//     if (DEBUG)
//     {
//         Serial.println(F("\n----DEBUG INFO----"));
//         Serial.printf("Core: %d, Trigger: %d, Size: %d\n",
//                       xPortGetCoreID(), triggerIndex, BUFFER_SIZE);
//     }
//     delay(10);
//     Serial.flush();
//     Serial.println(F("START"));
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

//             if (DEBUG && (i+j) == triggerIndex)
//             {
//                 Serial.printf("END\n", (unsigned)triggerIndex, data);
//             }

//             len += sprintf(buffer + len, "%d, ", printingBuffer[idx]); // Changed format to use comma + space
//         }
//         buffer[len] = '\n';
//         buffer[len + 1] = '\0';

//         Serial.print(buffer);
//         delay(20); // Larger delay between lines, but fewer total delays

//     }
//     delay(20);
//     Serial.println(F("END"));
//     delay(20);
//     Serial.flush();

//     xSemaphoreGive(bufferMutex);
// }
void loop()
{
    // delay(10000); // Just to avoid busy loop, can be removed or adjusted
    // // Main loop does nothing, all work is done in adcTask
    // // This is to keep the main loop responsive and allow other tasks to run
    // // If you want to add more functionality, you can do so here
    // Serial.print("main loop on core: ");
    // Serial.println(xPortGetCoreID());
    // if(captureReady)
    // {
    //     Serial.println("Capture found on loop!");
    //     delay(1000); // Allow time for Serial to print

    // }
}

void printingTask(void *pvParameters)
{
    delay(1000); // Allow time for Serial to initialize
    Serial.println("Printing Task started");
    while (1)
    {
        if (Serial.available())
        {
            String cmd = Serial.readStringUntil('\n'); // read line
            cmd.trim();
            if (cmd.equalsIgnoreCase("TRIGGER"))
            {
                xSemaphoreTake(bufferMutex, portMAX_DELAY);
                serialTriggerRequested = true;
                Serial.println("Serial trigger requested!");
                xSemaphoreGive(bufferMutex);
            }
            if (cmd.startsWith("V_THRESHOLD="))//V_THRESHOLD=2000
            {
                int newThresh = cmd.substring(12).toInt();

                sample_v_threshold = newThresh;
                Serial.printf("Threshold updated to %d\n", sample_v_threshold);
            }
            if (cmd.startsWith("TYPE="))// TYPE=FALLING OR TYPE=RISING
            {
                String typeStr = cmd.substring(5);
                typeStr.trim();
                if (typeStr.equalsIgnoreCase("RISING"))
                {
                    triggerType = TRIGGER_RISING;
                    Serial.println("Trigger type set to RISING edge");
                }
                else if (typeStr.equalsIgnoreCase("FALLING"))
                {
                    triggerType = TRIGGER_FALLING;
                    Serial.println("Trigger type set to FALLING edge");
                }
                else
                {
                    Serial.println("Invalid TRIGGER_TYPE command. Use RISING or FALLING.");
                }
            }
        }
        if (captureReady && !printed)
        {
            printed = true; // Set flag to indicate capture has been printed
            // printPreTriggerCapture();
            printCapture();
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// - printing debugging helper functions:

// Add these helper functions
void clearSerialBuffer()
{
    while (Serial.available())
    {
        Serial.read();
    }
}

bool isSerialBufferFull()
{
    return Serial.availableForWrite() == 0;
}

void waitForSerialToClear()
{
    Serial.flush();
    delay(10);
    clearSerialBuffer();
}
