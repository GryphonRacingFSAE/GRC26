#include <Arduino.h>
#include <RadioLib.h>
#include "LoRa.h"
#include "AssertMsg.h"
#include "LoRaAPI.h"
#include "LoRaOutputs.h"

#define LORA_TASK_PERIOD_MS      10
#define LORA_TX_PERIOD_MS        1000
#define LORA_RX_TIMEOUT_MS       5000

// true  = TX board
// false = RX board
static constexpr bool LORA_ROLE_TX = true;

static bool txActive = false;
static bool rxActive = false;

static uint32_t lastTxMs = 0;
static uint32_t txCount = 0;

static void BuildTelemetryPayload(char* payload, size_t payloadSize)
{
    uint32_t nowMs = millis();

    uint16_t rpm = 2500 + ((txCount * 137) % 5000);

    float apps = txCount * 3.7f;
    while (apps > 100.0f) {
        apps -= 100.0f;
    }

    float brakePressure = (txCount % 20) * 2.5f;
    float coolantTemp = 55.0f + ((txCount % 30) * 0.8f);
    float batteryVoltage = 13.2f + ((txCount % 10) * 0.03f);

    const char* stateStr = "RUN";

    snprintf(
    payload,
    payloadSize,
    "TEL,%lu,%lu,%u,%.1f,%.1f,%.1f,%.2f,%s",
    static_cast<unsigned long>(txCount),
    static_cast<unsigned long>(nowMs),
    rpm,
    apps,
    brakePressure,
    coolantTemp,
    batteryVoltage,
    stateStr
    );
}

static void LoRaTxTaskPoll(QueueHandle_t outputQueue)
{
    if (!txActive) {
        uint32_t nowMs = millis();

        if ((uint32_t)(nowMs - lastTxMs) < LORA_TX_PERIOD_MS) {
            return;
        }

        char payload[LORA_OUTPUT_MAX_PAYLOAD_LEN];

        BuildTelemetryPayload(payload, sizeof(payload));

        int16_t state = LoRaApiStartTransmit(payload);

        if (state == RADIOLIB_ERR_NONE) {
            txActive = true;
            lastTxMs = nowMs;
            txCount++;

            LoRaOutputsPublishTxStarted(outputQueue, payload);
        } else if (state == LORA_API_BUSY) {
            return;
        } else {
            LoRaOutputsPublishTxError(outputQueue, state);
        }

        return;
    }

    int16_t state = LoRaApiPollTransmit();

    if (state == LORA_API_BUSY) {
        return;
    }

    txActive = false;

    if (state == RADIOLIB_ERR_NONE) {
        LoRaOutputsPublishTxDone(outputQueue);
    } else {
        LoRaOutputsPublishTxError(outputQueue, state);
    }
}

static void LoRaRxTaskPoll(QueueHandle_t outputQueue)
{
    if (!rxActive) {
        int16_t state = LoRaApiStartReceive(LORA_RX_TIMEOUT_MS);

        if (state == RADIOLIB_ERR_NONE) {
            rxActive = true;
        } else if (state == LORA_API_BUSY) {
            return;
        } else {
            LoRaOutputsPublishRxError(outputQueue, state);
        }

        return;
    }

    String received;
    int16_t state = LoRaApiPollReceive(received);

    if (state == LORA_API_BUSY) {
        return;
    }

    rxActive = false;

    if (state == RADIOLIB_ERR_NONE) {
        LoRaOutputsPublishRxPacket(
            outputQueue,
            received.c_str(),
            LoRaApiGetRSSI(),
            LoRaApiGetSNR()
        );
    } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
        LoRaOutputsPublishRxTimeout(outputQueue, state);
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        LoRaOutputsPublishRxCrcMismatch(outputQueue, state);
    } else {
        LoRaOutputsPublishRxError(outputQueue, state);
    }
}

void LoRaTask(void* pvParameters)
{
    LoRaTaskParameters* params = reinterpret_cast<LoRaTaskParameters*>(pvParameters);

    configASSERT(params != nullptr);
    configASSERT(params->dataQueue != nullptr);

    QueueHandle_t outputQueue = params->dataQueue;

    // Serial.println("[LoRaTask] Starting LoRa task...");

    int16_t state = LoRaApiInit(LORA_ROLE_TX);

    ASSERT_MSG(
        state == RADIOLIB_ERR_NONE,
        "[LoRaTask] LoRaApiInit failed: " + String(state)
    );

    for (;;) {
        if (LORA_ROLE_TX) {
            LoRaTxTaskPoll(outputQueue);
        } else {
            LoRaRxTaskPoll(outputQueue);
        }

        vTaskDelay(pdMS_TO_TICKS(LORA_TASK_PERIOD_MS));
    }
}