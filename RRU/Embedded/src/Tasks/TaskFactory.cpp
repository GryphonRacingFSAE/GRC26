#include <Arduino.h>
#include "TaskFactory.h"
#include "LoRa.h"

static TaskHandle_t loraTaskHandle = nullptr;

void createTasks()
{
    // RX board only needs the LoRa task.

    const BaseType_t loraCreated = xTaskCreate(
        LoRaTask,
        "LoRaTask",
        4096,
        nullptr,
        1,
        &loraTaskHandle
    );

    if (loraCreated != pdPASS) {
        Serial.println("[TaskFactory] Failed to create LoRa task");
    }
}
