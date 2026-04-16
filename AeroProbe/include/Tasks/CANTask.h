#ifndef CAN_TASK_H
#define CAN_TASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

typedef struct {
    QueueHandle_t* dataQueue;
} CANTaskParameters;

void CANTask(void* pvParameters);

#endif // CAN_TASK_H
