#ifndef CAN_H
#define CAN_H

#include "DynoData.h" 

typedef struct 
{
    QueueHandle_t* dynoQueue;
} CANTaskParameters;

void CANTask(void* pvParameters);

#endif // CAN_H
