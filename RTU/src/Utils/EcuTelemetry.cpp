#include "EcuTelemetry.h"

namespace
{

uint16_t readU16Le(const uint8_t* data, size_t byteIndex)
{
    return static_cast<uint16_t>(data[byteIndex]) | (static_cast<uint16_t>(data[byteIndex + 1]) << 8);
}

int16_t readI16Le(const uint8_t* data, size_t byteIndex)
{
    const uint16_t raw = readU16Le(data, byteIndex);
    // Avoid implementation-defined unsigned-to-signed narrowing for negatives.
    return static_cast<int16_t>(raw <= 0x7FFFu ? static_cast<int32_t>(raw) : static_cast<int32_t>(raw) - 65536);
}

int sourceIndex(uint32_t id)
{
    switch (id) {
    case 0x520:
        return 0;
    case 0x521:
        return 1;
    case 0x522:
        return 2;
    case 0x523:
        return 3;
    case 0x524:
        return 4;
    case 0x526:
        return 5;
    case 0x527:
        return 6;
    case 0x528:
        return 7;
    case 0x530:
        return 8;
    case 0x534:
        return 9;
    case 0x536:
        return 10;
    case 0x537:
        return 11;
    case 0x538:
        return 12;
    case 0x600:
        return 13;
    default:
        return -1;
    }
}

void decodeStatus(EcuTelemetryState& state)
{
    const uint16_t bits = state.status_bits;
    state.status.shift_cut_active = (bits & (1u << 0)) != 0;
    state.status.rev_limit_active = (bits & (1u << 1)) != 0;
    state.status.anti_lag_active = (bits & (1u << 2)) != 0;
    state.status.launch_control_active = (bits & (1u << 3)) != 0;
    state.status.traction_power_limiter_active = (bits & (1u << 4)) != 0;
    state.status.throttle_blip_active = (bits & (1u << 5)) != 0;
    state.status.knock_detected = (bits & (1u << 7)) != 0;
    state.status.brake_pedal_active = (bits & (1u << 8)) != 0;
    state.status.clutch_pedal_active = (bits & (1u << 9)) != 0;
    state.status.speed_limiter_active = (bits & (1u << 10)) != 0;
    state.status.gp_limiter_active = (bits & (1u << 11)) != 0;
    state.status.user_cut_active = (bits & (1u << 12)) != 0;
    state.status.ecu_logging = (bits & (1u << 13)) != 0;
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

    // All supported MAXXECU values are byte-aligned Intel 16-bit words.
    // Keeping their raw integers preserves every declared fixed-point scale.
    switch (id) {
    case 0x520:
        state.rpm = readU16Le(data, 0);
        state.tps_x10 = readU16Le(data, 2);
        state.map_kpa_x10 = readU16Le(data, 4);
        state.lambda_avg_x1000 = readU16Le(data, 6);
        break;
    case 0x521:
        state.lambda_a_x1000 = readU16Le(data, 0);
        state.lambda_b_x1000 = readU16Le(data, 2);
        state.ignition_timing_deg_x10 = readU16Le(data, 4);
        state.ignition_cut_percent = readU16Le(data, 6);
        break;
    case 0x522:
        state.fuel_inj_pulse_width_ms_x100 = readU16Le(data, 0);
        state.fuel_inj_duty_x10 = readU16Le(data, 2);
        state.fuel_cut_percent = readU16Le(data, 4);
        state.vehicle_speed_kph_x10 = readU16Le(data, 6);
        break;
    case 0x523:
        state.non_driven_wheel_speed_kph_x10 = readU16Le(data, 0);
        state.driven_wheel_speed_kph_x10 = readU16Le(data, 2);
        state.traction_slip_measured_x10 = readU16Le(data, 4);
        state.traction_slip_target_x10 = readU16Le(data, 6);
        break;
    case 0x524:
        state.traction_cut_request_x10 = readU16Le(data, 0);
        state.lambda_corr_a_x10 = readU16Le(data, 2);
        state.lambda_corr_b_x10 = readU16Le(data, 4);
        break;
    case 0x526:
        state.status_bits = readU16Le(data, 0);
        decodeStatus(state);
        break;
    case 0x527:
        state.lambda_target_x1000 = readI16Le(data, 6);
        break;
    case 0x528:
        state.knock_level_peak = readU16Le(data, 0);
        state.knock_correction_deg_x10 = readU16Le(data, 2);
        state.knock_count = readU16Le(data, 4);
        state.last_knock_cylinder = readU16Le(data, 6);
        break;
    case 0x530:
        state.battery_v_x100 = readU16Le(data, 0);
        state.baro_kpa_x10 = readU16Le(data, 2);
        state.intake_air_temp_c_x10 = readU16Le(data, 4);
        state.coolant_temp_c_x10 = readU16Le(data, 6);
        break;
    case 0x534:
        state.egt_delta_c = readU16Le(data, 0);
        state.ecu_temp_c = readU16Le(data, 2);
        state.ecu_error_count = readU16Le(data, 4);
        state.ecu_lost_sync_count = readU16Le(data, 6);
        break;
    case 0x536:
        state.gear = readU16Le(data, 0);
        state.boost_solenoid_duty_x10 = readU16Le(data, 2);
        state.oil_pressure_kpa_x10 = readU16Le(data, 4);
        state.oil_temp_c_x10 = readI16Le(data, 6);
        break;
    case 0x537:
        state.fuel_pressure_kpa_x10 = readU16Le(data, 0);
        state.coolant_pressure_kpa_x10 = readU16Le(data, 4);
        break;
    case 0x538:
        state.brake_pressure_kpa_x10 = readU16Le(data, 0);
        break;
    case 0x600:
        state.acceleration_x_mg = readI16Le(data, 0);
        state.acceleration_y_mg = readI16Le(data, 2);
        state.acceleration_z_mg = readI16Le(data, 4);
        break;
    }

    state.received_mask |= static_cast<uint16_t>(1u << index);
    state.last_received_ms[index] = nowMs;
    state.hasCanData = true;
    state.lastCanRxMs = nowMs;

    const uint16_t lambdaSources = TelemetryV2::SOURCE_520 | TelemetryV2::SOURCE_527;
    if ((state.received_mask & lambdaSources) == lambdaSources) {
        state.lambda_error_x1000 =
            static_cast<int32_t>(state.lambda_avg_x1000) - static_cast<int32_t>(state.lambda_target_x1000);
    } else {
        state.lambda_error_x1000 = 0;
    }

    return true;
}

uint16_t ecuFreshMask(const EcuTelemetryState& state, uint32_t nowMs)
{
    uint16_t fresh = 0;
    for (size_t index = 0; index < TelemetryV2::SOURCE_COUNT; ++index) {
        const uint16_t bit = static_cast<uint16_t>(1u << index);
        if ((state.received_mask & bit) != 0 &&
            static_cast<uint32_t>(nowMs - state.last_received_ms[index]) <= TelemetryV2::CAN_FRESHNESS_MS) {
            fresh |= bit;
        }
    }
    return fresh;
}
