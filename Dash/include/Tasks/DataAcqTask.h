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
    uint16_t speed;
    uint16_t wheelSpeed;
    uint16_t tps;
    uint16_t clt;
    uint16_t oilPressure;
    bool oilPressure_flag;
    uint8_t bp;
    uint16_t apps;
} EcuData_t;

typedef struct {
    QueueHandle_t* dataQueue;
} DataAcqTaskParameters;

void DataAcqTask(void* pvParameters);

#endif // DATA_ACQ_TASK_H
