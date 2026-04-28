#ifndef DATA_ACQ_TASK_H
#define DATA_ACQ_TASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t rpm;
    float speed;
    float wheelSpeed;
    float tps;
    float clt;
    float batteryVoltage;
    float oilPressure;
    bool oilPressure_flag;
    uint16_t bp;
} EcuData_t;

typedef struct {
    QueueHandle_t* dataQueue;
} DataAcqTaskParameters;

void DataAcqTask(void* pvParameters);

#endif // DATA_ACQ_TASK_H
