#ifndef TEST_CAN_TASK_H
#define TEST_CAN_TASK_H

#include <assert.h>
#include <stdint.h>

#include "Telemetry.h"

using QueueHandle_t = void*;
using TickType_t = uint32_t;
using BaseType_t = int;

constexpr BaseType_t pdTRUE = 1;
constexpr BaseType_t pdFALSE = 0;
constexpr TickType_t portMAX_DELAY = UINT32_MAX;

#define configASSERT(condition) assert(condition)
#define pdMS_TO_TICKS(value) static_cast<TickType_t>(value)

BaseType_t xQueueSend(QueueHandle_t queue, const void* packet, TickType_t waitTicks);
void vTaskDelay(TickType_t ticks);
void vTaskDelete(void* task);

struct CANTaskParameters {
    QueueHandle_t dataQueue;
};

void CANTask(void* parameters);

#endif
