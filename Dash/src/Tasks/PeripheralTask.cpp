#include <PeripheralTask.h>
#include <PinDefs.h>
#include <Arduino.h>
#include <FastLED.h>

#define PERIPH_TASK_PERIOD_MS 20
#define VOLTS 5
#define MAX_AMPS 500
#define PERIPH_TASK_PERIOD_MS 20
#define LED_UPDATE_INTERVAL_MS 100

CRGB leds[NUM_LEDS];

void PeripheralTask(void *pvParameters)
{
    Serial.println("[Periph] Task Started");

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(PERIPH_TASK_PERIOD_MS);

    // LED setup
    // FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    // FastLED.setBrightness(80);
    // FastLED.setMaxPowerInVoltsAndMilliamps(5, 1500);

    // LED setup for Testcode 3
    delay(3000); // Power-up safety delay
    FastLED.addLeds<LED_TYPE,LED_DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS,MAX_AMPS);
    FastLED.setBrightness(80);
    FastLED.clear();
    FastLED.show();

    // LED setup for Testcode 4
    // FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    // FastLED.setBrightness(80);
    // FastLED.setMaxPowerInVoltsAndMilliamps(5, 1500);
    // uint32_t lastLedUpdate = 0;
    // uint8_t colorIndex = 0;
    // CRGB colors[] = {CRGB::Blue, CRGB::Green, CRGB::Red};

    // digitalWrite(LED_DATA_PIN, HIGH);

    for (;;)
    {
        // digitalWrite(LED_DATA_PIN, HIGH);
        // printf("Digital Write HIGH to LED_DATA_PIN\n");
        // vTaskDelayUntil(&xLastWakeTime, xFrequency);
        // Serial.println("Peripheral Task");
        // TODO: Blink LEDs, Read Buttons, Update IO Expander

        // Testcode 1
        //  fill_solid(leds, NUM_LEDS, CRGB::Blue);
        //  FastLED.show();
        //  printf("LEDs set to Blue\n");
        //  // vTaskDelay(pdMS_TO_TICKS(100));

        // Testcode 2
        //  for (int i = 0; i < NUM_LEDS; i++)
        //  {
        //      leds[i] = CRGB(255, 0, 0); // Red
        //  }
        //  fill_solid(leds, NUM_LEDS, CRGB::Red);
        //  FastLED.show();
        //  printf("LEDs set to Red\n");

        // Testcode 3
        leds[0] = CRGB(0, 0, 255); // Blue
        FastLED.show();
        printf("LED 0 set to Blue\n");

        // Testcode 4
        //  vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // uint32_t now = millis();

        // Update LEDs every 100ms
        // if (now - lastLedUpdate >= LED_UPDATE_INTERVAL_MS) {
        //     fill_solid(leds, NUM_LEDS, colors[colorIndex]);
        //     FastLED.show();
        //     colorIndex = (colorIndex + 1) % 3;
        //     lastLedUpdate = now;
        // }
    }
}