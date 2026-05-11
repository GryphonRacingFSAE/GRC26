#ifndef LORA_H
#define LORA_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

struct LoRaTaskParameters
{
    // RX board does not need this queue, but keeping the field avoids breaking
    // older project code that already fills loraParams.dataQueue.
    QueueHandle_t dataQueue;
};

void LoRaTask(void* pvParameters);

#endif // LORA_H
