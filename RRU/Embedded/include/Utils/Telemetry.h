#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "TelemetryProtocol.h"

#define LORA_RX_TIMEOUT_MS 5000u

enum TelemetryPacketType : uint8_t {
    TELEMETRY_PACKET_FAST = 1,
    TELEMETRY_PACKET_SLOW = 2,
    TELEMETRY_PACKET_EVENT = 3,
    TELEMETRY_PACKET_SENSORS = 5
};

#pragma pack(push, 1)
struct TelemetryPacket {
    uint8_t type;
    union {
        TelemetryProtocol::FastPacket fast;
        TelemetryProtocol::SlowPacket slow;
        TelemetryProtocol::EventPacket event;
        TelemetryProtocol::SensorsPacket sensors;
    } data;
};
#pragma pack(pop)

static_assert(sizeof(TelemetryPacket) == 53, "Receiver must hold every supported body");

#endif
