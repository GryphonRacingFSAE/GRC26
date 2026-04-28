#ifndef DATA_ACQ_TASK_H
#define DATA_ACQ_TASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

typedef struct {
    QueueHandle_t* dataQueue;
} DataAcqTaskParameters;

typedef struct {
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    bool updated;
} IMUData_t;

void DataAcqTask(void* pvParameters);

#endif // DATA_ACQ_TASK_H
