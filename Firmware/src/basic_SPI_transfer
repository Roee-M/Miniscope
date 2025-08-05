/* This main is used to read data samples form the AD7276 adc on the miniscope, and display it on the serial print
  the current method is to use the SPI library and uses hardware CS pin to automaticly toggle it instead of manually toggling it
  23/07/2025
*/

#include <Arduino.h>
#include <SPI.h>

#define CS_PIN 10
#define SCLK_PIN 12
#define MISO_PIN 13
#define VREF 3.3
#define TRIGGER_PIN 0 // Pin for manually triggering

// use manually toggeling cs insted of  SPI_ADC.setHwCs(1); in setup so it will be faster
#define CS_LOW() GPIO.out_w1tc = (1 << CS_PIN)  // Clear bit
#define CS_HIGH() GPIO.out_w1ts = (1 << CS_PIN) // Set bit

#define BUFFER_SIZE (1 * 1000)         // 1M samples for MVP - CHANGE if NEEDED - chaged to 1000
#define SAMPLE_RATE 3000000            // 3 MSPS
#define SAMPLE_T 2000                  // trigger voltage threshold - CHANGE if NEEDED
#define CAPTURE_SIZE (BUFFER_SIZE / 2) // total samples to save on trigger
#define PRINT_AROUND 32                // how many samples around the trigger to print automatically
#define SPI_HZ 40000000                // SPI clock speed

uint16_t *ringBuffer;
volatile size_t writeIndex = 0;
bool triggered = false;
bool captureReady = false;
int pre_trigger_samples = 0;
size_t triggerIndex = 0;
size_t samplesAfterTrigger = 0;
const size_t HALF_WINDOW = BUFFER_SIZE / 2;
bool button_pressed = false; // for manual trigger reset

SPIClass SPI_ADC(FSPI); // Use FSPI for ESP32-S3

void setup()
{
    Serial.begin(115200);
    delay(1000); 
    Serial.println("---Starting setup---");
    pinMode(CS_PIN, OUTPUT);
    digitalWrite(CS_PIN, HIGH);
    CS_HIGH();
    SPI_ADC.begin(SCLK_PIN, MISO_PIN, -1, CS_PIN);
    SPI_ADC.setHwCs(1); 
    Serial.println("SPI initialized using manual toggeling CS");

    // SPI_ADC.begin(SCLK_PIN, MISO_PIN, -1, CS_PIN); 
    // SPI_ADC.setHwCs(1);
    Serial.print("initiating SPI with ADC at speed:");
    Serial.println(SPI_HZ);
    // Serial.println("SPI initialized using HwCs");
    SPI_ADC.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE0)); // 40 MHz, Mode 0 - best results, try 40Mhz. tried 40Mhz and Mode 1
    delay(1000);                                                        // wait for serial to be ready
    Serial.println("allocating ring buffer");
    ringBuffer = (uint16_t *)ps_malloc(BUFFER_SIZE * sizeof(uint16_t));
    if (!ringBuffer)
    {
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

void loop()
{
    // add a flag that knows when the pre-trigger samples are ready
    uint16_t sample = readADC();
    ringBuffer[writeIndex] = sample;

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

    if (triggered && !captureReady)
    {
        samplesAfterTrigger++;
        if (samplesAfterTrigger >= HALF_WINDOW)
        {
            captureReady = true;
            Serial.println("Capture ready: pre-trigger + post-trigger window complete.");
            printCapture();
            // send over UDP in future
        }
    }

    writeIndex = (writeIndex + 1) % BUFFER_SIZE;

    // Manual trigger reset button - after pressing waiting for (Voltage > V_T) sample > SAMPLE_T
    if (digitalRead(TRIGGER_PIN) == LOW && !button_pressed)
    {
        button_pressed = true; // Set flag to prevent multiple resets
        triggered = false;
        captureReady = false;
        pre_trigger_samples = 0;
        samplesAfterTrigger = 0;
        triggerIndex = 0;
        writeIndex = 0; // Reset write index - needed?
        memset(ringBuffer, 0, BUFFER_SIZE * sizeof(uint16_t));
        Serial.println("Manual trigger reset! Waiting for next signal...");
        delay(500);             // Debounce delay
        button_pressed = false; // Reset button pressed flag
    }
}