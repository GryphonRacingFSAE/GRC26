#ifndef DYNO_DATA_H
#define DYNO_DATA_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

typedef struct 
{
    QueueHandle_t* dataQueue;
} DynoDataTaskParameters;

void DynoDataTask(void* pvParameters);

#endif // DYNO_DATA_H