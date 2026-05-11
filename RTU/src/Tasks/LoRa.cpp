#include <Arduino.h>
#include <RadioLib.h>
#include <string.h>

#include "LoRa.h"
#include "AssertMsg.h"
#include "LoRaAPI.h"

static constexpr uint8_t TELEMETRY_MAGIC_0 = 'T';
static constexpr uint8_t TELEMETRY_MAGIC_1 = 'M';
static constexpr uint8_t TELEMETRY_VERSION = 1;
static constexpr size_t  TELEMETRY_MAX_RADIO_PAYLOAD = 96;

static uint32_t txCount = 0;
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

static const void* getPacketPayloadPtr(const TelemetryPacket& packet, size_t* len)
{
    if (len == nullptr) {
        return nullptr;
    }

    switch (packet.type) {
        case TELEMETRY_PACKET_FAST:
            *len = sizeof(TelemetryFastPacket);
            return &packet.data.fast;

        case TELEMETRY_PACKET_SLOW:
            *len = sizeof(TelemetrySlowPacket);
            return &packet.data.slow;

        case TELEMETRY_PACKET_EVENT:
            *len = sizeof(TelemetryEventPacket);
            return &packet.data.event;

        default:
            *len = 0;
            return nullptr;
    }
}

static bool buildRadioPayload(const TelemetryPacket& packet, uint8_t* out, size_t outSize, size_t* outLen)
{
    if (out == nullptr || outLen == nullptr) {
        return false;
    }

    *outLen = 0;

    size_t payloadLen = 0;
    const void* payloadPtr = getPacketPayloadPtr(packet, &payloadLen);

    if (payloadPtr == nullptr || payloadLen == 0) {
        return false;
    }

    // Header: magic[2], version[1], type[1], payload length[1]
    // Footer: CRC16 over header + payload
    const size_t headerLen = 5;
    const size_t crcLen = 2;
    const size_t totalLen = headerLen + payloadLen + crcLen;

    if (payloadLen > 255 || totalLen > outSize) {
        return false;
    }

    out[0] = TELEMETRY_MAGIC_0;
    out[1] = TELEMETRY_MAGIC_1;
    out[2] = TELEMETRY_VERSION;
    out[3] = packet.type;
    out[4] = (uint8_t)payloadLen;

    memcpy(&out[headerLen], payloadPtr, payloadLen);

    const uint16_t crc = crc16_ccitt(out, headerLen + payloadLen);
    out[headerLen + payloadLen] = (uint8_t)(crc & 0xFF);
    out[headerLen + payloadLen + 1] = (uint8_t)((crc >> 8) & 0xFF);

    *outLen = totalLen;
    return true;
}

static bool validateRadioPayload(const uint8_t* data, size_t len, uint8_t* packetType, const uint8_t** payload, size_t* payloadLen)
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

    if (data[0] != TELEMETRY_MAGIC_0 || data[1] != TELEMETRY_MAGIC_1 || data[2] != TELEMETRY_VERSION) {
        return false;
    }

    const size_t declaredLen = data[4];
    const size_t expectedLen = 5 + declaredLen + 2;

    if (len != expectedLen) {
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

    if (type == TELEMETRY_PACKET_FAST && payloadLen == sizeof(TelemetryFastPacket)) {
        TelemetryFastPacket p;
        memcpy(&p, payload, sizeof(p));

        Serial.print("  FAST seq="); Serial.print(p.seq);
        Serial.print(" rpm="); Serial.print(p.rpm);
        Serial.print(" tps_x10="); Serial.print(p.tps_x10);
        Serial.print(" map_x10="); Serial.print(p.map_kpa_x10);
        Serial.print(" lambda_x1000="); Serial.print(p.lambda_avg_x1000);
        Serial.print(" lambda_err_x1000="); Serial.print(p.lambda_error_x1000);
        Serial.print(" oilp_x10="); Serial.print(p.oil_pressure_kpa_x10);
        Serial.print(" fuelp_x10="); Serial.print(p.fuel_pressure_kpa_x10);
        Serial.print(" coolant_x10="); Serial.print(p.coolant_temp_c_x10);
        Serial.print(" batt_x100="); Serial.print(p.battery_v_x100);
        Serial.print(" speed_x10="); Serial.print(p.vehicle_speed_kph_x10);
        Serial.print(" gear="); Serial.print(p.gear);
        Serial.print(" status=0x"); Serial.println(p.status_bits, HEX);
    } else if (type == TELEMETRY_PACKET_SLOW && payloadLen == sizeof(TelemetrySlowPacket)) {
        TelemetrySlowPacket p;
        memcpy(&p, payload, sizeof(p));

        Serial.print("  SLOW seq="); Serial.print(p.seq);
        Serial.print(" oiltemp_x10="); Serial.print(p.oil_temp_c_x10);
        Serial.print(" iat_x10="); Serial.print(p.intake_air_temp_c_x10);
        Serial.print(" injduty_x10="); Serial.print(p.fuel_inj_duty_x10);
        Serial.print(" trim_x10="); Serial.print(p.fuel_trim_total_x10);
        Serial.print(" ecu_err="); Serial.print(p.ecu_error_count);
        Serial.print(" lost_sync="); Serial.print(p.ecu_lost_sync_count);
        Serial.print(" egt_high="); Serial.print(p.egt_highest_c);
        Serial.print(" knock_count="); Serial.println(p.knock_count);
    } else if (type == TELEMETRY_PACKET_EVENT && payloadLen == sizeof(TelemetryEventPacket)) {
        TelemetryEventPacket p;
        memcpy(&p, payload, sizeof(p));

        Serial.print("  EVENT seq="); Serial.print(p.seq);
        Serial.print(" flags=0x"); Serial.print(p.alert_flags, HEX);
        Serial.print(" status=0x"); Serial.print(p.status_bits, HEX);
        Serial.print(" rpm="); Serial.print(p.rpm);
        Serial.print(" oilp_x10="); Serial.print(p.oil_pressure_kpa_x10);
        Serial.print(" fuelp_x10="); Serial.print(p.fuel_pressure_kpa_x10);
        Serial.print(" coolant_x10="); Serial.print(p.coolant_temp_c_x10);
        Serial.print(" lambda_err_x1000="); Serial.println(p.lambda_error_x1000);
    }
}

static void transmitOnePacket(const TelemetryPacket& packet)
{
    uint8_t radioPayload[TELEMETRY_MAX_RADIO_PAYLOAD] = {0};
    size_t radioPayloadLen = 0;

    if (!buildRadioPayload(packet, radioPayload, sizeof(radioPayload), &radioPayloadLen)) {
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
            Serial.print("[LoRa][TX] done count=");
            Serial.print(txCount);
            Serial.print(" bytes=");
            Serial.println(radioPayloadLen);
        } else {
            Serial.print("[LoRa][TX] failed: ");
            Serial.println(state);
        }

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

    uint8_t buffer[TELEMETRY_MAX_RADIO_PAYLOAD] = {0};
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
