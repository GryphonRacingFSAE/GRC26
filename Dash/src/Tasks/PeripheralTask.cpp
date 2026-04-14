#include <PeripheralTask.h>
#include <PinDefs.h>
#include <Arduino.h>
#include <FastLED.h>

// ── LED Config ──────────────────────────────────────────────────
static constexpr int8_t NUM_LEDS            = 12 - 1;  // 11 LEDs, idx 0–10
#define LED_TYPE                              WS2812B
#define COLOR_ORDER                           GRB
#define VOLTS                                 5
#define MAX_AMPS                              500

// ── Zone indices ────────────────────────────────────────────────
static constexpr int8_t GREEN_STARTING_IDX  = 0;   // idx 0        (1 LED)
static constexpr int8_t YELLOW_STARTING_IDX = 1;   // idx 1–3      (3 LEDs)
static constexpr int8_t RED_STARTING_IDX    = 4;   // idx 4–6      (3 LEDs)
static constexpr int8_t BLUE_STARTING_IDX   = 7;   // idx 7–10     (4 LEDs)

// ── RPM sim config ──────────────────────────────────────────────
static constexpr uint16_t RPM_MIN           = 0;
static constexpr uint16_t RPM_MAX           = 8000;
static constexpr uint16_t RPM_SHIFT         = 7500;
static constexpr uint16_t RPM_STEP          = 200;
static constexpr uint32_t RPM_TICK_MS       = 80;

CRGB leds[NUM_LEDS];

// ── Zone map functions (unchanged from your originals) ───────────
void yellowLEDmap()
{
    for (int i = YELLOW_STARTING_IDX; i < RED_STARTING_IDX; i++)
    {
        leds[i] = CRGB::Yellow;
    }
}

void redLEDmap()
{
    for (int i = RED_STARTING_IDX; i < BLUE_STARTING_IDX; i++)
    {
        leds[i] = CRGB::Red;
    }
}

void blueLEDmap()
{
    for (int i = BLUE_STARTING_IDX; i < NUM_LEDS; i++)
    {
        leds[i] = CRGB::Blue;
    }
}

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

    FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_AMPS);
    FastLED.setBrightness(50);
    FastLED.clear(true);

    uint16_t simRPM      = RPM_MIN;
    uint32_t lastRPMTick = 0;
    bool     flashState  = false;
    uint32_t lastFlash   = 0;

    for (;;)
    {
        uint32_t now = millis();

        // Simulate climbing RPM, reset at redline
        if (now - lastRPMTick >= RPM_TICK_MS)
        {
            lastRPMTick = now;
            simRPM += RPM_STEP;
            if (simRPM > RPM_MAX) simRPM = RPM_MIN;
        }

        // Shift flash: strobe blue (matches your top zone) above RPM_SHIFT
        if (simRPM >= RPM_SHIFT)
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
            writeRPMBar(rpmToLEDCount(simRPM));
            FastLED.show();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}