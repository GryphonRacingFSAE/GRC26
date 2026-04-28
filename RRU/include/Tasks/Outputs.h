#ifndef OUTPUTS_H
#define OUTPUTS_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

typedef struct 
{
    QueueHandle_t* dataQueue;
} OutputsTaskParameters;

/// @brief Outputs task, processes data from the data queue and prints to serial
/// @param pvParameters 
void OutputsTask(void* pvParameters);

#endif // OUTPUTS_H