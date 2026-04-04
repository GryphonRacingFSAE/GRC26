#ifndef DYNO_DATA_H
#define DYNO_DATA_H

#include <Arduino.h>

typedef struct
{
    float torque;
    float horsepower;
    int16_t rpm;
} DynoData;

typedef struct 
{
    QueueHandle_t* inputQueue;
    QueueHandle_t* outputQueue;
} DynoDataTaskParameters;

extern QueueHandle_t xCanToDynoQueue;
extern QueueHandle_t xDynoToOutputsQueue;


void DynoDataTask(void* pvParameters);

#endif // DYNO_DATA_H