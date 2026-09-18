#ifndef TELEMETRY_SENDER_H
#define TELEMETRY_SENDER_H

#include "EcuTelemetry.h"
#include "Telemetry.h"

// Observations advance even when the nonblocking queue drops a packet, as in V1.
struct TelemetryEventTracker {
    uint16_t received_mask = 0;
    uint16_t status_bits = 0;
    uint16_t ecu_error_count = 0;
    uint16_t ecu_lost_sync_count = 0;
    uint16_t knock_count = 0;
    bool fuel_cut_active = false;
    bool ignition_cut_active = false;
    bool traction_cut_active = false;
};

uint16_t observeTelemetryEvents(const EcuTelemetryState& ecu, TelemetryEventTracker& tracker);
void populateFastPacket(TelemetryV2::FastPacket& packet, const EcuTelemetryState& ecu, uint32_t nowMs, uint16_t seq);
void populatePowertrainPacket(TelemetryV2::PowertrainPacket& packet, const EcuTelemetryState& ecu, uint32_t nowMs,
                              uint16_t seq);
void populateSlowPacket(TelemetryV2::SlowPacket& packet, const EcuTelemetryState& ecu, uint32_t nowMs, uint16_t seq);
void populateEventPacket(TelemetryV2::EventPacket& packet, const EcuTelemetryState& ecu, uint32_t nowMs, uint16_t seq,
                         uint16_t flags);
bool buildTelemetryRadioPayload(const TelemetryPacket& packet, uint8_t* out, size_t outSize, size_t* outLen);

#endif
