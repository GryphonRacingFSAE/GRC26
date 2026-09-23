#ifndef LORA_H
#define LORA_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "Telemetry.h"

struct LoRaTaskParameters
{
    // TX role: receives TelemetryPacket objects from CAN task.
    // RX role: may be nullptr; receiver prints decoded packets to Serial.
    QueueHandle_t dataQueue;
};

void LoRaTask(void* pvParameters);

#endif // LORA_H
