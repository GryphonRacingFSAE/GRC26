#ifndef OUTPUTS_H
#define OUTPUTS_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

typedef struct 
{
    QueueHandle_t* dataQueue;
} OutputsTaskParameters;

void OutputsTask(void* pvParameters);

#endif // OUTPUTS_H