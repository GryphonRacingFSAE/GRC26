#include <TaskFactory.h>
#include "LoRa.h"
#include "CAN.h"
#include <Arduino.h>

static QueueHandle_t dataQueueHandle;

static TaskHandle_t loraTaskHandle = NULL;
static TaskHandle_t canTaskHandle = NULL;

static LoRaTaskParameters loraParams;
static CANTaskParameters canParams;

void createTasks() {
    // 1. Object
    dataQueueHandle = xQueueCreate(10, sizeof(int)); // Update sizeof() later

    // 2. Params
    loraParams.dataQueue = &dataQueueHandle;
    canParams.dataQueue = &dataQueueHandle;

    // 3. Tasks
    xTaskCreate(LoRaTask, "LoRaTask", 4096, (void*)&loraParams, 1, &loraTaskHandle);
    // xTaskCreate(CANTask, "CANTask", 4096, (void*)&canParams, 1, &canTaskHandle);
}