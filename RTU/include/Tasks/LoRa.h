#ifndef LORA_H
#define LORA_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

typedef struct 
{
    QueueHandle_t* dataQueue;
} LoRaTaskParameters;


/// @brief Initializes the LoRa module, required before communication can be established
/// @warning If LoRa module fails to initialize, MCU will hard fault.
void LoRaInit();

/// @brief LoRa Task, handles all communication with the LoRa module
/// @param pvParameters LoRaTaskParameters
void LoRaTask(void* pvParameters);

#endif // LORA_H