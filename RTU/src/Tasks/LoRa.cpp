#include <Arduino.h>
#include "LoRa.h"
#include "AssertMsg.h"
#include "LoRaAPI.h"

#define LORA_TASK_PERIOD_MS      1000
#define LORA_RX_TIMEOUT_MS       5000

// true  = TX board
// false = RX board
static constexpr bool LORA_ROLE_TX = true;

static void LoRaTxLoop()
{
    static uint32_t txCount = 0;

    uint32_t nowMs = millis();

    // Fake test data for now
    uint16_t rpm = 2500 + ((txCount * 137) % 5000);

    float apps = txCount * 3.7f;
    while (apps > 100.0f) {
        apps -= 100.0f;
    }

    float brakePressure = (txCount % 20) * 2.5f;
    float coolantTemp = 55.0f + ((txCount % 30) * 0.8f);
    float batteryVoltage = 13.2f + ((txCount % 10) * 0.03f);

    const char* stateStr = "RUN";

    char payload[192];

    snprintf(
        payload,
        sizeof(payload),
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

    txCount++;

    Serial.print("[LoRaTask][TX] Transmitting: ");
    Serial.println(payload);

    int16_t state = LoRaApiTransmit(payload);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("[LoRaTask][TX] TX done");
    } else {
        Serial.print("[LoRaTask][TX] TX failed: ");
        Serial.println(state);
    }
}

static void LoRaRxLoop()
{
    String received;

    Serial.println("[LoRaTask][RX] Waiting for packet...");

    int16_t state = LoRaApiReceive(received, LORA_RX_TIMEOUT_MS);

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
            LoRaTxLoop();
        } else {
            LoRaRxLoop();
        }

        vTaskDelay(pdMS_TO_TICKS(LORA_TASK_PERIOD_MS));
    }
}