#ifndef DATA_ACQ_TASK_H
#define DATA_ACQ_TASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

typedef struct {
    SemaphoreHandle_t* guiMutex;
} DataAcqTaskParameters;

void DataAcqTask(void* pvParameters);

// TODO: Public Struct that will update values for all values read from CAN
// The variables in this struct will be used elsewhere in the program
typedef struct ecuData
{
    // Add variables names in here 
    // ex: uint16_t rpm;
} ecuData;

#endif // DATA_ACQ_TASK_H
