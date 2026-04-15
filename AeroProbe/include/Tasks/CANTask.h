#ifndef CANTASK_H
#define CANTASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

typedef struct {
    QueueHandle_t* dataQueue;
} CANTaskParameters;

void CANTask(void* pvParameters);

#endif // CANTASK_H