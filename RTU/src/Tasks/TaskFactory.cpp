#include <Arduino.h>
#include <TaskFactory.h>
#include "Telemetry.h"
#include "CANTask.h"
#include "LoRa.h"

static QueueHandle_t dataQueueHandle = nullptr;

static TaskHandle_t loraTaskHandle = nullptr;
static TaskHandle_t canTaskHandle = nullptr;

static LoRaTaskParameters loraParams = {};
static CANTaskParameters canParams = {};

void createTasks()
{
    // One typed queue: CAN task produces TelemetryPacket, LoRa task consumes it.
    dataQueueHandle = xQueueCreate(24, sizeof(TelemetryPacket));

    if (dataQueueHandle == nullptr) {
        Serial.println("[TaskFactory] Failed to create telemetry queue");
        return;
    }

    loraParams.dataQueue = dataQueueHandle;
    canParams.dataQueue = dataQueueHandle;

    const BaseType_t loraCreated = xTaskCreate(
        LoRaTask,
        "LoRaTask",
        4096,
        &loraParams,
        2,
        &loraTaskHandle
    );

    const BaseType_t canCreated = xTaskCreate(
        CANTask,
        "CANTask",
        4096,
        &canParams,
        3,
        &canTaskHandle
    );

    if (loraCreated != pdPASS) {
        Serial.println("[TaskFactory] Failed to create LoRa task");
    }

    if (canCreated != pdPASS) {
        Serial.println("[TaskFactory] Failed to create CAN task");
    }
}
