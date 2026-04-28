#include <CANTask.h>
#include <Arduino.h>
#include <PinDefs.h>

#define CAN_TASK_PERIOD_MS 50

void CANTask(void* pvParameters) {
    CANTaskParameters* params = (CANTaskParameters*)pvParameters;

    Serial.println("[CAN] Task Started");

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(CAN_TASK_PERIOD_MS);

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        Serial.println("CAN Task");
        // TODO: Transmit data to CAN Bus here
        // xQueueSend(*params->dataQueue, &myPacket, 0);
    }
}
