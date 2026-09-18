#include <Arduino.h>
#include <RadioLib.h>

#include "LoRa.h"
#include "LoRaAPI.h"
#include "TelemetryCsv.h"
#include "AssertMsg.h"

// One tick is 1 ms in the ESP32-S3 Arduino configuration; never round to zero ticks.
#define LORA_TASK_PERIOD_TICKS 1
#define RX_BUFFER_LEN RADIOLIB_LR11X0_MAX_PACKET_LENGTH

static bool rxActive = false;
static uint32_t rxPacketCount = 0;
static bool csvHeaderPrinted = false;
// Only the LoRa task writes this buffer; keep the expanded CSV off its 4096-byte stack.
static char csvBuffer[TELEMETRY_CSV_BUFFER_SIZE];

static TelemetryRxMetadata rxMetadata(size_t radioLen, float rssi, float snr)
{
    TelemetryRxMetadata metadata = {};
    metadata.rx_count = rxPacketCount;
    metadata.rx_ms = millis();
    metadata.rssi_dbm = rssi;
    metadata.snr_db = snr;
    metadata.radio_len = radioLen;
    return metadata;
}

static void printCsvHeaderOnce()
{
    if (!csvHeaderPrinted) {
        csvHeaderPrinted = true;
        Serial.println(telemetryCsvHeader());
    }
}

static void printInvalidCsv(const TelemetryRxMetadata& metadata, int16_t errorCode, const char* errorText)
{
    if (formatTelemetryErrorCsv(csvBuffer, sizeof(csvBuffer), metadata, errorCode, errorText)) {
        Serial.println(csvBuffer);
    }
}

static int16_t startReceive()
{
    const int16_t state = LoRaApiStartReceive(LORA_RX_TIMEOUT_MS);
    rxActive = state == RADIOLIB_ERR_NONE;
    return state;
}

static void reportReceiveStartFailure(int16_t state)
{
    if (state != RADIOLIB_ERR_NONE && state != LORA_API_BUSY) {
        printInvalidCsv(rxMetadata(0, 0.0f, 0.0f), state, "start_receive_failed");
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void LoRaRxTaskPoll()
{
    if (!rxActive) {
        reportReceiveStartFailure(startReceive());
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
        ++rxPacketCount;
    }
    // A new receive can replace the radio's RSSI/SNR and blocking serial output can
    // delay this task. Preserve this packet's metadata, then listen immediately.
    TelemetryRxMetadata metadata = {};
    if (state != RADIOLIB_ERR_RX_TIMEOUT) {
        const float rssi = LoRaApiGetRSSI();
        const float snr = LoRaApiGetSNR();
        metadata = rxMetadata(rxLen, rssi, snr);
    }
    const int16_t rearmState = startReceive();

    if (state == RADIOLIB_ERR_NONE) {
        TelemetryReceivedPacket decoded = {};
        const TelemetryDecodeResult result = decodeTelemetryRadioPayload(rxBuffer, rxLen, decoded);
        if (result != TelemetryDecodeResult::Ok) {
            printInvalidCsv(metadata, -5, telemetryDecodeErrorText(result));
        } else if (formatTelemetryCsv(csvBuffer, sizeof(csvBuffer), decoded, metadata)) {
            Serial.println(csvBuffer);
        } else {
            printInvalidCsv(metadata, -6, "telemetry_csv_format_failed");
        }
    } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
        // Normal when no transmitter is active. Do not spam the CSV.
    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        printInvalidCsv(metadata, state, "radio_crc_or_header_mismatch");
    } else {
        printInvalidCsv(metadata, state, "receive_failed");
    }
    reportReceiveStartFailure(rearmState);
}

void LoRaTask(void* pvParameters)
{
    (void)pvParameters;
    const int16_t state = LoRaApiInit(false);
    ASSERT_MSG(state == RADIOLIB_ERR_NONE, "[LoRa][RX] LoRaApiInit failed: " + String(state));
    printCsvHeaderOnce();
    for (;;) {
        LoRaRxTaskPoll();
        vTaskDelay(LORA_TASK_PERIOD_TICKS);
    }
}
