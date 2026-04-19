#include <TaskFactory.h>
#include <LoRa.h>
#include <Outputs.h>
#include <Arduino.h>

static QueueHandle_t dataQueueHandle;

static TaskHandle_t loraTaskHandle = NULL;
static TaskHandle_t outputsTaskHandle = NULL;

static LoRaTaskParameters loraParams;
static OutputsTaskParameters outputsParams;

void createTasks() {
    // 1. Object
    dataQueueHandle = xQueueCreate(10, sizeof(int)); // Update sizeof() later

    // 2. Params
    loraParams.dataQueue = &dataQueueHandle;
    outputsParams.dataQueue = &dataQueueHandle;

    // 3. Tasks
    xTaskCreate(LoRaTask, "LoRaTask", 4096, (void*)&loraParams, 1, &loraTaskHandle);
    xTaskCreate(OutputsTask, "OutputsTask", 4096, (void*)&outputsParams, 1, &outputsTaskHandle);
}