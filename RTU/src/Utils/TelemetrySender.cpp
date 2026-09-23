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
    using namespace TelemetryProtocol;
    uint16_t flags = 0;
    if (ecu.received_mask & SOURCE_526) {
        if (ecu.status_bits != tracker.status_bits) {
            flags |= STATUS_CHANGED;
        }
        tracker.status_bits = ecu.status_bits;
    }
    if (ecu.received_mask & SOURCE_602) {
        if (ecu.aero_node_state != tracker.aero_node_state || ecu.aero_sensor_flags != tracker.aero_sensor_flags ||
            ecu.aero_fault_flags != tracker.aero_fault_flags) {
            flags |= AERO_STATUS_CHANGED;
        }
        tracker.aero_node_state = ecu.aero_node_state;
        tracker.aero_sensor_flags = ecu.aero_sensor_flags;
        tracker.aero_fault_flags = ecu.aero_fault_flags;
    }
    if (ecu.received_mask & SOURCE_612) {
        if (ecu.imu_node_state != tracker.imu_node_state || ecu.imu_sensor_flags != tracker.imu_sensor_flags ||
            ecu.imu_fault_flags != tracker.imu_fault_flags) {
            flags |= IMU_STATUS_CHANGED;
        }
        tracker.imu_node_state = ecu.imu_node_state;
        tracker.imu_sensor_flags = ecu.imu_sensor_flags;
        tracker.imu_fault_flags = ecu.imu_fault_flags;
    }
    // Sequence counters are data, not fault transitions.
    return flags;
}

void populateFastPacket(TelemetryProtocol::FastPacket& p, const EcuTelemetryState& ecu, uint32_t nowMs, uint16_t seq)
{
    populateHeader(p, ecu, nowMs, seq);
    p.rpm = ecu.rpm;
    p.tps_x10 = ecu.tps_x10;
    p.lambda_avg_x1000 = ecu.lambda_avg_x1000;
    p.vehicle_speed_kph_x10 = ecu.vehicle_speed_kph_x10;
    p.status_bits = ecu.status_bits;
    p.rev_limit_rpm = ecu.rev_limit_rpm;
    p.gear = ecu.gear;
    p.user_channel_1_x10 = ecu.user_channel_1_x10;
    p.battery_v_x100 = ecu.battery_v_x100;
}

void populateSlowPacket(TelemetryProtocol::SlowPacket& p, const EcuTelemetryState& ecu, uint32_t nowMs, uint16_t seq)
{
    populateHeader(p, ecu, nowMs, seq);
    p.lambda_a_x1000 = ecu.lambda_a_x1000;
    p.lambda_b_x1000 = ecu.lambda_b_x1000;
    p.lambda_target_x1000 = ecu.lambda_target_x1000;
    p.fuel_inj_pulse_width_ms_x100 = ecu.fuel_inj_pulse_width_ms_x100;
    p.fuel_inj_duty_x10 = ecu.fuel_inj_duty_x10;
    p.intake_air_temp_c_x10 = ecu.intake_air_temp_c_x10;
    p.coolant_temp_c_x10 = ecu.coolant_temp_c_x10;
}

void populateSensorsPacket(TelemetryProtocol::SensorsPacket& p, const EcuTelemetryState& ecu, uint32_t nowMs, uint16_t seq)
{
    populateHeader(p, ecu, nowMs, seq);
    p.aero_pressure_1_pa = ecu.aero_pressure_1_pa;
    p.aero_pressure_2_pa = ecu.aero_pressure_2_pa;
    p.aero_ambient_temp_c_x100 = ecu.aero_ambient_temp_c_x100;
    p.aero_ambient_pressure_hpa_x10 = ecu.aero_ambient_pressure_hpa_x10;
    p.aero_node_state = ecu.aero_node_state;
    p.aero_sensor_flags = ecu.aero_sensor_flags;
    p.aero_fault_flags = ecu.aero_fault_flags;
    p.aero_sequence = ecu.aero_sequence;
    p.acceleration_x_mg = ecu.acceleration_x_mg;
    p.acceleration_y_mg = ecu.acceleration_y_mg;
    p.acceleration_z_mg = ecu.acceleration_z_mg;
    p.yaw_rate_dps_x100 = ecu.yaw_rate_dps_x100;
    p.pitch_rate_dps_x100 = ecu.pitch_rate_dps_x100;
    p.roll_rate_dps_x100 = ecu.roll_rate_dps_x100;
    p.imu_node_state = ecu.imu_node_state;
    p.imu_sensor_flags = ecu.imu_sensor_flags;
    p.imu_fault_flags = ecu.imu_fault_flags;
    p.imu_sequence = ecu.imu_sequence;
    p.gps_latitude_deg_x1e7 = ecu.gps_latitude_deg_x1e7;
    p.gps_longitude_deg_x1e7 = ecu.gps_longitude_deg_x1e7;
    p.gps_ground_speed_kph_x100 = ecu.gps_ground_speed_kph_x100;
    p.gps_course_deg_x100 = ecu.gps_course_deg_x100;
}

void populateEventPacket(TelemetryProtocol::EventPacket& p, const EcuTelemetryState& ecu, uint32_t nowMs, uint16_t seq, uint16_t flags)
{
    populateHeader(p, ecu, nowMs, seq);
    p.alert_flags = flags;
    p.status_bits = ecu.status_bits;
    p.rpm = ecu.rpm;
    p.aero_node_state = ecu.aero_node_state;
    p.aero_sensor_flags = ecu.aero_sensor_flags;
    p.aero_fault_flags = ecu.aero_fault_flags;
    p.aero_sequence = ecu.aero_sequence;
    p.imu_node_state = ecu.imu_node_state;
    p.imu_sensor_flags = ecu.imu_sensor_flags;
    p.imu_fault_flags = ecu.imu_fault_flags;
    p.imu_sequence = ecu.imu_sequence;
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
        payload = &packet.data.fast;
        payloadLen = sizeof(packet.data.fast);
        break;
    case TELEMETRY_PACKET_SLOW:
        payload = &packet.data.slow;
        payloadLen = sizeof(packet.data.slow);
        break;
    case TELEMETRY_PACKET_SENSORS:
        payload = &packet.data.sensors;
        payloadLen = sizeof(packet.data.sensors);
        break;
    case TELEMETRY_PACKET_EVENT:
        payload = &packet.data.event;
        payloadLen = sizeof(packet.data.event);
        break;
    default:
        return false;
    }
    const size_t totalLen = payloadLen + TelemetryProtocol::RADIO_OVERHEAD;
    if (totalLen > outSize || totalLen > TelemetryProtocol::MAX_RADIO_PAYLOAD) {
        return false;
    }
    out[0] = 'T';
    out[1] = 'M';
    out[2] = TelemetryProtocol::VERSION;
    out[3] = packet.type;
    out[4] = static_cast<uint8_t>(payloadLen);
    memcpy(out + 5, payload, payloadLen);
    const uint16_t crc = crc16Ccitt(out, payloadLen + 5);
    out[payloadLen + 5] = static_cast<uint8_t>(crc);
    out[payloadLen + 6] = static_cast<uint8_t>(crc >> 8);
    *outLen = totalLen;
    return true;
}
