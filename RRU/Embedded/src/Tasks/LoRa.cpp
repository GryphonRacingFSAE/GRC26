#include <Arduino.h>
#include <RadioLib.h>

#include "LoRa.h"
#include "LoRaAPI.h"
#include "TelemetryCsv.h"
#include "AssertMsg.h"

#define LORA_TASK_PERIOD_MS 5
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

static void printInvalidCsv(size_t radioLen, float rssi, float snr, int16_t errorCode, const char* errorText)
{
    const TelemetryRxMetadata metadata = rxMetadata(radioLen, rssi, snr);
    if (formatTelemetryErrorCsv(csvBuffer, sizeof(csvBuffer), metadata, errorCode, errorText)) {
        Serial.println(csvBuffer);
    }
}

static void LoRaRxTaskPoll()
{
    if (!rxActive) {
        const int16_t state = LoRaApiStartReceive(LORA_RX_TIMEOUT_MS);
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
        ++rxPacketCount;
        const float rssi = LoRaApiGetRSSI();
        const float snr = LoRaApiGetSNR();
        TelemetryReceivedPacket decoded = {};
        const TelemetryDecodeResult result = decodeTelemetryRadioPayload(rxBuffer, rxLen, decoded);
        if (result != TelemetryDecodeResult::Ok) {
            printInvalidCsv(rxLen, rssi, snr, -5, telemetryDecodeErrorText(result));
            return;
        }
        const TelemetryRxMetadata metadata = rxMetadata(rxLen, rssi, snr);
        if (formatTelemetryCsv(csvBuffer, sizeof(csvBuffer), decoded, metadata)) {
            Serial.println(csvBuffer);
        } else {
            printInvalidCsv(rxLen, rssi, snr, -6, "telemetry_csv_format_failed");
        }
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
    ASSERT_MSG(state == RADIOLIB_ERR_NONE, "[LoRa][RX] LoRaApiInit failed: " + String(state));
    printCsvHeaderOnce();
    for (;;) {
        LoRaRxTaskPoll();
        vTaskDelay(pdMS_TO_TICKS(LORA_TASK_PERIOD_MS));
    }
}
