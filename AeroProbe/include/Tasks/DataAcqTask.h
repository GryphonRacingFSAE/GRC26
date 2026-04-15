#ifndef DATAACQTASK_H
#define DATAACQTASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

typedef struct {
    QueueHandle_t* dataQueue;
} DataAcqTaskParameters;

void DataAcqTask(void* pvParameters);

#endif // DATAACQTASK_H