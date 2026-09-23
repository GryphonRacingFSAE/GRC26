#ifndef ECU_TELEMETRY_H
#define ECU_TELEMETRY_H

#include <stddef.h>
#include <stdint.h>
#include "TelemetryProtocol.h"

// Only the three status bits explicitly named by the combined DBC.
struct EcuStatusState {
    bool knock_detected;
    bool brake_pedal_active;
    bool clutch_pedal_active;
};

// Fixed-point integers preserve the DBC's signedness and raw precision.
// Missing sources stay unavailable in received_mask; cached values need fresh_mask.
struct EcuTelemetryState {
    bool hasCanData;
    uint16_t rpm;
    uint16_t tps_x10;
    uint16_t lambda_avg_x1000;
    uint16_t lambda_a_x1000;
    uint16_t lambda_b_x1000;
    uint16_t fuel_inj_pulse_width_ms_x100;
    uint16_t fuel_inj_duty_x10;
    uint16_t vehicle_speed_kph_x10;
    uint16_t status_bits;
    EcuStatusState status;
    uint16_t rev_limit_rpm;
    int16_t lambda_target_x1000;
    uint16_t battery_v_x100;
    uint16_t intake_air_temp_c_x10;
    uint16_t coolant_temp_c_x10;
    uint16_t gear;
    uint16_t user_channel_1_x10; // MTune assignment and unit must be configured before vehicle use.
    int16_t aero_pressure_1_pa;
    int16_t aero_pressure_2_pa;
    int16_t aero_ambient_temp_c_x100;
    uint16_t aero_ambient_pressure_hpa_x10;
    uint8_t aero_node_state;
    uint8_t aero_sensor_flags;
    uint16_t aero_fault_flags;
    uint8_t aero_sequence;
    int16_t acceleration_x_mg; // longitudinal, 0x610
    int16_t acceleration_y_mg; // lateral
    int16_t acceleration_z_mg; // vertical
    int16_t yaw_rate_dps_x100;
    int16_t pitch_rate_dps_x100;
    int16_t roll_rate_dps_x100;
    uint8_t imu_node_state;
    uint8_t imu_sensor_flags;
    uint16_t imu_fault_flags;
    uint8_t imu_sequence;
    int32_t gps_latitude_deg_x1e7;
    int32_t gps_longitude_deg_x1e7;
    uint16_t gps_ground_speed_kph_x100;
    uint16_t gps_course_deg_x100;
    uint32_t lastCanRxMs;
    uint16_t received_mask;
    uint32_t last_received_ms[TelemetryProtocol::SOURCE_COUNT];
};

// Supported standard data frames must have the full eight-byte payload.
// Rejected frames leave all state and freshness unchanged.
bool decodeEcuCanFrame(EcuTelemetryState& state, uint32_t id, const uint8_t* data, size_t dlc, uint32_t nowMs,
                       bool extended = false, bool remote = false);
uint16_t ecuFreshMask(const EcuTelemetryState& state, uint32_t nowMs);

#endif
