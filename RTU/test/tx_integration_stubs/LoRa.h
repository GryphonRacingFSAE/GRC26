#pragma once
#include <Arduino.h>
#include "Telemetry.h"
struct LoRaTaskParameters {
    QueueHandle_t dataQueue;
};
