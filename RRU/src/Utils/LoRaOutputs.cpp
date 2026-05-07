#include "LoRaOutputs.h"

static uint32_t rxPacketCount = 0;
static uint32_t txPacketCount = 0;

static bool LoRaOutputsPublishEvent(QueueHandle_t queue, const LoRaOutputEvent& event)
{
    if (queue == nullptr) {
        return false;
    }

    // Do not overwrite old events. If the queue is full, this drops the newest event.
    return xQueueSend(queue, &event, 0) == pdTRUE;
}

bool LoRaOutputsPublishRxPacket(QueueHandle_t queue, const char* payload, float rssi, float snr)
{
    LoRaOutputEvent event = {};

    event.type = LoRaOutputEventType::RxPacket;
    event.rssi = rssi;
    event.snr = snr;
    event.errorCode = 0;
    event.rxPacketCount = ++rxPacketCount;
    event.txPacketCount = txPacketCount;
    event.timestampMs = millis();

    if (payload != nullptr) {
        snprintf(event.payload, sizeof(event.payload), "%s", payload);
    } else {
        event.payload[0] = '\0';
    }

    return LoRaOutputsPublishEvent(queue, event);
}

bool LoRaOutputsPublishRxTimeout(QueueHandle_t queue, int16_t errorCode)
{
    LoRaOutputEvent event = {};

    event.type = LoRaOutputEventType::RxTimeout;
    event.errorCode = errorCode;
    event.rxPacketCount = rxPacketCount;
    event.txPacketCount = txPacketCount;
    event.timestampMs = millis();

    return LoRaOutputsPublishEvent(queue, event);
}

bool LoRaOutputsPublishRxCrcMismatch(QueueHandle_t queue, int16_t errorCode)
{
    LoRaOutputEvent event = {};

    event.type = LoRaOutputEventType::RxCrcMismatch;
    event.errorCode = errorCode;
    event.rxPacketCount = rxPacketCount;
    event.txPacketCount = txPacketCount;
    event.timestampMs = millis();

    return LoRaOutputsPublishEvent(queue, event);
}

bool LoRaOutputsPublishRxError(QueueHandle_t queue, int16_t errorCode)
{
    LoRaOutputEvent event = {};

    event.type = LoRaOutputEventType::RxError;
    event.errorCode = errorCode;
    event.rxPacketCount = rxPacketCount;
    event.txPacketCount = txPacketCount;
    event.timestampMs = millis();

    return LoRaOutputsPublishEvent(queue, event);
}

bool LoRaOutputsPublishTxStarted(QueueHandle_t queue, const char* payload)
{
    LoRaOutputEvent event = {};

    event.type = LoRaOutputEventType::TxStarted;
    event.errorCode = 0;
    event.rxPacketCount = rxPacketCount;
    event.txPacketCount = txPacketCount;
    event.timestampMs = millis();

    if (payload != nullptr) {
        snprintf(event.payload, sizeof(event.payload), "%s", payload);
    } else {
        event.payload[0] = '\0';
    }

    return LoRaOutputsPublishEvent(queue, event);
}

bool LoRaOutputsPublishTxDone(QueueHandle_t queue)
{
    LoRaOutputEvent event = {};

    event.type = LoRaOutputEventType::TxDone;
    event.errorCode = 0;
    event.rxPacketCount = rxPacketCount;
    event.txPacketCount = ++txPacketCount;
    event.timestampMs = millis();

    return LoRaOutputsPublishEvent(queue, event);
}

bool LoRaOutputsPublishTxError(QueueHandle_t queue, int16_t errorCode)
{
    LoRaOutputEvent event = {};

    event.type = LoRaOutputEventType::TxError;
    event.errorCode = errorCode;
    event.rxPacketCount = rxPacketCount;
    event.txPacketCount = txPacketCount;
    event.timestampMs = millis();

    return LoRaOutputsPublishEvent(queue, event);
}

bool LoRaOutputsReceive(QueueHandle_t queue, LoRaOutputEvent* event, TickType_t waitTicks)
{
    if (queue == nullptr || event == nullptr) {
        return false;
    }

    return xQueueReceive(queue, event, waitTicks) == pdTRUE;
}