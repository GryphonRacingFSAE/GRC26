#include <Outputs.h>
#include <Arduino.h>
#include <PinDefs.h>

#define OUTPUT_TASK_PERIOD_MS 50

void OutputsTask(void* pvParameters) 
{
    OutputsTaskParameters* params = (OutputsTaskParameters*)pvParameters;

    Serial.println("[Outputs] Task Started");

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(OUTPUT_TASK_PERIOD_MS);

    for (;;) 
    {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        Serial.println("[Outputs] Running");
    }
}