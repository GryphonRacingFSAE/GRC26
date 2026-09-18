#ifndef ECU_TELEMETRY_H
#define ECU_TELEMETRY_H

#include <stddef.h>
#include <stdint.h>

#include "TelemetryV2.h"

// Named states from 0x526. AC, nitrous, and spare bits remain uninterpreted.
struct EcuStatusState {
    bool shift_cut_active;
    bool rev_limit_active;
    bool anti_lag_active;
    bool launch_control_active;
    bool traction_power_limiter_active;
    bool throttle_blip_active;
    bool knock_detected;
    bool brake_pedal_active;
    bool clutch_pedal_active;
    bool speed_limiter_active;
    bool gp_limiter_active;
    bool user_cut_active;
    bool ecu_logging;
};

// Fixed-point values retain the DBC's declared signedness and full raw range.
// Divide by the suffix multiplier to obtain the named engineering unit.
struct EcuTelemetryState {
    bool hasCanData;

    // 0x520
    uint16_t rpm;
    uint16_t tps_x10; // percent * 10
    uint16_t map_kpa_x10;
    uint16_t lambda_avg_x1000;

    // 0x521
    uint16_t lambda_a_x1000;
    uint16_t lambda_b_x1000;
    uint16_t ignition_timing_deg_x10;
    uint16_t ignition_cut_percent;

    // 0x522
    uint16_t fuel_inj_pulse_width_ms_x100;
    uint16_t fuel_inj_duty_x10; // percent * 10
    uint16_t fuel_cut_percent;
    uint16_t vehicle_speed_kph_x10;

    // 0x523
    uint16_t non_driven_wheel_speed_kph_x10;
    uint16_t driven_wheel_speed_kph_x10;
    uint16_t traction_slip_measured_x10; // percent * 10
    uint16_t traction_slip_target_x10;   // percent * 10

    // 0x524
    uint16_t traction_cut_request_x10; // percent * 10
    uint16_t lambda_corr_a_x10;        // percent * 10
    uint16_t lambda_corr_b_x10;        // percent * 10

    // 0x526: raw status retains every bit, including uninterpreted bits.
    uint16_t status_bits;
    EcuStatusState status;

    // 0x527
    int16_t lambda_target_x1000;

    // 0x528: peak level has no engineering unit in the supplied DBC.
    uint16_t knock_level_peak;
    uint16_t knock_correction_deg_x10;
    uint16_t knock_count;
    uint16_t last_knock_cylinder;

    // 0x530: temperatures are unsigned in the supplied DBC.
    uint16_t battery_v_x100;
    uint16_t baro_kpa_x10;
    uint16_t intake_air_temp_c_x10;
    uint16_t coolant_temp_c_x10;

    // 0x534
    uint16_t egt_delta_c;
    uint16_t ecu_temp_c;
    uint16_t ecu_error_count;
    uint16_t ecu_lost_sync_count;

    // 0x536: gear is unsigned; oil temperature is signed.
    uint16_t gear;
    uint16_t boost_solenoid_duty_x10; // percent * 10
    uint16_t oil_pressure_kpa_x10;
    int16_t oil_temp_c_x10;

    // 0x537
    uint16_t fuel_pressure_kpa_x10;
    uint16_t coolant_pressure_kpa_x10;

    // 0x538: intentionally configured brake-pressure mapping, word zero.
    uint16_t brake_pressure_kpa_x10;

    // 0x600: existing IMU mapping, signed milligravity units.
    int16_t acceleration_x_mg;
    int16_t acceleration_y_mg;
    int16_t acceleration_z_mg;

    // Derived lambda average - target; valid only with both sources received.
    int32_t lambda_error_x1000;

    uint32_t lastCanRxMs;
    // Bit/index order: 520, 521, 522, 523, 524, 526, 527, 528, 530,
    // 534, 536, 537, 538, 600 (hexadecimal CAN identifiers).
    uint16_t received_mask;
    uint32_t last_received_ms[TelemetryV2::SOURCE_COUNT];
};

// A valid supported data frame must have the full eight-byte payload.
// Rejected frames leave the complete state, including freshness, unchanged.
bool decodeEcuCanFrame(EcuTelemetryState& state, uint32_t id, const uint8_t* data, size_t dlc, uint32_t nowMs,
                       bool extended = false, bool remote = false);

// Received bits persist; fresh bits expire after CAN_FRESHNESS_MS.
uint16_t ecuFreshMask(const EcuTelemetryState& state, uint32_t nowMs);

#endif // ECU_TELEMETRY_H
