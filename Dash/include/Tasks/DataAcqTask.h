#ifndef DATA_ACQ_TASK_H
#define DATA_ACQ_TASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <stdint.h>
#include <freertos/semphr.h>
#include <stdbool.h>

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

    //0x520
    uint16_t rpm; 
    float throttlePercent;

    //0x522
    float vehicleSpeedKph;

    //0x523
    float wheelSpeedDrivenKph;

    //0x530
    float coolantTempC;

    //0x536
    bool oilPressureWarning; //true = low/no pressure
    float oilTempC;

} ecuData;

#endif // DATA_ACQ_TASK_H
