#include <PeripheralTask.h>
#include <PinDefs.h>
#include <Arduino.h>
#include <FastLED.h>

static constexpr int8_t NUM_LEDS = 12 - 1;
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define PERIPH_TASK_PERIOD_MS 20
#define VOLTS 5
#define MAX_AMPS 500
#define PERIPH_TASK_PERIOD_MS 20
#define LED_UPDATE_INTERVAL_MS 100

CRGB leds[NUM_LEDS];

void PeripheralTask(void *pvParameters)
{
    Serial.println("[Periph] Task Started");

    FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_AMPS);
    FastLED.clear(true);

    for (;;)
    {
        for(int i = 0; i < NUM_LEDS; i++) 
        {
            leds[i] = CRGB::Red;
        }
        FastLED.show();
    }
}