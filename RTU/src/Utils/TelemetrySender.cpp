#include "TelemetrySender.h"

#include <string.h>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Packed telemetry sender serialization requires a little-endian target"
#endif

namespace
{

template <typename Packet>
void populateHeader(Packet& packet, const EcuTelemetryState& ecu, uint32_t nowMs, uint16_t seq)
{
    packet = {};
    packet.ms = nowMs;
    packet.seq = seq;
    packet.received_mask = ecu.received_mask;
    packet.fresh_mask = ecuFreshMask(ecu, nowMs);
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

} // namespace

uint16_t observeTelemetryEvents(const EcuTelemetryState& ecu, TelemetryEventTracker& tracker)
{
    using namespace TelemetryV2;
    uint16_t flags = 0;
    if (ecu.received_mask & SOURCE_526) {
        if (ecu.status_bits != tracker.status_bits) {
            flags |= STATUS_CHANGED;
        }
        tracker.status_bits = ecu.status_bits;
    }
    if (ecu.received_mask & SOURCE_534) {
        if (ecu.ecu_error_count != tracker.ecu_error_count) {
            flags |= ECU_ERROR_CHANGED;
        }
        if (ecu.ecu_lost_sync_count != tracker.ecu_lost_sync_count) {
            flags |= LOST_SYNC_CHANGED;
        }
        tracker.ecu_error_count = ecu.ecu_error_count;
        tracker.ecu_lost_sync_count = ecu.ecu_lost_sync_count;
    }
    if (ecu.received_mask & SOURCE_528) {
        // First observation is a baseline, not a new knock event. Decreases reset
        // the baseline; also recognize adjacent U16 rollover (indistinguishable
        // from an ECU reset at 65535 because the DBC supplies no reset marker).
        if ((tracker.received_mask & SOURCE_528) &&
            (ecu.knock_count > tracker.knock_count || (tracker.knock_count == UINT16_MAX && ecu.knock_count == 0))) {
            flags |= KNOCK_COUNT_INCREMENTED;
        }
        tracker.knock_count = ecu.knock_count;
    }
    if (ecu.received_mask & SOURCE_522) {
        const bool active = ecu.fuel_cut_percent != 0;
        if (active != tracker.fuel_cut_active) {
            flags |= FUEL_CUT_CHANGED;
        }
        tracker.fuel_cut_active = active;
    }
    if (ecu.received_mask & SOURCE_521) {
        const bool active = ecu.ignition_cut_percent != 0;
        if (active != tracker.ignition_cut_active) {
            flags |= IGNITION_CUT_CHANGED;
        }
        tracker.ignition_cut_active = active;
    }
    if (ecu.received_mask & SOURCE_524) {
        const bool active = ecu.traction_cut_request_x10 != 0;
        if (active != tracker.traction_cut_active) {
            flags |= TRACTION_CUT_CHANGED;
        }
        tracker.traction_cut_active = active;
    }
    tracker.received_mask = ecu.received_mask;
    return flags;
}

void populateFastPacket(TelemetryV2::FastPacket& p, const EcuTelemetryState& ecu, uint32_t nowMs, uint16_t seq)
{
    populateHeader(p, ecu, nowMs, seq);
    p.rpm = ecu.rpm;
    p.tps_x10 = ecu.tps_x10;
    p.map_kpa_x10 = ecu.map_kpa_x10;
    p.lambda_avg_x1000 = ecu.lambda_avg_x1000;
    p.oil_pressure_kpa_x10 = ecu.oil_pressure_kpa_x10;
    p.battery_v_x100 = ecu.battery_v_x100;
    p.vehicle_speed_kph_x10 = ecu.vehicle_speed_kph_x10;
    p.brake_pressure_kpa_x10 = ecu.brake_pressure_kpa_x10;
    p.status_bits = ecu.status_bits;
}

void populatePowertrainPacket(TelemetryV2::PowertrainPacket& p, const EcuTelemetryState& ecu, uint32_t nowMs,
                              uint16_t seq)
{
    populateHeader(p, ecu, nowMs, seq);
    p.lambda_a_x1000 = ecu.lambda_a_x1000;
    p.lambda_b_x1000 = ecu.lambda_b_x1000;
    p.lambda_target_x1000 = ecu.lambda_target_x1000;
    p.lambda_error_x1000 = ecu.lambda_error_x1000;
    p.fuel_inj_pulse_width_ms_x100 = ecu.fuel_inj_pulse_width_ms_x100;
    p.fuel_inj_duty_x10 = ecu.fuel_inj_duty_x10;
    p.fuel_cut_percent = ecu.fuel_cut_percent;
    p.ignition_timing_deg_x10 = ecu.ignition_timing_deg_x10;
    p.ignition_cut_percent = ecu.ignition_cut_percent;
    p.driven_wheel_speed_kph_x10 = ecu.driven_wheel_speed_kph_x10;
    p.non_driven_wheel_speed_kph_x10 = ecu.non_driven_wheel_speed_kph_x10;
    p.traction_slip_measured_x10 = ecu.traction_slip_measured_x10;
    p.traction_slip_target_x10 = ecu.traction_slip_target_x10;
    p.traction_cut_request_x10 = ecu.traction_cut_request_x10;
    p.lambda_corr_a_x10 = ecu.lambda_corr_a_x10;
    p.lambda_corr_b_x10 = ecu.lambda_corr_b_x10;
    p.gear = ecu.gear;
    p.boost_solenoid_duty_x10 = ecu.boost_solenoid_duty_x10;
    p.knock_level_peak = ecu.knock_level_peak;
    p.knock_correction_deg_x10 = ecu.knock_correction_deg_x10;
    p.acceleration_x_mg = ecu.acceleration_x_mg;
    p.acceleration_y_mg = ecu.acceleration_y_mg;
    p.acceleration_z_mg = ecu.acceleration_z_mg;
}

void populateSlowPacket(TelemetryV2::SlowPacket& p, const EcuTelemetryState& ecu, uint32_t nowMs, uint16_t seq)
{
    populateHeader(p, ecu, nowMs, seq);
    p.oil_temp_c_x10 = ecu.oil_temp_c_x10;
    p.coolant_temp_c_x10 = ecu.coolant_temp_c_x10;
    p.intake_air_temp_c_x10 = ecu.intake_air_temp_c_x10;
    p.ecu_temp_c = ecu.ecu_temp_c;
    p.egt_delta_c = ecu.egt_delta_c;
    p.fuel_pressure_kpa_x10 = ecu.fuel_pressure_kpa_x10;
    p.coolant_pressure_kpa_x10 = ecu.coolant_pressure_kpa_x10;
    p.ecu_error_count = ecu.ecu_error_count;
    p.ecu_lost_sync_count = ecu.ecu_lost_sync_count;
    p.knock_count = ecu.knock_count;
    p.last_knock_cylinder = ecu.last_knock_cylinder;
}

void populateEventPacket(TelemetryV2::EventPacket& p, const EcuTelemetryState& ecu, uint32_t nowMs, uint16_t seq,
                         uint16_t flags)
{
    populateHeader(p, ecu, nowMs, seq);
    p.alert_flags = flags;
    p.status_bits = ecu.status_bits;
    p.rpm = ecu.rpm;
    p.oil_pressure_kpa_x10 = ecu.oil_pressure_kpa_x10;
    p.fuel_pressure_kpa_x10 = ecu.fuel_pressure_kpa_x10;
    p.coolant_temp_c_x10 = ecu.coolant_temp_c_x10;
    p.battery_v_x100 = ecu.battery_v_x100;
    p.lambda_error_x1000 = ecu.lambda_error_x1000;
    p.ecu_error_count = ecu.ecu_error_count;
    p.ecu_lost_sync_count = ecu.ecu_lost_sync_count;
    p.knock_count = ecu.knock_count;
    p.last_knock_cylinder = ecu.last_knock_cylinder;
    p.fuel_cut_percent = ecu.fuel_cut_percent;
    p.ignition_cut_percent = ecu.ignition_cut_percent;
    p.traction_cut_request_x10 = ecu.traction_cut_request_x10;
    p.knock_level_peak = ecu.knock_level_peak;
    p.knock_correction_deg_x10 = ecu.knock_correction_deg_x10;
}

bool buildTelemetryRadioPayload(const TelemetryPacket& packet, uint8_t* out, size_t outSize, size_t* outLen)
{
    if (outLen == nullptr) {
        return false;
    }
    *outLen = 0;
    if (out == nullptr) {
        return false;
    }
    const void* payload = nullptr;
    size_t payloadLen = 0;
    switch (packet.type) {
    case TELEMETRY_PACKET_FAST:
        payload = &packet.data.fast_v2;
        payloadLen = sizeof(packet.data.fast_v2);
        break;
    case TELEMETRY_PACKET_SLOW:
        payload = &packet.data.slow_v2;
        payloadLen = sizeof(packet.data.slow_v2);
        break;
    case TELEMETRY_PACKET_EVENT:
        payload = &packet.data.event_v2;
        payloadLen = sizeof(packet.data.event_v2);
        break;
    case TELEMETRY_PACKET_POWERTRAIN:
        payload = &packet.data.powertrain_v2;
        payloadLen = sizeof(packet.data.powertrain_v2);
        break;
    default:
        return false;
    }
    const size_t totalLen = payloadLen + TelemetryV2::RADIO_OVERHEAD;
    if (totalLen > outSize || totalLen > TelemetryV2::MAX_RADIO_PAYLOAD) {
        return false;
    }
    out[0] = 'T';
    out[1] = 'M';
    out[2] = TelemetryV2::VERSION;
    out[3] = packet.type;
    out[4] = static_cast<uint8_t>(payloadLen);
    memcpy(out + 5, payload, payloadLen);
    const uint16_t crc = crc16Ccitt(out, payloadLen + 5);
    out[payloadLen + 5] = static_cast<uint8_t>(crc);
    out[payloadLen + 6] = static_cast<uint8_t>(crc >> 8);
    *outLen = totalLen;
    return true;
}
