#include <Arduino.h>
#include <RadioLib.h>
#include <string.h>

#include "LoRa.h"
#include "LoRaAPI.h"
#include "Telemetry.h"
#include "AssertMsg.h"

#define LORA_TASK_PERIOD_MS      5
#define RX_BUFFER_LEN            RADIOLIB_LR11X0_MAX_PACKET_LENGTH

static constexpr uint8_t TELEMETRY_MAGIC_0 = 'T';
static constexpr uint8_t TELEMETRY_MAGIC_1 = 'M';
static constexpr uint8_t TELEMETRY_VERSION = 1;

static bool rxActive = false;
static uint32_t rxPacketCount = 0;
static bool csvHeaderPrinted = false;

struct CsvRowFields
{
    char packet_type[12];
    char seq[12];
    char tx_ms[16];
    char alert_flags_hex[12];
    char status_bits_hex[12];
    char rpm[12];
    char tps_pct[16];
    char map_kpa[16];
    char lambda_avg[16];
    char lambda_error[16];
    char oil_pressure_kpa[16];
    char fuel_pressure_kpa[16];
    char coolant_temp_c[16];
    char battery_v[16];
    char vehicle_speed_kph[16];
    char gear[12];
    char oil_temp_c[16];
    char intake_air_temp_c[16];
    char fuel_inj_duty_pct[16];
    char fuel_trim_total_pct[16];
    char lambda_corr_a_pct[16];
    char lambda_corr_b_pct[16];
    char ignition_timing_deg[16];
    char ignition_cut_pct[16];
    char fuel_cut_pct[16];
    char ecu_error_count[12];
    char ecu_lost_sync_count[12];
    char ecu_temp_c[16];
    char egt_highest_c[16];
    char egt_delta_c[16];
    char knock_count[12];
    char knock_correction_deg[16];
    char boost_target_kpa[16];
    char boost_duty_pct[16];
    char coolant_pressure_kpa[16];
    char wastegate_pressure_kpa[16];
    char error_code[12];
    char error_text[48];
};

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

static void fmt_u32(char* dst, size_t dstSize, uint32_t value)
{
    snprintf(dst, dstSize, "%lu", static_cast<unsigned long>(value));
}

static void fmt_i32(char* dst, size_t dstSize, int32_t value)
{
    snprintf(dst, dstSize, "%ld", static_cast<long>(value));
}

static void fmt_float(char* dst, size_t dstSize, float value, uint8_t decimals)
{
    switch (decimals) {
        case 1:
            snprintf(dst, dstSize, "%.1f", value);
            break;
        case 2:
            snprintf(dst, dstSize, "%.2f", value);
            break;
        case 3:
            snprintf(dst, dstSize, "%.3f", value);
            break;
        default:
            snprintf(dst, dstSize, "%f", value);
            break;
    }
}

static void fmt_hex16(char* dst, size_t dstSize, uint16_t value)
{
    snprintf(dst, dstSize, "0x%04X", value);
}

static bool validateRadioPayload(
    const uint8_t* data,
    size_t len,
    uint8_t* packetType,
    const uint8_t** payload,
    size_t* payloadLen
)
{
    if (data == nullptr || packetType == nullptr || payload == nullptr || payloadLen == nullptr) {
        return false;
    }

    *packetType = 0;
    *payload = nullptr;
    *payloadLen = 0;

    // Header: magic[2], version[1], type[1], payload length[1]
    // Footer: CRC16 little-endian over header + payload.
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

static void printCsvHeaderOnce()
{
    if (csvHeaderPrinted) {
        return;
    }

    csvHeaderPrinted = true;

    Serial.println(
        "event,"
        "rx_count,"
        "rx_ms,"
        "rssi_dbm,"
        "snr_db,"
        "radio_len,"
        "packet_type,"
        "seq,"
        "tx_ms,"
        "alert_flags_hex,"
        "status_bits_hex,"
        "rpm,"
        "tps_pct,"
        "map_kpa,"
        "lambda_avg,"
        "lambda_error,"
        "oil_pressure_kpa,"
        "fuel_pressure_kpa,"
        "coolant_temp_c,"
        "battery_v,"
        "vehicle_speed_kph,"
        "gear,"
        "oil_temp_c,"
        "intake_air_temp_c,"
        "fuel_inj_duty_pct,"
        "fuel_trim_total_pct,"
        "lambda_corr_a_pct,"
        "lambda_corr_b_pct,"
        "ignition_timing_deg,"
        "ignition_cut_pct,"
        "fuel_cut_pct,"
        "ecu_error_count,"
        "ecu_lost_sync_count,"
        "ecu_temp_c,"
        "egt_highest_c,"
        "egt_delta_c,"
        "knock_count,"
        "knock_correction_deg,"
        "boost_target_kpa,"
        "boost_duty_pct,"
        "coolant_pressure_kpa,"
        "wastegate_pressure_kpa,"
        "error_code,"
        "error_text"
    );
}

static void printCsvRow(
    const char* eventName,
    size_t radioLen,
    float rssi,
    float snr,
    const CsvRowFields& row
)
{
    Serial.printf(
        "%s,%lu,%lu,%.2f,%.2f,%u,"
        "%s,%s,%s,%s,%s,"
        "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,"
        "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,"
        "%s,%s\n",
        eventName,
        static_cast<unsigned long>(rxPacketCount),
        static_cast<unsigned long>(millis()),
        rssi,
        snr,
        static_cast<unsigned int>(radioLen),
        row.packet_type,
        row.seq,
        row.tx_ms,
        row.alert_flags_hex,
        row.status_bits_hex,
        row.rpm,
        row.tps_pct,
        row.map_kpa,
        row.lambda_avg,
        row.lambda_error,
        row.oil_pressure_kpa,
        row.fuel_pressure_kpa,
        row.coolant_temp_c,
        row.battery_v,
        row.vehicle_speed_kph,
        row.gear,
        row.oil_temp_c,
        row.intake_air_temp_c,
        row.fuel_inj_duty_pct,
        row.fuel_trim_total_pct,
        row.lambda_corr_a_pct,
        row.lambda_corr_b_pct,
        row.ignition_timing_deg,
        row.ignition_cut_pct,
        row.fuel_cut_pct,
        row.ecu_error_count,
        row.ecu_lost_sync_count,
        row.ecu_temp_c,
        row.egt_highest_c,
        row.egt_delta_c,
        row.knock_count,
        row.knock_correction_deg,
        row.boost_target_kpa,
        row.boost_duty_pct,
        row.coolant_pressure_kpa,
        row.wastegate_pressure_kpa,
        row.error_code,
        row.error_text
    );
}

static void printInvalidCsv(size_t radioLen, float rssi, float snr, int16_t errorCode, const char* errorText)
{
    CsvRowFields row = {};
    snprintf(row.packet_type, sizeof(row.packet_type), "UNKNOWN");
    fmt_i32(row.error_code, sizeof(row.error_code), errorCode);
    snprintf(row.error_text, sizeof(row.error_text), "%s", errorText != nullptr ? errorText : "unknown");
    printCsvRow("rx_error", radioLen, rssi, snr, row);
}

static void printFastCsv(const TelemetryFastPacket& p, size_t radioLen, float rssi, float snr)
{
    CsvRowFields row = {};

    snprintf(row.packet_type, sizeof(row.packet_type), "FAST");
    fmt_u32(row.seq, sizeof(row.seq), p.seq);
    fmt_u32(row.tx_ms, sizeof(row.tx_ms), p.ms);
    fmt_hex16(row.status_bits_hex, sizeof(row.status_bits_hex), p.status_bits);
    fmt_u32(row.rpm, sizeof(row.rpm), p.rpm);
    fmt_float(row.tps_pct, sizeof(row.tps_pct), p.tps_x10 / 10.0f, 1);
    fmt_float(row.map_kpa, sizeof(row.map_kpa), p.map_kpa_x10 / 10.0f, 1);
    fmt_float(row.lambda_avg, sizeof(row.lambda_avg), p.lambda_avg_x1000 / 1000.0f, 3);
    fmt_float(row.lambda_error, sizeof(row.lambda_error), p.lambda_error_x1000 / 1000.0f, 3);
    fmt_float(row.oil_pressure_kpa, sizeof(row.oil_pressure_kpa), p.oil_pressure_kpa_x10 / 10.0f, 1);
    fmt_float(row.fuel_pressure_kpa, sizeof(row.fuel_pressure_kpa), p.fuel_pressure_kpa_x10 / 10.0f, 1);
    fmt_float(row.coolant_temp_c, sizeof(row.coolant_temp_c), p.coolant_temp_c_x10 / 10.0f, 1);
    fmt_float(row.battery_v, sizeof(row.battery_v), p.battery_v_x100 / 100.0f, 2);
    fmt_float(row.vehicle_speed_kph, sizeof(row.vehicle_speed_kph), p.vehicle_speed_kph_x10 / 10.0f, 1);
    fmt_i32(row.gear, sizeof(row.gear), p.gear);

    printCsvRow("rx_packet", radioLen, rssi, snr, row);
}

static void printSlowCsv(const TelemetrySlowPacket& p, size_t radioLen, float rssi, float snr)
{
    CsvRowFields row = {};

    snprintf(row.packet_type, sizeof(row.packet_type), "SLOW");
    fmt_u32(row.seq, sizeof(row.seq), p.seq);
    fmt_u32(row.tx_ms, sizeof(row.tx_ms), p.ms);
    fmt_float(row.oil_temp_c, sizeof(row.oil_temp_c), p.oil_temp_c_x10 / 10.0f, 1);
    fmt_float(row.intake_air_temp_c, sizeof(row.intake_air_temp_c), p.intake_air_temp_c_x10 / 10.0f, 1);
    fmt_float(row.fuel_inj_duty_pct, sizeof(row.fuel_inj_duty_pct), p.fuel_inj_duty_x10 / 10.0f, 1);
    fmt_float(row.fuel_trim_total_pct, sizeof(row.fuel_trim_total_pct), p.fuel_trim_total_x10 / 10.0f, 1);
    fmt_float(row.lambda_corr_a_pct, sizeof(row.lambda_corr_a_pct), p.lambda_corr_a_x10 / 10.0f, 1);
    fmt_float(row.lambda_corr_b_pct, sizeof(row.lambda_corr_b_pct), p.lambda_corr_b_x10 / 10.0f, 1);
    fmt_float(row.ignition_timing_deg, sizeof(row.ignition_timing_deg), p.ignition_timing_deg_x10 / 10.0f, 1);
    fmt_u32(row.ignition_cut_pct, sizeof(row.ignition_cut_pct), p.ignition_cut_percent);
    fmt_u32(row.fuel_cut_pct, sizeof(row.fuel_cut_pct), p.fuel_cut_percent);
    fmt_u32(row.ecu_error_count, sizeof(row.ecu_error_count), p.ecu_error_count);
    fmt_u32(row.ecu_lost_sync_count, sizeof(row.ecu_lost_sync_count), p.ecu_lost_sync_count);
    fmt_u32(row.ecu_temp_c, sizeof(row.ecu_temp_c), p.ecu_temp_c);
    fmt_u32(row.egt_highest_c, sizeof(row.egt_highest_c), p.egt_highest_c);
    fmt_u32(row.egt_delta_c, sizeof(row.egt_delta_c), p.egt_delta_c);
    fmt_u32(row.knock_count, sizeof(row.knock_count), p.knock_count);
    fmt_float(row.knock_correction_deg, sizeof(row.knock_correction_deg), p.knock_correction_deg_x10 / 10.0f, 1);
    fmt_float(row.boost_target_kpa, sizeof(row.boost_target_kpa), p.boost_target_kpa_x10 / 10.0f, 1);
    fmt_float(row.boost_duty_pct, sizeof(row.boost_duty_pct), p.boost_duty_x10 / 10.0f, 1);
    fmt_float(row.coolant_pressure_kpa, sizeof(row.coolant_pressure_kpa), p.coolant_pressure_kpa_x10 / 10.0f, 1);
    fmt_float(row.wastegate_pressure_kpa, sizeof(row.wastegate_pressure_kpa), p.wastegate_pressure_kpa_x10 / 10.0f, 1);

    printCsvRow("rx_packet", radioLen, rssi, snr, row);
}

static void printEventCsv(const TelemetryEventPacket& p, size_t radioLen, float rssi, float snr)
{
    CsvRowFields row = {};

    snprintf(row.packet_type, sizeof(row.packet_type), "EVENT");
    fmt_u32(row.seq, sizeof(row.seq), p.seq);
    fmt_u32(row.tx_ms, sizeof(row.tx_ms), p.ms);
    fmt_hex16(row.alert_flags_hex, sizeof(row.alert_flags_hex), p.alert_flags);
    fmt_hex16(row.status_bits_hex, sizeof(row.status_bits_hex), p.status_bits);
    fmt_u32(row.rpm, sizeof(row.rpm), p.rpm);
    fmt_float(row.lambda_error, sizeof(row.lambda_error), p.lambda_error_x1000 / 1000.0f, 3);
    fmt_float(row.oil_pressure_kpa, sizeof(row.oil_pressure_kpa), p.oil_pressure_kpa_x10 / 10.0f, 1);
    fmt_float(row.fuel_pressure_kpa, sizeof(row.fuel_pressure_kpa), p.fuel_pressure_kpa_x10 / 10.0f, 1);
    fmt_float(row.coolant_temp_c, sizeof(row.coolant_temp_c), p.coolant_temp_c_x10 / 10.0f, 1);
    fmt_float(row.battery_v, sizeof(row.battery_v), p.battery_v_x100 / 100.0f, 2);
    fmt_u32(row.ecu_error_count, sizeof(row.ecu_error_count), p.ecu_error_count);
    fmt_u32(row.ecu_lost_sync_count, sizeof(row.ecu_lost_sync_count), p.ecu_lost_sync_count);
    fmt_u32(row.knock_count, sizeof(row.knock_count), p.knock_count);

    printCsvRow("rx_packet", radioLen, rssi, snr, row);
}

static void printDecodedTelemetryCsv(
    uint8_t packetType,
    const uint8_t* payload,
    size_t payloadLen,
    size_t radioLen,
    float rssi,
    float snr
)
{
    switch (packetType) {
        case TELEMETRY_PACKET_FAST:
        {
            if (payloadLen != sizeof(TelemetryFastPacket)) {
                printInvalidCsv(radioLen, rssi, snr, -1, "fast_payload_size_mismatch");
                return;
            }

            TelemetryFastPacket p = {};
            memcpy(&p, payload, sizeof(p));
            printFastCsv(p, radioLen, rssi, snr);
            return;
        }

        case TELEMETRY_PACKET_SLOW:
        {
            if (payloadLen != sizeof(TelemetrySlowPacket)) {
                printInvalidCsv(radioLen, rssi, snr, -2, "slow_payload_size_mismatch");
                return;
            }

            TelemetrySlowPacket p = {};
            memcpy(&p, payload, sizeof(p));
            printSlowCsv(p, radioLen, rssi, snr);
            return;
        }

        case TELEMETRY_PACKET_EVENT:
        {
            if (payloadLen != sizeof(TelemetryEventPacket)) {
                printInvalidCsv(radioLen, rssi, snr, -3, "event_payload_size_mismatch");
                return;
            }

            TelemetryEventPacket p = {};
            memcpy(&p, payload, sizeof(p));
            printEventCsv(p, radioLen, rssi, snr);
            return;
        }

        default:
        {
            printInvalidCsv(radioLen, rssi, snr, -4, "unknown_packet_type");
            return;
        }
    }
}

static void LoRaRxTaskPoll()
{
    if (!rxActive) {
        int16_t state = LoRaApiStartReceive(LORA_RX_TIMEOUT_MS);

        if (state == RADIOLIB_ERR_NONE) {
            rxActive = true;
        } else if (state == LORA_API_BUSY) {
            return;
        } else {
            printInvalidCsv(0, 0.0f, 0.0f, state, "start_receive_failed");
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        return;
    }

    uint8_t rxBuffer[RX_BUFFER_LEN] = {0};
    size_t rxLen = 0;

    const int16_t state = LoRaApiPollReceive(rxBuffer, sizeof(rxBuffer), &rxLen);

    if (state == LORA_API_BUSY) {
        return;
    }

    rxActive = false;

    if (state == RADIOLIB_ERR_NONE) {
        rxPacketCount++;

        uint8_t packetType = 0;
        const uint8_t* payload = nullptr;
        size_t payloadLen = 0;

        const float rssi = LoRaApiGetRSSI();
        const float snr = LoRaApiGetSNR();

        if (!validateRadioPayload(rxBuffer, rxLen, &packetType, &payload, &payloadLen)) {
            printInvalidCsv(rxLen, rssi, snr, -5, "telemetry_crc_or_header_invalid");
            return;
        }

        printDecodedTelemetryCsv(packetType, payload, payloadLen, rxLen, rssi, snr);
    } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
        // Normal when no transmitter is active. Do not spam the CSV.
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        printInvalidCsv(rxLen, LoRaApiGetRSSI(), LoRaApiGetSNR(), state, "radio_crc_or_header_mismatch");
    } else {
        printInvalidCsv(rxLen, LoRaApiGetRSSI(), LoRaApiGetSNR(), state, "receive_failed");
    }
}

void LoRaTask(void* pvParameters)
{
    (void)pvParameters;

    const int16_t state = LoRaApiInit(false);

    ASSERT_MSG(
        state == RADIOLIB_ERR_NONE,
        "[LoRa][RX] LoRaApiInit failed: " + String(state)
    );

    printCsvHeaderOnce();

    for (;;) {
        LoRaRxTaskPoll();
        vTaskDelay(pdMS_TO_TICKS(LORA_TASK_PERIOD_MS));
    }
}
