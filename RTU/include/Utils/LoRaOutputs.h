#pragma once

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define LORA_OUTPUT_MAX_PAYLOAD_LEN 192

enum class LoRaOutputEventType 
{
    RxPacket,
    RxTimeout,
    RxCrcMismatch,
    RxError,
    TxStarted,
    TxDone,
    TxError
};

struct LoRaOutputEvent 
{
    LoRaOutputEventType type;

    char payload[LORA_OUTPUT_MAX_PAYLOAD_LEN];

    float rssi;
    float snr;

    int16_t errorCode;

    uint32_t rxPacketCount;
    uint32_t txPacketCount;

    uint32_t timestampMs;
};

bool LoRaOutputsPublishRxPacket(QueueHandle_t queue, const char* payload, float rssi, float snr);
bool LoRaOutputsPublishRxTimeout(QueueHandle_t queue, int16_t errorCode);
bool LoRaOutputsPublishRxCrcMismatch(QueueHandle_t queue, int16_t errorCode);
bool LoRaOutputsPublishRxError(QueueHandle_t queue, int16_t errorCode);

bool LoRaOutputsPublishTxStarted(QueueHandle_t queue, const char* payload);
bool LoRaOutputsPublishTxDone(QueueHandle_t queue);
bool LoRaOutputsPublishTxError(QueueHandle_t queue, int16_t errorCode);

bool LoRaOutputsReceive(QueueHandle_t queue, LoRaOutputEvent* event, TickType_t waitTicks);