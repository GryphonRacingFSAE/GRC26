#include <PeripheralTask.h>
#include <Arduino.h>

#define PERIPH_TASK_PERIOD_MS 20

void PeripheralTask(void* pvParameters) {
    Serial.println("[Periph] Task Started");

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(PERIPH_TASK_PERIOD_MS);

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        Serial.println("Peripheral Task");
        // TODO: Blink LEDs, Read Buttons, Update IO Expander
    }
}