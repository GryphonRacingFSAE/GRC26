#ifndef PERIPHERAL_TASK_H
#define PERIPHERAL_TASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

static constexpr int8_t NUM_LEDS = 16;
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

typedef struct {
    SemaphoreHandle_t* guiMutex;
} PeripheralTaskParameters;

void PeripheralTask(void* pvParameters);

#endif // PERIPHERAL_TASK_H
