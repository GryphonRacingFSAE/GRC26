#include <PeripheralTask.h>
#include <PinDefs.h>
#include <Arduino.h>
#include <FastLED.h>

#define PERIPH_TASK_PERIOD_MS 20

CRGB leds[NUM_LEDS];

void PeripheralTask(void *pvParameters)
{
    Serial.println("[Periph] Task Started");

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(PERIPH_TASK_PERIOD_MS);

    // LED setup
    FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(80);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 1500);

    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        Serial.println("Peripheral Task");
        // TODO: Blink LEDs, Read Buttons, Update IO Expander

        fill_solid(leds, NUM_LEDS, CRGB::Blue);
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(100));

        fill_solid(leds, NUM_LEDS, CRGB::Green);
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(100));

        fill_solid(leds, NUM_LEDS, CRGB::Red);
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}