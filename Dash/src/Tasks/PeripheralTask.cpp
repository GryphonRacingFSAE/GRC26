#include <PeripheralTask.h>
#include <DataAcqTask.h>
#include <PinDefs.h>
#include <Arduino.h>
#include <FastLED.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

static constexpr uint8_t NUM_LEDS = 11;   // Physical LEDs: idx 0–10

#define LED_TYPE              WS2812B
#define COLOR_ORDER           GRB
#define VOLTS                 5
#define MAX_AMPS              500

// Physical LED layout
static constexpr uint8_t NEUTRAL_LED_IDX      =  0;
static constexpr uint8_t RPM_FIRST_LED_IDX    =  1;
static constexpr uint8_t RPM_LAST_LED_IDX     =  9;
static constexpr uint8_t OIL_PRESSURE_LED_IDX = 10;

static constexpr uint8_t RPM_LED_COUNT = RPM_LAST_LED_IDX - RPM_FIRST_LED_IDX + 1;  // 9 LEDs

static constexpr uint16_t RPM_MIN             = 0;
static constexpr uint16_t RPM_REDLINE         = 11000;
static constexpr uint16_t RPM_FLASH_THRESHOLD = 10500;

CRGB leds[NUM_LEDS];

static SemaphoreHandle_t neutralSwitchSemaphore = nullptr;
static SemaphoreHandle_t oilPressureSwitchSemaphore = nullptr;

static bool neutralSwitchFlag = false;
static bool oilPressureSwitchFlag = false;

void IRAM_ATTR neutralSwitchISR()
{
    BaseType_t higherPriorityTaskWoken = pdFALSE;

    if (neutralSwitchSemaphore != nullptr)
    {
        xSemaphoreGiveFromISR(neutralSwitchSemaphore, &higherPriorityTaskWoken);
    }

    if (higherPriorityTaskWoken == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
}

void IRAM_ATTR oilPressureSwitchISR()
{
    BaseType_t higherPriorityTaskWoken = pdFALSE;

    if (oilPressureSwitchSemaphore != nullptr)
    {
        xSemaphoreGiveFromISR(oilPressureSwitchSemaphore, &higherPriorityTaskWoken);
    }

    if (higherPriorityTaskWoken == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
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

        leds[ledIdx] = (rpmIdx < litCount) ? colourForRPMIndex(ledIdx) : CRGB::Black;
    }
}

static void writeRPMFlash(bool flashState)
{
    for (uint8_t ledIdx = RPM_FIRST_LED_IDX; ledIdx <= RPM_LAST_LED_IDX; ledIdx++)
    {
        leds[ledIdx] = flashState ? CRGB::Blue : CRGB::Black;
    }
}

static void writeNeutralLED()
{
    leds[NEUTRAL_LED_IDX] = neutralSwitchFlag ? CRGB::Green : CRGB::Black;
}

static void writeOilPressureLED()
{
    leds[OIL_PRESSURE_LED_IDX] = oilPressureSwitchFlag ? CRGB::Red : CRGB::Black;
}

static void writeAllLEDs(uint16_t rpm, bool rpmFlashState)
{
    if (rpm >= RPM_FLASH_THRESHOLD)
    {
        writeRPMFlash(rpmFlashState);
    }
    else
    {
        writeRPMBar(rpmToLEDCount(rpm));
    }

    writeNeutralLED();
    writeOilPressureLED();

    FastLED.show();
}

void PeripheralTask(void *pvParameters)
{
    Serial.println("[Periph] Task Started");

    PeripheralTaskParameters* params = static_cast<PeripheralTaskParameters*>(pvParameters);
    QueueHandle_t data_queue = *(params->peripheralQueue);

    pinMode(NEUTRAL_DETECT_PIN, INPUT_PULLDOWN);
    pinMode(OIL_PRESSURE_PIN,   INPUT_PULLDOWN);

    neutralSwitchSemaphore     = xSemaphoreCreateBinary();
    oilPressureSwitchSemaphore = xSemaphoreCreateBinary();

    if (neutralSwitchSemaphore == nullptr || oilPressureSwitchSemaphore == nullptr)
    {
        Serial.println("[Periph] Failed to create switch semaphores");
        vTaskDelete(nullptr);
    }

    const UBaseType_t dataQueueLength =
        uxQueueMessagesWaiting(data_queue) + uxQueueSpacesAvailable(data_queue);

    QueueSetHandle_t peripheralEventSet = xQueueCreateSet(dataQueueLength + 2);

    if (peripheralEventSet == nullptr)
    {
        Serial.println("[Periph] Failed to create queue set");
        vTaskDelete(nullptr);
    }

    xQueueAddToSet(data_queue, peripheralEventSet);
    xQueueAddToSet(neutralSwitchSemaphore, peripheralEventSet);
    xQueueAddToSet(oilPressureSwitchSemaphore, peripheralEventSet);

    neutralSwitchFlag = digitalRead(NEUTRAL_DETECT_PIN) == HIGH;
    oilPressureSwitchFlag = digitalRead(OIL_PRESSURE_PIN) == HIGH;

    attachInterrupt(
        digitalPinToInterrupt(NEUTRAL_DETECT_PIN),
        neutralSwitchISR,
        CHANGE
    );

    attachInterrupt(
        digitalPinToInterrupt(OIL_PRESSURE_PIN),
        oilPressureSwitchISR,
        CHANGE
    );

    FastLED.addLeds<LED_TYPE, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip);

    FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_AMPS);
    FastLED.setBrightness(50);

    FastLED.clear(true);

    EcuData_t ecuData = {0};
    uint16_t rpm_curr = 0;

    bool rpmFlashState = false;

    writeAllLEDs(rpm_curr, rpmFlashState);

    for (;;)
    {
        QueueSetMemberHandle_t activatedMember = xQueueSelectFromSet(peripheralEventSet, portMAX_DELAY);

        bool ledDirty = false;
        bool canUpdated = false;

        if (activatedMember == data_queue)
        {
            EcuData_t incoming;

            while (xQueueReceive(data_queue, &incoming, 0) == pdPASS)
            {
                ecuData = incoming;
                rpm_curr = ecuData.rpm;
                canUpdated = true;
            }

            if (canUpdated)
            {
                if (rpm_curr >= RPM_FLASH_THRESHOLD)
                {
                    rpmFlashState = !rpmFlashState;
                }
                else
                {
                    rpmFlashState = false;
                }

                ledDirty = true;
            }
        }
        else if (activatedMember == neutralSwitchSemaphore)
        {
            if (xSemaphoreTake(neutralSwitchSemaphore, 0) == pdPASS)
            {
                neutralSwitchFlag = digitalRead(NEUTRAL_DETECT_PIN) == HIGH;
                ledDirty = true;
            }
        }
        else if (activatedMember == oilPressureSwitchSemaphore)
        {
            if (xSemaphoreTake(oilPressureSwitchSemaphore, 0) == pdPASS)
            {
                oilPressureSwitchFlag = digitalRead(OIL_PRESSURE_PIN) == HIGH;
                ledDirty = true;
            }
        }

        if (ledDirty)
        {
            writeAllLEDs(rpm_curr, rpmFlashState);
        }
    }
}