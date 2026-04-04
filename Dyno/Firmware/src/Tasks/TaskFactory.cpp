#include "TaskFactory.h"
#include "CAN.h"
#include "DynoData.h"
#include "Outputs.h"
#include <Arduino.h>

static TaskHandle_t canTaskHandle    = NULL;
static TaskHandle_t dataTaskHandle   = NULL;
static TaskHandle_t outputTaskHandle = NULL;

CANTaskParameters      canTaskParams;
DynoDataTaskParameters dynoDataTaskParams;
OutputsTaskParameters  outputsTaskParams;

QueueHandle_t xCanToDynoQueue;
QueueHandle_t xDynoToOutputsQueue;

void createTasks() {
    Serial.println("[Factory] Creating Tasks...");
    
    xCanToDynoQueue     = xQueueCreate(10, sizeof(DynoData));
    xDynoToOutputsQueue = xQueueCreate(10, sizeof(DynoData));

    canTaskParams.dynoQueue        = &xCanToDynoQueue;
    dynoDataTaskParams.inputQueue  = &xCanToDynoQueue;
    dynoDataTaskParams.outputQueue = &xDynoToOutputsQueue;
    outputsTaskParams.dynoQueue    = &xDynoToOutputsQueue;

    xTaskCreate(CANTask,        "CAN", 4096, (void*)&canTaskParams,      1, &canTaskHandle);
    xTaskCreate(DynoDataTask,  "Data", 4096, (void*)&dynoDataTaskParams, 2, &dataTaskHandle);
    xTaskCreate(OutputsTask, "Output", 4096, (void*)&outputsTaskParams,  2, &outputTaskHandle);

    Serial.println("[Factory] Tasks Created");
}