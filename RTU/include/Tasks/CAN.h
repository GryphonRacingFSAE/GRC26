#ifndef CAN_H
#define CAN_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

typedef struct 
{
    QueueHandle_t* dataQueue;
} CANTaskParameters;

/// @brief CAN communication task, receives data from CAN bus and sends it to the data queue for processing
/// @param pvParameters 
void CANTask(void* pvParameters);

#endif // CAN_H