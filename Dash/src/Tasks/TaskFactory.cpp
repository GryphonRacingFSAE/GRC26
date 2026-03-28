#include <TaskFactory.h>
#include <GuiTask.h>
#include <DataAcqTask.h>
#include <PeripheralTask.h>
#include <Arduino.h>

#include "driver/twai.h"

// Static Handles (Private)
static SemaphoreHandle_t guiMutexHandle;
static QueueHandle_t     dataQueueHandle;

static TaskHandle_t guiTaskHandle = NULL;
static TaskHandle_t dataTaskHandle = NULL;
static TaskHandle_t periphTaskHandle = NULL;

// Static Parameters
static GuiTaskParameters      guiParams;
static DataAcqTaskParameters  dataParams;
static PeripheralTaskParameters periphParams;

void createTasks() {
    Serial.println("[Factory] Creating Tasks...");

    // CAN Init
    // CAN_H and CAN_L are different from CAN_TX and CAN_RX.
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_15, GPIO_NUM_16, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); 
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        Serial.println("[Factory] TWAI Driver Installed");
    }

    if (twai_start() == ESP_OK) {
        Serial.println("[Factory] TWAI Started");
    }

    // 1. Objects
    guiMutexHandle = xSemaphoreCreateMutex();
    dataQueueHandle = xQueueCreate(10, sizeof(EcuData_t)); // Update sizeof() later

    // 2. Params
    guiParams.guiMutex = &guiMutexHandle;
    guiParams.dataQueue = &dataQueueHandle;
    
    dataParams.dataQueue = &dataQueueHandle;
    
    periphParams.guiMutex = &guiMutexHandle;

    // 3. Tasks
    // GUI on Core 1 (App Core) is best for Rendering
    xTaskCreatePinnedToCore(GuiTask, "GUI", 8192, (void*)&guiParams, 2, &guiTaskHandle, 1);

    // Logic on Core 0 or 1
    xTaskCreate(DataAcqTask, "Data", 4096, (void*)&dataParams, 1, &dataTaskHandle);
    xTaskCreate(PeripheralTask, "Periph", 2048, (void*)&periphParams, 1, &periphTaskHandle);
}