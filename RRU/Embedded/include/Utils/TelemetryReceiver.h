#ifndef TELEMETRY_RECEIVER_H
#define TELEMETRY_RECEIVER_H

#include "Telemetry.h"

struct TelemetryReceivedPacket {
    uint8_t version;
    TelemetryPacket packet;
};

enum class TelemetryDecodeResult {
    Ok,
    InvalidArgument,
    InvalidHeader,
    UnsupportedVersion,
    InvalidLength,
    CrcMismatch,
    UnsupportedType,
    InvalidSourceMasks
};

// Validate the complete envelope and exact version/type/body length before copying
// any measurement. Rejected input clears the output, never exposing a partial packet.
TelemetryDecodeResult decodeTelemetryRadioPayload(const uint8_t* data, size_t len, TelemetryReceivedPacket& out);
const char* telemetryDecodeErrorText(TelemetryDecodeResult result);

#endif
