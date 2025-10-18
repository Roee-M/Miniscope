

/* 16/10/2025 - Miniscope Presentation version
    Feature list:
    - Fastest sampling logic -cs line register level toggeling
    - 1.1594 MSPS
    - Double buffer for printing , both buffers in PSRAM, protected by semphore
    - Manual trigger reset button
    - Formated printing in chunks for reliable serial transmission
    - Using semphores only when capture ready copying and printing - not affecting window sample rate
    - Tested larger buffer size (10k) - seeing artifacts caused by manual CS toggeling also on app
    - Supporting commands over serial - Trigger type (rising/falling) and threshold setting over serial

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

#define BUFFER_SIZE (1 * 1000)    
#define SPI_HZ 48000000                // SPI clock speed

// SPI defines
#define MSB_16_SET(var, val)                                     \
    {                                                            \
        (var) = (((val) & 0xFF00) >> 8) | (((val) & 0xFF) << 8); \
    }
#define SPI2 ((spi_dev_t *)DR_REG_SPI2_BASE)

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

// --- Function declarations ---
void printPreTriggerCapture();
void printCapture();
void printingTask(void *pvParameters);

// --- Trigger type definitions ---
enum TriggerType
{
    TRIGGER_RISING = 0,
    TRIGGER_FALLING = 1,
};
volatile uint16_t prevData = 0; // for edge detection

volatile TriggerType triggerType = TRIGGER_RISING; // default = rising



/**************************************************************************************************/
/**
 * @brief ADC sampling task for Miniscope, runs on a dedicated FreeRTOS core.
 * 
 * This task continuously samples data from the AD7276 ADC via SPI using 
 * direct register access. It implements trigger-based acquisition with 
 * a pre-trigger and post-trigger window, writes samples into a ring buffer, 
 * and copies the data to a printing buffer when a trigger occurs.
 * 
 * Features:
 * - Continuous high-speed sampling (~1.16 MSPS)
 * - Rising or falling edge trigger detection
 * - Pre-trigger and post-trigger buffering
 * - Manual trigger reset via a button
 * - Serial debug output when DEBUG is enabled
 * 
 * @param pvParameters FreeRTOS task parameter (unused)
 * 
 * @note The function uses IRAM_ATTR to place it in instruction RAM for 
 *       deterministic high-speed execution.
 * 
 * @note Trigger detection uses previous sample (`prevData`) to handle edge detection.
 * 
 * @note The task runs indefinitely in a while(true) loop.
 * 
 * @return void
 */
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

        // --- Simple Threshold detection check ---
        // if (!triggered && pre_trigger_samples >= HALF_WINDOW &&
        //     ((triggerType == TRIGGER_RISING && data > sample_v_threshold) || (triggerType == TRIGGER_FALLING && data < sample_v_threshold)))
        // {
        //     triggered = true;
        //     triggerIndex = writeIndex;
        //     pre_trigger_samples = 0; // reset pre-trigger samples
        //     samplesAfterTrigger = 0;
        //     if (DEBUG)
        //     {
        //         Serial.printf("Trigger at idx=%u raw=%u\n", (unsigned)triggerIndex, data);
        //     }
        // }

        //  --- edge detection version using prevData for rising/falling ---
        if (!triggered && pre_trigger_samples >= HALF_WINDOW)
        {
            if (triggerType == TRIGGER_RISING &&
                prevData <= sample_v_threshold &&
                data > sample_v_threshold)
            {
                triggered = true;
            }
            else if (triggerType == TRIGGER_FALLING &&
                     prevData >= sample_v_threshold &&
                     data < sample_v_threshold)
            {
                triggered = true;
            }

            if (triggered)
            {
                triggerIndex = writeIndex;
                pre_trigger_samples = 0;
                samplesAfterTrigger = 0;
                if (DEBUG)
                {
                    Serial.printf("Trigger at idx=%u raw=%u\n",
                                  (unsigned)triggerIndex, data);
                }
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
                xSemaphoreGive(bufferMutex);
            }
        }

        writeIndex = (writeIndex + 1) % BUFFER_SIZE;
        prevData = data; // comment out if not needed for rising/falling detection

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
            writeIndex = 0; 
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


/**
 * @brief Initializes the Miniscope hardware and firmware environment.
 * 
 * This function performs all necessary initializations for the Miniscope,
 * including serial communication, SPI setup, PSRAM allocation, mutex creation,
 * and FreeRTOS task creation. It is called once at the beginning of execution.
 * 
 * @details
 * - Configures Serial with enlarged TX/RX buffers and baud rate 921600.
 * - Initializes SPI interface for ADC communication (AD7276) with manual CS handling.
 * - Allocates ring buffer and printing buffer in PSRAM for high-speed data storage.
 * - Creates a mutex (`bufferMutex`) for safe access to the printing buffer.
 * - Configures the manual trigger reset pin (`RESET_TRIGGER_PIN`) as INPUT_PULLUP.
 * - Disables ESP32 watchdog timers to prevent task reset during high-speed sampling.
 * - Creates two FreeRTOS tasks pinned to separate cores:
 *   - `adcTask` on Core 1: Handles high-speed ADC sampling and trigger detection.
 *   - `printingTask` on Core 0: Manages USB serial communication and data transfer to the host.
 * 
 * @note Uses `ps_malloc` for allocation in PSRAM. If allocation fails, prints error messages.
 * @note The setup ensures deterministic high-speed sampling and safe multitasking for Miniscope operation.
 * @note Must be called before starting data acquisition tasks.
 * 
 * @return void
 */
void setup()
{
    Serial.setTxBufferSize(32768); // increase buffer sizes for printing purposes, default is 265
    Serial.setRxBufferSize(8192);  // increase buffer sizes for printing purposes, default is 265
    Serial.begin(921600);          
    delay(1000);
    Serial.println("---Starting setup---");
    pinMode(CS_PIN, OUTPUT);
    digitalWrite(CS_PIN, HIGH);
    SPI_ADC.begin(SCLK_PIN, MISO_PIN, -1, CS_PIN);
    SPI_ADC.setHwCs(1);

    // ---  Minimal SPI ADC communication setup ---
    SPI_ADC.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE0)); 

    SPI2->ms_dlen.ms_data_bitlen = 15;
    SPI2->cmd.update = 1;
    while (SPI2->cmd.update)
        delay(1000); // wait for serial to be ready
    Serial.println("allocating ring buffer");
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

    disableCore0WDT();                                                                             // disabling the watch dog for core 0, since we are using it for the ADC task
    xTaskCreatePinnedToCore(adcTask, "ADC Task", 4096, NULL, 3, &adcTaskHandle, 1);                // Core 1
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
    SPI2->data_buf[0] = 0x0000;
    SPI2->cmd.usr = 1;
    // while (SPI2->cmd.usr); - Slows down sampling considerably
    data = SPI2->data_buf[0] & 0xFFFF;
    return data;
}

/**
 * @brief Prints the pre-trigger samples from the ring buffer for debugging purposes.
 *
 * This function iterates over the samples captured before the trigger event
 * (pre-trigger window) and prints them to the Serial console. Useful for
 * validating capture logic and trigger alignment during development.
 *
 * @note **Debug Only:** Not intended for production use due to blocking delays.
 * @note Adds a small delay between prints to avoid overwhelming the Serial output.
 *
 * @return void
 */
void printPreTriggerCapture()
{
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
}

/**
 * @brief Prints the full captured buffer to the Serial console for debugging.
 *
 * This function outputs the pre-trigger and post-trigger samples stored in
 * `printingBuffer`, providing a complete view of the captured waveform
 * around the trigger event.
 *
 * @details
 * - Takes the `bufferMutex` to ensure safe access to the printing buffer.
 * - Prints a header "START" and a footer "END" to delimit the capture.
 * - Outputs 10 samples per line for readability.
 * - Handles out-of-range samples (>4095) by printing "[ERR]".
 * - Includes optional debug info such as core ID and trigger index.
 * - Small delays are inserted to avoid overwhelming the UART buffer.
 *
 * @note **Debug Only:** This function blocks execution and should not be
 *       used in real-time acquisition tasks.
 *
 * @return void
 */
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

/**
 * @brief Main Arduino loop.
 *
 * The `loop()` function is intentionally empty because the Miniscope
 * firmware relies on FreeRTOS tasks (`adcTask` and `printingTask`) for
 * all runtime operations. No code is executed in this loop.
 *
 * @note All real-time processing is handled by tasks pinned to separate cores.
 */
void loop()
{}


/**
 * @brief FreeRTOS task for handling serial commands and printing captured data.
 *
 * This task runs on a dedicated core and performs the following functions:
 * - Listens to the Serial interface for commands from the host (PC/Android app).
 * - Supports the following commands:
 *   - `"TRIGGER"`: Requests a manual trigger for capture.
 *   - `"V_THRESHOLD=<value>"`: Sets the voltage threshold for triggering.
 *   - `"TYPE=RISING"` / `"TYPE=FALLING"`: Sets the trigger edge type.
 * - When a capture is ready (`captureReady`), prints the captured waveform
 *   from the `printingBuffer` using `printCapture()`.
 *
 * @note The task delays 1 second at startup to allow Serial initialization.
 * @note Uses `bufferMutex` to ensure safe access to shared buffers.
 * @note Runs indefinitely; real-time ADC sampling is handled in `adcTask`.
 *
 * @param pvParameters Not used; required by FreeRTOS task signature.
 */
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


