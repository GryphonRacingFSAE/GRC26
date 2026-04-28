#include <PeripheralTask.h>
#include <DataAcqTask.h>
#include <PinDefs.h>
#include <Arduino.h>
#include <FastLED.h>

static constexpr int8_t NUM_LEDS            = 11;  // 11 LEDs, idx 0–10
#define PERIPH_TASK_PERIOD_MS 10
#define LED_TYPE                              WS2812B
#define COLOR_ORDER                           GRB
#define VOLTS                                 5
#define MAX_AMPS                              500

static constexpr int8_t GREEN_STARTING_IDX  = 0;   // idx 0        (1 LED)
static constexpr int8_t YELLOW_STARTING_IDX = 1;   // idx 1–3      (3 LEDs)
static constexpr int8_t RED_STARTING_IDX    = 4;   // idx 4–6      (3 LEDs)
static constexpr int8_t BLUE_STARTING_IDX   = 7;   // idx 7–10     (4 LEDs)

static constexpr uint16_t RPM_MIN           = 0;
static constexpr uint16_t RPM_MAX           = 11000;
static constexpr uint16_t RPM_SHIFT         = 9000;

CRGB leds[NUM_LEDS];

// ── Returns colour matching your zone layout ─────────────────────
static CRGB colourForIndex(uint8_t idx)
{
    if (idx < YELLOW_STARTING_IDX) return CRGB::Green;
    if (idx < RED_STARTING_IDX)    return CRGB::Yellow;
    if (idx < BLUE_STARTING_IDX)   return CRGB::Red;
    return CRGB::Blue;
}

// ── Maps RPM to how many LEDs should be lit ──────────────────────
static uint8_t rpmToLEDCount(uint16_t rpm)
{
    if (rpm <= RPM_MIN) return 0;
    if (rpm >= RPM_MAX) return NUM_LEDS;
    return (uint8_t)map(rpm, RPM_MIN, RPM_MAX, 0, NUM_LEDS);
}

// ── Writes the RPM bar — no show() call ─────────────────────────
static void writeRPMBar(uint8_t litCount)
{
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        leds[i] = (i < litCount) ? colourForIndex(i) : CRGB::Black;
    }
}

// ── Task ─────────────────────────────────────────────────────────
void PeripheralTask(void *pvParameters)
{
    Serial.println("[Periph] Task Started");
    PeripheralTaskParameters* params = (PeripheralTaskParameters*)pvParameters;
    QueueHandle_t data_queue = *(params->peripheralQueue); 

    FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_AMPS);
    FastLED.setBrightness(50);
    FastLED.clear(true);

    EcuData_t ecuData = {0};
    uint16_t rpm_curr = 0;
    bool     flashState = false;
    uint32_t lastFlash = 0;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(PERIPH_TASK_PERIOD_MS);

    for (;;)
    {
        EcuData_t incoming; 
        while(xQueueReceive(data_queue, &incoming, 0) == pdPASS) {
            ecuData = incoming;
            rpm_curr = ecuData.rpm;
        }

        uint32_t now = millis();

        // Shift flash: strobe blue (matches your top zone) above RPM_SHIFT
        if (rpm_curr >= RPM_SHIFT)
        {
            if (now - lastFlash >= 80)
            {
                lastFlash  = now;
                flashState = !flashState;
                fill_solid(leds, NUM_LEDS, flashState ? CRGB::Blue : CRGB::Black);
                FastLED.show();
            }
        }
        else
        {
            writeRPMBar(rpmToLEDCount(rpm_curr));
            FastLED.show();
        }

        vTaskDelay(xFrequency);
    }
}
