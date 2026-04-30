#include <TaskFactory.h>
#include <DataAcqTask.h>
#include <CANTask.h>
#include <Arduino.h>

static QueueHandle_t dataQueueHandle;

static TaskHandle_t dataTaskHandle = NULL;
static TaskHandle_t canTaskHandle = NULL;

static DataAcqTaskParameters dataParams;
static CANTaskParameters canParams;

void createTasks() {
    // 1. Object
    dataQueueHandle = xQueueCreate(10, sizeof(IMUGPSData_t)); // Update sizeof() later

    // 2. Params
    dataParams.dataQueue = &dataQueueHandle;
    canParams.dataQueue = &dataQueueHandle;

    // 3. Tasks
    xTaskCreate(DataAcqTask, "DataAcqTask", 4096, (void*)&dataParams, 1, &dataTaskHandle);
    xTaskCreate(CANTask, "CANTask", 4096, (void*)&canParams, 1, &canTaskHandle);
}
