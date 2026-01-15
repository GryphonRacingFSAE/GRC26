#include <DataAcqTask.h>
#include <Arduino.h>

#define DATA_TASK_PERIOD_MS 20

void DataAcqTask(void* pvParameters) {
    DataAcqTaskParameters* params = (DataAcqTaskParameters*)pvParameters;

    Serial.println("[Data] Task Started");

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(DATA_TASK_PERIOD_MS);

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        Serial.println("Data Acquisition Task");
        // TODO: Read CAN Bus / Sensors here
        // xQueueSend(*params->dataQueue, &myPacket, 0);
    }
}
