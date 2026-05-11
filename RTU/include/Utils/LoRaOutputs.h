#ifndef LORA_OUTPUTS_H
#define LORA_OUTPUTS_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#define LORA_OUTPUT_MAX_PAYLOAD_LEN 128

enum class LoRaOutputEventType : uint8_t
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

#endif // LORA_OUTPUTS_H
