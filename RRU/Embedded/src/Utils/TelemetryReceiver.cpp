#include "TelemetryReceiver.h"

#include <string.h>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Packed telemetry receiver decoding requires a little-endian target"
#endif

namespace
{

uint16_t readU16Le(const uint8_t* data)
{
    return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

uint16_t crc16Ccitt(const uint8_t* data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
        }
    }
    return crc;
}

size_t bodySize(uint8_t version, uint8_t type)
{
    if (version == 1) {
        switch (type) {
        case TELEMETRY_PACKET_FAST:
            return sizeof(TelemetryFastPacket);
        case TELEMETRY_PACKET_SLOW:
            return sizeof(TelemetrySlowPacket);
        case TELEMETRY_PACKET_EVENT:
            return sizeof(TelemetryEventPacket);
        default:
            return 0;
        }
    }
    switch (type) {
    case TELEMETRY_PACKET_FAST:
        return sizeof(TelemetryV2::FastPacket);
    case TELEMETRY_PACKET_SLOW:
        return sizeof(TelemetryV2::SlowPacket);
    case TELEMETRY_PACKET_EVENT:
        return sizeof(TelemetryV2::EventPacket);
    case TELEMETRY_PACKET_POWERTRAIN:
        return sizeof(TelemetryV2::PowertrainPacket);
    default:
        return 0;
    }
}

} // namespace

TelemetryDecodeResult decodeTelemetryRadioPayload(const uint8_t* data, size_t len, TelemetryReceivedPacket& out)
{
    out = {};
    if (data == nullptr) {
        return TelemetryDecodeResult::InvalidArgument;
    }
    if (len < TelemetryV2::RADIO_OVERHEAD || len > TelemetryV2::MAX_RADIO_PAYLOAD) {
        return TelemetryDecodeResult::InvalidLength;
    }
    if (data[0] != 'T' || data[1] != 'M') {
        return TelemetryDecodeResult::InvalidHeader;
    }
    const uint8_t version = data[2];
    if (version != 1 && version != TelemetryV2::VERSION) {
        return TelemetryDecodeResult::UnsupportedVersion;
    }
    const uint8_t type = data[3];
    const size_t expectedBodySize = bodySize(version, type);
    if (expectedBodySize == 0) {
        return TelemetryDecodeResult::UnsupportedType;
    }
    if (data[4] != expectedBodySize || len != expectedBodySize + TelemetryV2::RADIO_OVERHEAD) {
        return TelemetryDecodeResult::InvalidLength;
    }
    if (readU16Le(data + len - 2) != crc16Ccitt(data, len - 2)) {
        return TelemetryDecodeResult::CrcMismatch;
    }
    if (version == TelemetryV2::VERSION) {
        // These words are common to every V2 body, at body offsets 6 and 8.
        const uint16_t received = readU16Le(data + 5 + 6);
        const uint16_t fresh = readU16Le(data + 5 + 8);
        constexpr uint16_t sourceMask = (1u << TelemetryV2::SOURCE_COUNT) - 1u;
        if ((received & ~sourceMask) != 0 || (fresh & ~received) != 0) {
            return TelemetryDecodeResult::InvalidSourceMasks;
        }
    }

    out.version = version;
    out.packet.type = type;
    // Every union member starts at offset zero; bodySize selects its exact layout.
    memcpy(&out.packet.data, data + 5, expectedBodySize);
    return TelemetryDecodeResult::Ok;
}

const char* telemetryDecodeErrorText(TelemetryDecodeResult result)
{
    switch (result) {
    case TelemetryDecodeResult::Ok:
        return "ok";
    case TelemetryDecodeResult::InvalidArgument:
        return "telemetry_invalid_argument";
    case TelemetryDecodeResult::InvalidHeader:
        return "telemetry_header_invalid";
    case TelemetryDecodeResult::UnsupportedVersion:
        return "telemetry_version_unsupported";
    case TelemetryDecodeResult::InvalidLength:
        return "telemetry_payload_size_mismatch";
    case TelemetryDecodeResult::CrcMismatch:
        return "telemetry_crc_invalid";
    case TelemetryDecodeResult::UnsupportedType:
        return "unknown_packet_type";
    case TelemetryDecodeResult::InvalidSourceMasks:
        return "telemetry_source_masks_invalid";
    }
    return "telemetry_invalid";
}
