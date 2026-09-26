#include <Arduino.h>
#include <RadioLib.h>

#include "LoRa.h"
#include "AssertMsg.h"
#include "LoRaAPI.h"
#include "TelemetrySender.h"

static constexpr uint8_t TELEMETRY_MAGIC_0 = 'T';
static constexpr uint8_t TELEMETRY_MAGIC_1 = 'M';

static uint32_t txCount = 0;
static uint32_t lastTxReportMs = 0;
static uint32_t rxCount = 0;

static uint16_t crc16_ccitt(const uint8_t* data, size_t len)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;

        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

static bool validateRadioPayload(const uint8_t* data, size_t len, uint8_t* packetType, const uint8_t** payload,
                                 size_t* payloadLen)
{
    if (data == nullptr || packetType == nullptr || payload == nullptr || payloadLen == nullptr) {
        return false;
    }

    *packetType = 0;
    *payload = nullptr;
    *payloadLen = 0;

    if (len < 7) {
        return false;
    }

    if (data[0] != TELEMETRY_MAGIC_0 || data[1] != TELEMETRY_MAGIC_1 ||
        data[2] != TelemetryProtocol::VERSION) {
        return false;
    }

    const size_t declaredLen = data[4];
    const size_t expectedLen = 5 + declaredLen + 2;

    if (len != expectedLen) {
        return false;
    }

    size_t bodySize = 0;
    switch (data[3]) {
    case TELEMETRY_PACKET_FAST: bodySize = sizeof(TelemetryProtocol::FastPacket); break;
    case TELEMETRY_PACKET_SLOW: bodySize = sizeof(TelemetryProtocol::SlowPacket); break;
    case TELEMETRY_PACKET_EVENT: bodySize = sizeof(TelemetryProtocol::EventPacket); break;
    case TELEMETRY_PACKET_SENSORS: bodySize = sizeof(TelemetryProtocol::SensorsPacket); break;
    default: return false;
    }
    if (declaredLen != bodySize) {
        return false;
    }

    const uint16_t rxCrc = (uint16_t)data[len - 2] | ((uint16_t)data[len - 1] << 8);
    const uint16_t calcCrc = crc16_ccitt(data, len - 2);

    if (rxCrc != calcCrc) {
        return false;
    }

    *packetType = data[3];
    *payload = &data[5];
    *payloadLen = declaredLen;
    return true;
}

static void printPacketSummary(uint8_t type, const uint8_t* payload, size_t payloadLen, float rssi, float snr)
{
    Serial.print("[LoRa][RX] type=");
    Serial.print(type);
    Serial.print(" len=");
    Serial.print(payloadLen);
    Serial.print(" rssi=");
    Serial.print(rssi);
    Serial.print(" snr=");
    Serial.println(snr);

    // Every validated snapshot has the same ten-byte receipt/freshness header.
    const uint16_t sequence = static_cast<uint16_t>(payload[4]) | (static_cast<uint16_t>(payload[5]) << 8);
    const uint16_t received = static_cast<uint16_t>(payload[6]) | (static_cast<uint16_t>(payload[7]) << 8);
    const uint16_t fresh = static_cast<uint16_t>(payload[8]) | (static_cast<uint16_t>(payload[9]) << 8);
    Serial.print("  seq=");
    Serial.print(sequence);
    Serial.print(" received=0x");
    Serial.print(received, HEX);
    Serial.print(" fresh=0x");
    Serial.println(fresh, HEX);
    // Diagnostic RX role: expose each complete body independently.
    Serial.print("  body=");
    for (size_t i = 10; i < payloadLen; ++i) {
        Serial.print(payload[i], HEX);
        Serial.print(" ");
    }
    Serial.println("");
}

static void transmitOnePacket(const TelemetryPacket& packet)
{
    uint8_t radioPayload[TelemetryProtocol::MAX_RADIO_PAYLOAD] = {0};
    size_t radioPayloadLen = 0;

    if (!buildTelemetryRadioPayload(packet, radioPayload, sizeof(radioPayload), &radioPayloadLen)) {
        Serial.println("[LoRa][TX] Failed to build radio payload");
        return;
    }

    int16_t state = LoRaApiStartTransmit(radioPayload, radioPayloadLen);

    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("[LoRa][TX] startTransmit failed: ");
        Serial.println(state);
        return;
    }

    while (true) {
        state = LoRaApiPollTransmit();

        if (state == LORA_API_BUSY) {
            vTaskDelay(pdMS_TO_TICKS(LORA_TX_POLL_DELAY_MS));
            continue;
        }

        if (state == RADIOLIB_ERR_NONE) {
            txCount++;
            const uint32_t now = millis();
            if ((uint32_t)(now - lastTxReportMs) >= 1000u) {
                lastTxReportMs = now;
                Serial.print("[LoRa][TX] done count=");
                Serial.print(txCount);
                Serial.print(" bytes=");
                Serial.println(radioPayloadLen);
            }
        } else {
            Serial.print("[LoRa][TX] failed: ");
            Serial.println(state);
        }

        // Give the polled RRU time to read the completed frame and rearm before
        // another queued packet starts. This also applies to failed transmissions.
        const TickType_t gapTicks = pdMS_TO_TICKS(LORA_TX_GAP_MS);
        vTaskDelay(gapTicks > 0 ? gapTicks : 1);
        return;
    }
}

static void runTxTask(QueueHandle_t telemetryQueue)
{
    TelemetryPacket packet = {};

    // Event-driven behavior: sleep here until CAN task publishes telemetry.
    if (xQueueReceive(telemetryQueue, &packet, portMAX_DELAY) != pdTRUE) {
        return;
    }

    transmitOnePacket(packet);
}

static void runRxTask()
{
    static bool rxActive = false;

    if (!rxActive) {
        const int16_t state = LoRaApiStartReceive(LORA_RX_TIMEOUT_MS);

        if (state == RADIOLIB_ERR_NONE) {
            rxActive = true;
        } else if (state != LORA_API_BUSY) {
            Serial.print("[LoRa][RX] startReceive failed: ");
            Serial.println(state);
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        return;
    }

    uint8_t buffer[TelemetryProtocol::MAX_RADIO_PAYLOAD] = {0};
    size_t receivedLen = 0;
    const int16_t state = LoRaApiPollReceive(buffer, sizeof(buffer), &receivedLen);

    if (state == LORA_API_BUSY) {
        vTaskDelay(pdMS_TO_TICKS(5));
        return;
    }

    rxActive = false;

    if (state == RADIOLIB_ERR_NONE) {
        rxCount++;

        uint8_t packetType = 0;
        const uint8_t* payload = nullptr;
        size_t payloadLen = 0;

        if (validateRadioPayload(buffer, receivedLen, &packetType, &payload, &payloadLen)) {
            printPacketSummary(packetType, payload, payloadLen, LoRaApiGetRSSI(), LoRaApiGetSNR());
        } else {
            Serial.print("[LoRa][RX] invalid payload len=");
            Serial.println(receivedLen);
        }
    } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
        // Normal on RX when no transmitter is active.
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println("[LoRa][RX] CRC mismatch");
    } else {
        Serial.print("[LoRa][RX] failed: ");
        Serial.println(state);
    }
}

void LoRaTask(void* pvParameters)
{
    LoRaTaskParameters* params = reinterpret_cast<LoRaTaskParameters*>(pvParameters);

    configASSERT(params != nullptr);

#if LORA_ROLE_TX
    configASSERT(params->dataQueue != nullptr);
#endif

    Serial.println("[LoRa] Task started");

    const int16_t initState = LoRaApiInit(LORA_ROLE_TX != 0);
    ASSERT_MSG(initState == RADIOLIB_ERR_NONE, "[LoRa] LoRaApiInit failed: " + String(initState));

    Serial.println("[LoRa] Radio initialized");

    for (;;) {
#if LORA_ROLE_TX
        runTxTask(params->dataQueue);
#else
        runRxTask();
#endif
    }
}
