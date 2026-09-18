#ifndef TELEMETRY_V2_H
#define TELEMETRY_V2_H

#include <stddef.h>
#include <stdint.h>

// Sender schema only. The legacy packet structs remain available to the unchanged RX path.
namespace TelemetryV2
{

constexpr uint8_t VERSION = 2;
constexpr size_t MAX_RADIO_PAYLOAD = 96;
constexpr size_t RADIO_OVERHEAD = 7;
constexpr uint32_t CAN_FRESHNESS_MS = 2000;
constexpr uint32_t POWERTRAIN_PERIOD_MS = 6000;

enum Source : uint16_t {
    SOURCE_520 = 1u << 0,
    SOURCE_521 = 1u << 1,
    SOURCE_522 = 1u << 2,
    SOURCE_523 = 1u << 3,
    SOURCE_524 = 1u << 4,
    SOURCE_526 = 1u << 5,
    SOURCE_527 = 1u << 6,
    SOURCE_528 = 1u << 7,
    SOURCE_530 = 1u << 8,
    SOURCE_534 = 1u << 9,
    SOURCE_536 = 1u << 10,
    SOURCE_537 = 1u << 11,
    SOURCE_538 = 1u << 12,
    SOURCE_600 = 1u << 13
};
constexpr size_t SOURCE_COUNT = 14;

// Existing event bits retain their positions; counters never encode severity.
enum EventFlags : uint16_t {
    STATUS_CHANGED = 1u << 0,
    KNOCK_COUNT_INCREMENTED = 1u << 1,
    ECU_ERROR_CHANGED = 1u << 2,
    LOST_SYNC_CHANGED = 1u << 3,
    FUEL_CUT_CHANGED = 1u << 9,
    IGNITION_CUT_CHANGED = 1u << 10,
    TRACTION_CUT_CHANGED = 1u << 11
};

// All integers are serialized little-endian. No native bool or bitfield is on the wire.
// received_mask: frame observed since boot; fresh_mask: observed within CAN_FRESHNESS_MS.
// A derived lambda error requires BOTH SOURCE_520 and SOURCE_527.
struct __attribute__((packed)) FastPacket {
    uint32_t ms;
    uint16_t seq;
    uint16_t received_mask;
    uint16_t fresh_mask;
    uint16_t rpm;
    uint16_t tps_x10; // percent * 10
    uint16_t map_kpa_x10;
    uint16_t lambda_avg_x1000;
    uint16_t oil_pressure_kpa_x10;
    uint16_t battery_v_x100;
    uint16_t vehicle_speed_kph_x10;
    uint16_t brake_pressure_kpa_x10; // configured 0x538 User_Channel_1
    uint16_t status_bits;            // raw 0x526; named flags use DBC bit positions
};

// Supplemental snapshot for measurements outside the high-rate core packet.
struct __attribute__((packed)) PowertrainPacket {
    uint32_t ms;
    uint16_t seq;
    uint16_t received_mask;
    uint16_t fresh_mask;
    uint16_t lambda_a_x1000;
    uint16_t lambda_b_x1000;
    int16_t lambda_target_x1000;
    int32_t lambda_error_x1000; // average - target, without saturation
    uint16_t fuel_inj_pulse_width_ms_x100;
    uint16_t fuel_inj_duty_x10; // percent * 10; may exceed 100%
    uint16_t fuel_cut_percent;
    uint16_t ignition_timing_deg_x10; // unsigned as declared by supplied DBC
    uint16_t ignition_cut_percent;
    uint16_t driven_wheel_speed_kph_x10;
    uint16_t non_driven_wheel_speed_kph_x10;
    uint16_t traction_slip_measured_x10; // percent * 10
    uint16_t traction_slip_target_x10;   // percent * 10
    uint16_t traction_cut_request_x10;   // percent * 10
    uint16_t lambda_corr_a_x10;          // percent * 10
    uint16_t lambda_corr_b_x10;          // percent * 10
    uint16_t gear;
    uint16_t boost_solenoid_duty_x10; // percent * 10
    uint16_t knock_level_peak;        // DBC declares no physical unit
    uint16_t knock_correction_deg_x10;
    int16_t acceleration_x_mg; // existing IMU 0x600 mapping
    int16_t acceleration_y_mg;
    int16_t acceleration_z_mg;
};

struct __attribute__((packed)) SlowPacket {
    uint32_t ms;
    uint16_t seq;
    uint16_t received_mask;
    uint16_t fresh_mask;
    int16_t oil_temp_c_x10;
    uint16_t coolant_temp_c_x10;    // unsigned as declared by supplied DBC
    uint16_t intake_air_temp_c_x10; // unsigned as declared by supplied DBC
    uint16_t ecu_temp_c;
    uint16_t egt_delta_c;
    uint16_t fuel_pressure_kpa_x10;
    uint16_t coolant_pressure_kpa_x10;
    uint16_t ecu_error_count;
    uint16_t ecu_lost_sync_count;
    uint16_t knock_count;
    uint16_t last_knock_cylinder;
};

struct __attribute__((packed)) EventPacket {
    uint32_t ms;
    uint16_t seq;
    uint16_t received_mask;
    uint16_t fresh_mask;
    uint16_t alert_flags;
    uint16_t status_bits;
    uint16_t rpm;
    uint16_t oil_pressure_kpa_x10;
    uint16_t fuel_pressure_kpa_x10;
    uint16_t coolant_temp_c_x10;
    uint16_t battery_v_x100;
    int32_t lambda_error_x1000;
    uint16_t ecu_error_count;
    uint16_t ecu_lost_sync_count;
    uint16_t knock_count;
    uint16_t last_knock_cylinder;
    uint16_t fuel_cut_percent;
    uint16_t ignition_cut_percent;
    uint16_t traction_cut_request_x10;
    uint16_t knock_level_peak;
    uint16_t knock_correction_deg_x10;
};

static_assert(sizeof(FastPacket) == 28, "V2 fast layout changed: revise version/schema");
static_assert(sizeof(PowertrainPacket) == 58, "V2 powertrain layout changed: revise version/schema");
static_assert(sizeof(SlowPacket) == 32, "V2 slow layout changed: revise version/schema");
static_assert(sizeof(EventPacket) == 46, "V2 event layout changed: revise version/schema");
static_assert(sizeof(FastPacket) + RADIO_OVERHEAD <= MAX_RADIO_PAYLOAD, "Fast exceeds radio budget");
static_assert(sizeof(SlowPacket) + RADIO_OVERHEAD <= MAX_RADIO_PAYLOAD, "Slow exceeds radio budget");
static_assert(sizeof(EventPacket) + RADIO_OVERHEAD <= MAX_RADIO_PAYLOAD, "Event exceeds radio budget");
static_assert(sizeof(PowertrainPacket) + RADIO_OVERHEAD <= MAX_RADIO_PAYLOAD, "Powertrain exceeds radio budget");

} // namespace TelemetryV2

#endif
