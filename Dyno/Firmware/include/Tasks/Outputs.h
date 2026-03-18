#ifndef OUTPUTS_H
#define OUTPUTS_H

#include "DynoData.h" 

typedef struct 
{
    QueueHandle_t* dynoQueue;  
} OutputsTaskParameters;

void OutputsTask(void* pvParameters);

#endif // OUTPUTS_H
