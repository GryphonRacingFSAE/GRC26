#ifndef CAN_TASK_H
#define CAN_TASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "Telemetry.h"

struct CANTaskParameters
{
    // Queue receiving TelemetryPacket objects.
    QueueHandle_t dataQueue;
};

void CANTask(void* pvParameters);

#endif // CAN_TASK_H
