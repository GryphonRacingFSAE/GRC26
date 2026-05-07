#include <PeripheralTask.h>
#include <DataAcqTask.h>
#include <PinDefs.h>
#include <Arduino.h>
#include <FastLED.h>

static constexpr uint8_t NUM_LEDS = 11;   // Physical LEDs: idx 0–10

#define PERIPH_TASK_PERIOD_MS 10
#define LED_TYPE              WS2812B
#define COLOR_ORDER           GRB
#define VOLTS                 5
#define MAX_AMPS              500

// Physical LED layout
static constexpr uint8_t RESERVED_FIRST_LED_IDX = 0;
static constexpr uint8_t RPM_FIRST_LED_IDX      = 1;
static constexpr uint8_t RPM_LAST_LED_IDX       = 9;
static constexpr uint8_t RESERVED_LAST_LED_IDX  = 10;

static constexpr uint8_t RPM_LED_COUNT =
    RPM_LAST_LED_IDX - RPM_FIRST_LED_IDX + 1;  // 9 LEDs

static constexpr uint16_t RPM_MIN             = 0;
static constexpr uint16_t RPM_REDLINE         = 11000;
static constexpr uint16_t RPM_FLASH_THRESHOLD = 10500;

static constexpr uint32_t FLASH_INTERVAL_MS = 80;

CRGB leds[NUM_LEDS];

static CRGB colourForRPMIndex(uint8_t ledIdx)
{
    const uint8_t rpmIdx = ledIdx - RPM_FIRST_LED_IDX; 

    if (rpmIdx < 3) return CRGB::Yellow;   
    if (rpmIdx < 6) return CRGB::Red;  
    return CRGB::Blue;                     
}

static uint8_t rpmToLEDCount(uint16_t rpm)
{
    if (rpm <= RPM_MIN) {
        return 0;
    }

    if (rpm >= RPM_REDLINE) {
        return RPM_LED_COUNT;
    }

    return static_cast<uint8_t>(
        (static_cast<uint32_t>(rpm) * RPM_LED_COUNT) / RPM_REDLINE
    );
}

static void writeRPMBar(uint8_t litCount)
{
    for (uint8_t ledIdx = RPM_FIRST_LED_IDX; ledIdx <= RPM_LAST_LED_IDX; ledIdx++)
    {
        const uint8_t rpmIdx = ledIdx - RPM_FIRST_LED_IDX;

        if (rpmIdx < litCount) {
            leds[ledIdx] = colourForRPMIndex(ledIdx);
        } else {
            leds[ledIdx] = CRGB::Black;
        }
    }
}

static void writeRPMFlash(bool flashState)
{
    for (uint8_t ledIdx = RPM_FIRST_LED_IDX; ledIdx <= RPM_LAST_LED_IDX; ledIdx++)
    {
        leds[ledIdx] = flashState ? CRGB::Blue : CRGB::Black;
    }
}

void PeripheralTask(void *pvParameters)
{
    Serial.println("[Periph] Task Started");

    PeripheralTaskParameters* params = static_cast<PeripheralTaskParameters*>(pvParameters);
    QueueHandle_t data_queue = *(params->peripheralQueue);

    FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip);

    FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_AMPS);
    FastLED.setBrightness(50);

    FastLED.clear(true);

    EcuData_t ecuData = {0};
    uint16_t rpm_curr = 0;

    bool flashState = false;
    uint32_t lastFlashTime = 0;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(PERIPH_TASK_PERIOD_MS);

    for (;;)
    {
        EcuData_t incoming;

        while (xQueueReceive(data_queue, &incoming, 0) == pdPASS)
        {
            ecuData = incoming;
            rpm_curr = ecuData.rpm;
        }

        const uint32_t now = millis();

        if (rpm_curr >= RPM_FLASH_THRESHOLD)
        {
            if (now - lastFlashTime >= FLASH_INTERVAL_MS)
            {
                lastFlashTime = now;
                flashState = !flashState;

                writeRPMFlash(flashState);
                FastLED.show();
            }
        }
        else
        {
            flashState = false;

            const uint8_t litCount = rpmToLEDCount(rpm_curr);

            writeRPMBar(litCount);
            FastLED.show();
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}