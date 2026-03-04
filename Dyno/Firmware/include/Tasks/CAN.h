#ifndef CAN_H
#define CAN_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

typedef struct 
{
    QueueHandle_t* dataQueue;
} CANTaskParameters;

void CANTask(void* pvParameters);

#endif // CAN_H