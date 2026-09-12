#include <PeripheralTask.h>
#include <DataAcqTask.h>
#include <PinDefs.h>
#include <Arduino.h>
#include <FastLED.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static constexpr uint8_t NUM_LEDS = 11;   // Physical LEDs: idx 0–10

int8_t NEUTRAL_TRUE = 1;

#define LED_TYPE              WS2812B
#define COLOR_ORDER           GRB
#define VOLTS                 5
#define MAX_AMPS              500

// Physical LED layout
// LED 0 is the neutral indicator. LED 10 is unused and intentionally kept off.
static constexpr uint8_t NEUTRAL_LED_IDX   = 0;
static constexpr uint8_t RPM_FIRST_LED_IDX = 1;
static constexpr uint8_t RPM_LAST_LED_IDX  = 9;
static constexpr uint8_t UNUSED_LED_IDX    = 10;

static constexpr uint8_t RPM_LED_COUNT = RPM_LAST_LED_IDX - RPM_FIRST_LED_IDX + 1;  // 9 LEDs

static constexpr uint16_t RPM_MIN             = 0;
static constexpr uint16_t RPM_REDLINE         = 11000;
static constexpr uint16_t RPM_FLASH_THRESHOLD = 10500;

CRGB leds[NUM_LEDS];

void NeutralDetectISR()
{
    NEUTRAL_TRUE = digitalRead(NEUTRAL_DETECT_PIN) == LOW;
}

static CRGB colourForRPMIndex(uint8_t ledIdx)
{
    const uint8_t rpmIdx = ledIdx - RPM_FIRST_LED_IDX;

    if (rpmIdx < 3) return CRGB::Yellow;
    if (rpmIdx < 6) return CRGB::Red;
    return CRGB::Blue;
}

static uint8_t rpmToLEDCount(uint16_t rpm)
{
    if (rpm <= RPM_MIN)
    {
        return 0;
    }

    if (rpm >= RPM_REDLINE)
    {
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

        leds[ledIdx] = (rpmIdx < litCount)
            ? colourForRPMIndex(ledIdx)
            : CRGB::Black;
    }
}

static void writeRPMFlash(bool flashState)
{
    for (uint8_t ledIdx = RPM_FIRST_LED_IDX; ledIdx <= RPM_LAST_LED_IDX; ledIdx++)
    {
        leds[ledIdx] = flashState ? CRGB::Blue : CRGB::Black;
    }
}

static void writeAllLEDs(uint16_t rpm, bool rpmFlashState, int8_t neutralState)
{
    leds[NEUTRAL_LED_IDX] = neutralState ? CRGB::Green : CRGB::Black;
    leds[UNUSED_LED_IDX] = CRGB::Black;

    if (rpm >= RPM_FLASH_THRESHOLD)
    {
        writeRPMFlash(rpmFlashState);
    }
    else
    {
        writeRPMBar(rpmToLEDCount(rpm));
    }

    FastLED.show();
}

void PeripheralTask(void *pvParameters)
{
    Serial.println("[Periph] Task Started");

    // Interrupt registration
    pinMode(NEUTRAL_DETECT_PIN, INPUT_PULLUP);
    NEUTRAL_TRUE = digitalRead(NEUTRAL_DETECT_PIN) == LOW;
    attachInterrupt(digitalPinToInterrupt(NEUTRAL_DETECT_PIN), NeutralDetectISR, CHANGE);

    PeripheralTaskParameters* params = static_cast<PeripheralTaskParameters*>(pvParameters);

    QueueHandle_t data_queue = *(params->peripheralQueue);

    FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

    FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_AMPS);
    FastLED.setBrightness(100);

    FastLED.clear(true);

    EcuData_t ecuData = {0};
    uint16_t rpm_curr = 100;

    bool rpmFlashState = false;

    writeAllLEDs(rpm_curr, rpmFlashState, NEUTRAL_TRUE);

    for (;;)
    {
        EcuData_t incoming;

        //Block here until the queue receives new data.
        if (xQueueReceive(data_queue, &incoming, portMAX_DELAY) == pdPASS)
        {
            ecuData = incoming;

            // Drain any queued frames so we render the most recent RPM.
            while (xQueueReceive(data_queue, &incoming, 0) == pdPASS)
            {
                ecuData = incoming;
            }

            rpm_curr = ecuData.rpm;

            if (rpm_curr >= RPM_FLASH_THRESHOLD)
            {
                rpmFlashState = !rpmFlashState;
            }
            else
            {
                rpmFlashState = false;
            }

            writeAllLEDs(rpm_curr, rpmFlashState, NEUTRAL_TRUE);
        }
    }
}
