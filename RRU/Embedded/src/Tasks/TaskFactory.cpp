#include <Arduino.h>
#include "TaskFactory.h"
#include "LoRa.h"

static TaskHandle_t loraTaskHandle = nullptr;
static LoRaTaskParameters loraParams = {};

void createTasks()
{
    // RX board only needs the LoRa task.
    // No CAN task, no Outputs task, no queue needed for raw serial dumping.
    loraParams.dataQueue = nullptr;

    const BaseType_t loraCreated = xTaskCreate(
        LoRaTask,
        "LoRaTask",
        4096,
        static_cast<void*>(&loraParams),
        1,
        &loraTaskHandle
    );

    if (loraCreated != pdPASS) {
        Serial.println("[TaskFactory] Failed to create LoRa task");
    }
}
