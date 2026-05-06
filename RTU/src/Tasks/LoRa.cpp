#include <Arduino.h>
#include <RadioLib.h>
#include "LoRa.h"
#include "AssertMsg.h"
#include "LoRaAPI.h"

#define LORA_TASK_PERIOD_MS      5
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
        "TEL|seq=%lu|ms=%lu|rpm=%u|apps=%.1f|brake=%.1f|temp=%.1f|vbat=%.2f|state=%s",
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

static void LoRaTxTaskPoll()
{
    if (!txActive) {
        uint32_t nowMs = millis();

        if ((uint32_t)(nowMs - lastTxMs) < LORA_TX_PERIOD_MS) {
            return;
        }

        char payload[192];

        BuildTelemetryPayload(payload, sizeof(payload));

        Serial.print("[LoRaTask][TX] Starting TX: ");
        Serial.println(payload);

        int16_t state = LoRaApiStartTransmit(payload);

        if (state == RADIOLIB_ERR_NONE) {
            txActive = true;
            lastTxMs = nowMs;
            txCount++;
        } else if (state == LORA_API_BUSY) {
            // Radio is busy; try again next task tick.
        } else {
            Serial.print("[LoRaTask][TX] start failed: ");
            Serial.println(state);
        }

        return;
    }

    int16_t state = LoRaApiPollTransmit();

    if (state == LORA_API_BUSY) {
        return;
    }

    txActive = false;

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("[LoRaTask][TX] TX done");
    } else {
        Serial.print("[LoRaTask][TX] TX failed: ");
        Serial.println(state);
    }
}

static void LoRaRxTaskPoll()
{
    if (!rxActive) {
        Serial.println("[LoRaTask][RX] Starting RX window...");

        int16_t state = LoRaApiStartReceive(LORA_RX_TIMEOUT_MS);

        if (state == RADIOLIB_ERR_NONE) {
            rxActive = true;
        } else if (state == LORA_API_BUSY) {
            // Radio is busy; try again next task tick.
        } else {
            Serial.print("[LoRaTask][RX] start failed: ");
            Serial.println(state);
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
        Serial.println("=============================");
        Serial.println("[LoRaTask][RX] PACKET RECEIVED");

        Serial.print("  Raw:  ");
        Serial.println(received);

        Serial.print("  RSSI: ");
        Serial.print(LoRaApiGetRSSI());
        Serial.println(" dBm");

        Serial.print("  SNR:  ");
        Serial.print(LoRaApiGetSNR());
        Serial.println(" dB");

        if (received.startsWith("TEL|")) {
            Serial.println("  Type: Telemetry packet");
        } else {
            Serial.println("  Type: Unknown packet");
        }

        Serial.println("=============================");
    } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
        Serial.println("[LoRaTask][RX] RX timeout — no packet received");
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println("[LoRaTask][RX] CRC mismatch — packet detected but corrupted");
    } else {
        Serial.print("[LoRaTask][RX] RX failed: ");
        Serial.println(state);
    }
}

void LoRaTask(void* pvParameters)
{
    LoRaTaskParameters* params = reinterpret_cast<LoRaTaskParameters*>(pvParameters);
    (void)params;

    Serial.println("[LoRaTask] Starting LoRa task...");

    int16_t state = LoRaApiInit(LORA_ROLE_TX);

    ASSERT_MSG(
        state == RADIOLIB_ERR_NONE,
        "[LoRaTask] LoRaApiInit failed: " + String(state)
    );

    for (;;) {
        if (LORA_ROLE_TX) {
            LoRaTxTaskPoll();
        } else {
            LoRaRxTaskPoll();
        }

        vTaskDelay(pdMS_TO_TICKS(LORA_TASK_PERIOD_MS));
    }
}