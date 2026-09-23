#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include <stddef.h>
#include "TelemetryProtocol.h"

// -----------------------------
// Telemetry rate configuration
// -----------------------------
// FAST requires the matching 500 kHz / SF6 / 4/5 profile on both radios.
#define TELEMETRY_FAST_PERIOD_MS 20u   // 50 Hz
#define TELEMETRY_SLOW_PERIOD_MS 2000u // 0.5 Hz

// -----------------------------
// CAN/TWAI configuration
// -----------------------------
#define CAN_TASK_PERIOD_MS 20u // Error backoff; normal receive waits follow telemetry deadlines.

// -----------------------------
// LoRa task configuration
// -----------------------------
#define LORA_RX_TIMEOUT_MS 5000u
#define LORA_TX_POLL_DELAY_MS 2u
#define LORA_TX_GAP_MS 3u // Allow the receiver to finish reading and rearm between queued transmissions.

// Set to 1 on the car/transmitter board and 0 on the pit/receiver board.
#ifndef LORA_ROLE_TX
#define LORA_ROLE_TX 1
#endif

// Bench test: 1 sends generated ECU data over the real LoRa radio without CAN.
// Set back to 0 before using real vehicle data. May also be set with -D at build time.
#ifndef TELEMETRY_MOCK_DATA
#define TELEMETRY_MOCK_DATA 0
#endif
#if TELEMETRY_MOCK_DATA != 0 && TELEMETRY_MOCK_DATA != 1
#error "TELEMETRY_MOCK_DATA must be 0 or 1"
#endif
#if TELEMETRY_MOCK_DATA && !LORA_ROLE_TX
#error "Mock telemetry requires LORA_ROLE_TX=1"
#endif

enum TelemetryPacketType : uint8_t {
    TELEMETRY_PACKET_FAST = 1,
    TELEMETRY_PACKET_SLOW = 2,
    TELEMETRY_PACKET_EVENT = 3,
    TELEMETRY_PACKET_SENSORS = 5
};

enum EcuStatusBits : uint16_t {
    ECU_STATUS_KNOCK_DETECTED = 1u << 7,
    ECU_STATUS_BRAKE_PEDAL_ACTIVE = 1u << 8,
    ECU_STATUS_CLUTCH_PEDAL_ACTIVE = 1u << 9
};

#pragma pack(push, 1)
struct __attribute__((packed)) TelemetryPacket {
    uint8_t type;
    union {
        TelemetryProtocol::FastPacket fast;
        TelemetryProtocol::SlowPacket slow;
        TelemetryProtocol::EventPacket event;
        TelemetryProtocol::SensorsPacket sensors;
    } data;
};
#pragma pack(pop)

static_assert(sizeof(TelemetryPacket) == 53, "Queue item must hold the complete sensors packet");

#endif // TELEMETRY_H
