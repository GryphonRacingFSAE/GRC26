#include "EcuTelemetry.h"

namespace
{
uint16_t readU16Le(const uint8_t* data, size_t offset)
{
    return static_cast<uint16_t>(data[offset]) | (static_cast<uint16_t>(data[offset + 1]) << 8);
}
int16_t readI16Le(const uint8_t* data, size_t offset)
{
    const uint16_t raw = readU16Le(data, offset);
    return static_cast<int16_t>(raw <= 0x7FFFu ? static_cast<int32_t>(raw) : static_cast<int32_t>(raw) - 65536);
}
int32_t readI32Le(const uint8_t* data, size_t offset)
{
    const uint32_t raw = static_cast<uint32_t>(readU16Le(data, offset)) |
                         (static_cast<uint32_t>(readU16Le(data, offset + 2)) << 16);
    // Conversion via int64_t avoids implementation-defined unsigned narrowing.
    return static_cast<int32_t>(raw <= 0x7FFFFFFFu ? static_cast<int64_t>(raw) : static_cast<int64_t>(raw) - 4294967296LL);
}
int sourceIndex(uint32_t id)
{
    const uint32_t ids[] = {0x520, 0x521, 0x522, 0x526, 0x527, 0x530, 0x536, 0x538,
                            0x600, 0x601, 0x602, 0x610, 0x611, 0x612, 0x620, 0x621};
    for (size_t i = 0; i < TelemetryProtocol::SOURCE_COUNT; ++i) {
        if (ids[i] == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}
} // namespace

bool decodeEcuCanFrame(EcuTelemetryState& state, uint32_t id, const uint8_t* data, size_t dlc, uint32_t nowMs,
                       bool extended, bool remote)
{
    if (data == nullptr || extended || remote || dlc != 8) {
        return false;
    }
    const int index = sourceIndex(id);
    if (index < 0) {
        return false;
    }
    switch (id) {
    case 0x520:
        state.rpm = readU16Le(data, 0);
        state.tps_x10 = readU16Le(data, 2);
        state.lambda_avg_x1000 = readU16Le(data, 6);
        break;
    case 0x521:
        state.lambda_a_x1000 = readU16Le(data, 0);
        state.lambda_b_x1000 = readU16Le(data, 2);
        break;
    case 0x522:
        state.fuel_inj_pulse_width_ms_x100 = readU16Le(data, 0);
        state.fuel_inj_duty_x10 = readU16Le(data, 2);
        state.vehicle_speed_kph_x10 = readU16Le(data, 6);
        break;
    case 0x526:
        state.status_bits = readU16Le(data, 0) & TelemetryProtocol::ECU_STATUS_MASK;
        state.status.knock_detected = (state.status_bits & (1u << 7)) != 0;
        state.status.brake_pedal_active = (state.status_bits & (1u << 8)) != 0;
        state.status.clutch_pedal_active = (state.status_bits & (1u << 9)) != 0;
        state.rev_limit_rpm = readU16Le(data, 4);
        break;
    case 0x527:
        state.lambda_target_x1000 = readI16Le(data, 6);
        break;
    case 0x530:
        state.battery_v_x100 = readU16Le(data, 0);
        state.intake_air_temp_c_x10 = readU16Le(data, 4);
        state.coolant_temp_c_x10 = readU16Le(data, 6);
        break;
    case 0x536:
        state.gear = readU16Le(data, 0);
        break;
    case 0x538:
        state.user_channel_1_x10 = readU16Le(data, 0);
        break;
    case 0x600:
        state.aero_pressure_1_pa = readI16Le(data, 0);
        state.aero_pressure_2_pa = readI16Le(data, 2);
        break;
    case 0x601:
        state.aero_ambient_temp_c_x100 = readI16Le(data, 0);
        state.aero_ambient_pressure_hpa_x10 = readU16Le(data, 2);
        break;
    case 0x602:
        state.aero_node_state = data[0];
        state.aero_sensor_flags = data[1];
        state.aero_fault_flags = readU16Le(data, 2);
        state.aero_sequence = data[4];
        break;
    case 0x610:
        state.acceleration_x_mg = readI16Le(data, 0);
        state.acceleration_y_mg = readI16Le(data, 2);
        state.acceleration_z_mg = readI16Le(data, 4);
        break;
    case 0x611:
        state.yaw_rate_dps_x100 = readI16Le(data, 0);
        state.pitch_rate_dps_x100 = readI16Le(data, 2);
        state.roll_rate_dps_x100 = readI16Le(data, 4);
        break;
    case 0x612:
        state.imu_node_state = data[0];
        state.imu_sensor_flags = data[1];
        state.imu_fault_flags = readU16Le(data, 2);
        state.imu_sequence = data[4];
        break;
    case 0x620:
        state.gps_latitude_deg_x1e7 = readI32Le(data, 0);
        state.gps_longitude_deg_x1e7 = readI32Le(data, 4);
        break;
    case 0x621:
        state.gps_ground_speed_kph_x100 = readU16Le(data, 0);
        state.gps_course_deg_x100 = readU16Le(data, 2);
        break;
    }
    state.received_mask |= static_cast<uint16_t>(1u << index);
    state.last_received_ms[index] = nowMs;
    state.hasCanData = true;
    state.lastCanRxMs = nowMs;
    return true;
}

uint16_t ecuFreshMask(const EcuTelemetryState& state, uint32_t nowMs)
{
    uint16_t fresh = 0;
    for (size_t index = 0; index < TelemetryProtocol::SOURCE_COUNT; ++index) {
        const uint16_t bit = static_cast<uint16_t>(1u << index);
        if ((state.received_mask & bit) != 0 &&
            static_cast<uint32_t>(nowMs - state.last_received_ms[index]) <= TelemetryProtocol::CAN_FRESHNESS_MS) {
            fresh |= bit;
        }
    }
    return fresh;
}
