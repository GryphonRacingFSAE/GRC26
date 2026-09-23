#ifndef TELEMETRY_PROTOCOL_H
#define TELEMETRY_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

// Wire definitions for the GRC26 combined CAN database in RTU/GRC26.dbc.
namespace TelemetryProtocol {
constexpr uint8_t VERSION = 3;
constexpr size_t MAX_RADIO_PAYLOAD = 96;
constexpr size_t RADIO_OVERHEAD = 7;
constexpr uint32_t CAN_FRESHNESS_MS = 2000;
constexpr uint32_t SENSORS_PERIOD_MS = 200;

enum Source : uint16_t {
    SOURCE_520 = 1u << 0,
    SOURCE_521 = 1u << 1,
    SOURCE_522 = 1u << 2,
    SOURCE_526 = 1u << 3,
    SOURCE_527 = 1u << 4,
    SOURCE_530 = 1u << 5,
    SOURCE_536 = 1u << 6,
    SOURCE_538 = 1u << 7,
    SOURCE_600 = 1u << 8,
    SOURCE_601 = 1u << 9,
    SOURCE_602 = 1u << 10,
    SOURCE_610 = 1u << 11,
    SOURCE_611 = 1u << 12,
    SOURCE_612 = 1u << 13,
    SOURCE_620 = 1u << 14,
    SOURCE_621 = 1u << 15
};
constexpr size_t SOURCE_COUNT = 16;
constexpr uint16_t ALL_SOURCES = 0xFFFFu;
constexpr uint16_t ECU_STATUS_MASK = 0x0380u; // Only the three DBC-defined bits.

enum EventFlags : uint16_t {
    STATUS_CHANGED = 1u << 0,
    AERO_STATUS_CHANGED = 1u << 1,
    IMU_STATUS_CHANGED = 1u << 2
};

// Packed sizes define the wire contract; multibyte wire values are little endian.
// Each packet is independent: no receiver must wait for another packet or source.
// received_mask persists since boot; fresh_mask is a subset at this snapshot's ms.
#pragma pack(push, 1)
struct __attribute__((packed)) FastPacket {
    uint32_t ms;
    uint16_t seq;
    uint16_t received_mask;
    uint16_t fresh_mask;
    uint16_t rpm;
    uint16_t tps_x10;
    uint16_t lambda_avg_x1000;
    uint16_t vehicle_speed_kph_x10;
    uint16_t status_bits;
    uint16_t rev_limit_rpm;
    uint16_t gear;
    uint16_t user_channel_1_x10; // Actual MTune assignment is unspecified; no invented unit.
    uint16_t battery_v_x100;
};

struct __attribute__((packed)) SlowPacket {
    uint32_t ms;
    uint16_t seq;
    uint16_t received_mask;
    uint16_t fresh_mask;
    uint16_t lambda_a_x1000;
    uint16_t lambda_b_x1000;
    int16_t lambda_target_x1000;
    uint16_t fuel_inj_pulse_width_ms_x100;
    uint16_t fuel_inj_duty_x10;
    uint16_t intake_air_temp_c_x10;
    uint16_t coolant_temp_c_x10;
};

struct __attribute__((packed)) SensorsPacket {
    uint32_t ms;
    uint16_t seq;
    uint16_t received_mask;
    uint16_t fresh_mask;
    int16_t aero_pressure_1_pa;
    int16_t aero_pressure_2_pa;
    int16_t aero_ambient_temp_c_x100;
    uint16_t aero_ambient_pressure_hpa_x10;
    uint8_t aero_node_state;
    uint8_t aero_sensor_flags;
    uint16_t aero_fault_flags;
    uint8_t aero_sequence;
    int16_t acceleration_x_mg; // Longitudinal
    int16_t acceleration_y_mg; // Lateral
    int16_t acceleration_z_mg; // Vertical
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
};

struct __attribute__((packed)) EventPacket {
    uint32_t ms;
    uint16_t seq;
    uint16_t received_mask;
    uint16_t fresh_mask;
    uint16_t alert_flags;
    uint16_t status_bits;
    uint16_t rpm;
    uint8_t aero_node_state;
    uint8_t aero_sensor_flags;
    uint16_t aero_fault_flags;
    uint8_t aero_sequence;
    uint8_t imu_node_state;
    uint8_t imu_sensor_flags;
    uint16_t imu_fault_flags;
    uint8_t imu_sequence;
};

#pragma pack(pop)

static_assert(sizeof(FastPacket) == 28, "Fast layout changed: revise schema");
static_assert(sizeof(SlowPacket) == 24, "Slow layout changed: revise schema");
static_assert(sizeof(SensorsPacket) == 52, "Sensors layout changed: revise schema");
static_assert(sizeof(EventPacket) == 26, "Event layout changed: revise schema");
static_assert(sizeof(FastPacket) + RADIO_OVERHEAD <= MAX_RADIO_PAYLOAD, "Fast exceeds radio budget");
static_assert(sizeof(SlowPacket) + RADIO_OVERHEAD <= MAX_RADIO_PAYLOAD, "Slow exceeds radio budget");
static_assert(sizeof(SensorsPacket) + RADIO_OVERHEAD <= MAX_RADIO_PAYLOAD, "Sensors exceed radio budget");
static_assert(sizeof(EventPacket) + RADIO_OVERHEAD <= MAX_RADIO_PAYLOAD, "Event exceeds radio budget");
} // namespace TelemetryProtocol

#endif
