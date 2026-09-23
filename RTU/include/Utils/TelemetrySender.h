#ifndef TELEMETRY_SENDER_H
#define TELEMETRY_SENDER_H

#include "EcuTelemetry.h"
#include "Telemetry.h"

// Observations advance even when the nonblocking queue drops a packet.
struct TelemetryEventTracker {
    uint16_t status_bits = 0;
    uint8_t aero_node_state = 0;
    uint8_t aero_sensor_flags = 0;
    uint16_t aero_fault_flags = 0;
    uint8_t imu_node_state = 0;
    uint8_t imu_sensor_flags = 0;
    uint16_t imu_fault_flags = 0;
};

uint16_t observeTelemetryEvents(const EcuTelemetryState& ecu, TelemetryEventTracker& tracker);
void populateFastPacket(TelemetryProtocol::FastPacket& packet, const EcuTelemetryState& ecu, uint32_t nowMs, uint16_t seq);
void populateSensorsPacket(TelemetryProtocol::SensorsPacket& packet, const EcuTelemetryState& ecu, uint32_t nowMs, uint16_t seq);
void populateSlowPacket(TelemetryProtocol::SlowPacket& packet, const EcuTelemetryState& ecu, uint32_t nowMs, uint16_t seq);
void populateEventPacket(TelemetryProtocol::EventPacket& packet, const EcuTelemetryState& ecu, uint32_t nowMs, uint16_t seq,
                         uint16_t flags);
bool buildTelemetryRadioPayload(const TelemetryPacket& packet, uint8_t* out, size_t outSize, size_t* outLen);

#endif
