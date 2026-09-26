#ifndef TELEMETRY_CSV_H
#define TELEMETRY_CSV_H

#include <stddef.h>
#include <stdint.h>

#include "TelemetryReceiver.h"

struct TelemetryRxMetadata {
    uint32_t rx_count;
    uint32_t rx_ms;
    float rssi_dbm;
    float snr_db;
    size_t radio_len;
};

// Includes the terminator. The formatter never writes beyond the supplied capacity.
constexpr size_t TELEMETRY_CSV_BUFFER_SIZE = 1536;

const char* telemetryCsvHeader();

// Rows exclude the newline. No measurement is retained between calls. Missing
// sources produce blank cells; received but stale values retain their snapshot value.
// False means unsupported version/type or insufficient capacity; output is then empty.
bool formatTelemetryCsv(char* out, size_t capacity, const TelemetryReceivedPacket& received,
                        const TelemetryRxMetadata& metadata);
bool formatTelemetryErrorCsv(char* out, size_t capacity, const TelemetryRxMetadata& metadata, int16_t errorCode,
                             const char* errorText);

#endif
