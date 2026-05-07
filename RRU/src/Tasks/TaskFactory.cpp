#include "TaskFactory.h"
#include "LoRa.h"
#include "Outputs.h"
#include "LoRaOutputs.h"
#include <Arduino.h>

static QueueHandle_t dataQueueHandle = nullptr;

static TaskHandle_t loraTaskHandle = nullptr;
static TaskHandle_t outputsTaskHandle = nullptr;

static LoRaTaskParameters loraParams;
static OutputsTaskParameters outputsParams;

void createTasks()
{
    dataQueueHandle = xQueueCreate(10, sizeof(LoRaOutputEvent));

    configASSERT(dataQueueHandle != nullptr);

    loraParams.dataQueue = dataQueueHandle;
    outputsParams.dataQueue = dataQueueHandle;

    xTaskCreate(
        LoRaTask,
        "LoRaTask",
        4096,
        static_cast<void*>(&loraParams),
        1,
        &loraTaskHandle
    );

    xTaskCreate(
        OutputsTask,
        "OutputsTask",
        4096,
        static_cast<void*>(&outputsParams),
        1,
        &outputsTaskHandle
    );
}