#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

// -----------------------------
// Telemetry rate configuration
// -----------------------------
// Conservative for 915 MHz LoRa range/reliability.
#define TELEMETRY_FAST_PERIOD_MS   500u     // 2 Hz
#define TELEMETRY_SLOW_PERIOD_MS   2000u    // 0.5 Hz

// -----------------------------
// CAN/TWAI configuration
// -----------------------------
#define CAN_TASK_PERIOD_MS         20u      // Used only for idle delays / status pacing

// -----------------------------
// LoRa task configuration
// -----------------------------
#define LORA_RX_TIMEOUT_MS         5000u
#define LORA_TX_POLL_DELAY_MS      2u

// Set to 1 on the car/transmitter board and 0 on the pit/receiver board.
#ifndef LORA_ROLE_TX
#define LORA_ROLE_TX               1
#endif

enum TelemetryPacketType : uint8_t
{
    TELEMETRY_PACKET_FAST  = 1,
    TELEMETRY_PACKET_SLOW  = 2,
    TELEMETRY_PACKET_EVENT = 3
};

enum EcuStatusBits : uint16_t
{
    ECU_STATUS_SHIFTCUT_ACTIVE         = 1u << 0,
    ECU_STATUS_REVLIMIT_ACTIVE         = 1u << 1,
    ECU_STATUS_ANTILAG_ACTIVE          = 1u << 2,
    ECU_STATUS_LAUNCH_CONTROL_ACTIVE   = 1u << 3,
    ECU_STATUS_TC_POWER_LIMITER_ACTIVE = 1u << 4,
    ECU_STATUS_THROTTLE_BLIP_ACTIVE    = 1u << 5,
    ECU_STATUS_AC_IDLE_UP_ACTIVE       = 1u << 6,
    ECU_STATUS_KNOCK_DETECTED          = 1u << 7,
    ECU_STATUS_BRAKE_PEDAL_ACTIVE      = 1u << 8,
    ECU_STATUS_CLUTCH_PEDAL_ACTIVE     = 1u << 9,
    ECU_STATUS_SPEED_LIMIT_ACTIVE      = 1u << 10,
    ECU_STATUS_GP_LIMITER_ACTIVE       = 1u << 11,
    ECU_STATUS_USER_CUT_ACTIVE         = 1u << 12,
    ECU_STATUS_ECU_IS_LOGGING          = 1u << 13,
    ECU_STATUS_NITROUS_ACTIVE          = 1u << 14,
    ECU_STATUS_SPARE_STATUS_BIT        = 1u << 15
};

enum TelemetryAlertFlags : uint16_t
{
    ALERT_NONE              = 0,
    ALERT_STATUS_CHANGED    = 1u << 0,
    ALERT_KNOCK_DETECTED    = 1u << 1,
    ALERT_ECU_ERROR_CHANGED = 1u << 2,
    ALERT_LOST_SYNC_CHANGED = 1u << 3,
    ALERT_OIL_PRESSURE_LOW  = 1u << 4,
    ALERT_FUEL_PRESSURE_LOW = 1u << 5,
    ALERT_COOLANT_TEMP_HIGH = 1u << 6,
    ALERT_BATTERY_LOW       = 1u << 7,
    ALERT_LAMBDA_ERROR_HIGH = 1u << 8
};

struct __attribute__((packed)) TelemetryFastPacket
{
    uint32_t ms;
    uint16_t seq;

    uint16_t rpm;
    uint16_t tps_x10;                 // percent * 10
    uint16_t map_kpa_x10;             // kPa * 10
    uint16_t lambda_avg_x1000;        // lambda * 1000
    int16_t  lambda_error_x1000;      // lambda average - lambda target, * 1000

    uint16_t oil_pressure_kpa_x10;    // kPa * 10
    uint16_t fuel_pressure_kpa_x10;   // kPa * 10
    int16_t  coolant_temp_c_x10;      // deg C * 10
    uint16_t battery_v_x100;          // V * 100
    uint16_t vehicle_speed_kph_x10;   // km/h * 10
    int16_t  gear;

    uint16_t status_bits;
};

struct __attribute__((packed)) TelemetrySlowPacket
{
    uint32_t ms;
    uint16_t seq;

    int16_t  oil_temp_c_x10;
    int16_t  intake_air_temp_c_x10;

    uint16_t fuel_inj_duty_x10;
    uint16_t fuel_trim_total_x10;
    uint16_t lambda_corr_a_x10;
    uint16_t lambda_corr_b_x10;

    uint16_t ignition_timing_deg_x10;
    uint16_t ignition_cut_percent;
    uint16_t fuel_cut_percent;

    uint16_t ecu_error_count;
    uint16_t ecu_lost_sync_count;
    uint16_t ecu_temp_c;

    uint16_t egt_highest_c;
    uint16_t egt_delta_c;

    uint16_t knock_count;
    uint16_t knock_correction_deg_x10;

    uint16_t boost_target_kpa_x10;
    uint16_t boost_duty_x10;
    uint16_t coolant_pressure_kpa_x10;
    uint16_t wastegate_pressure_kpa_x10;
};

struct __attribute__((packed)) TelemetryEventPacket
{
    uint32_t ms;
    uint16_t seq;

    uint16_t alert_flags;
    uint16_t status_bits;

    uint16_t rpm;
    uint16_t oil_pressure_kpa_x10;
    uint16_t fuel_pressure_kpa_x10;
    int16_t  coolant_temp_c_x10;
    uint16_t battery_v_x100;
    int16_t  lambda_error_x1000;

    uint16_t ecu_error_count;
    uint16_t ecu_lost_sync_count;
    uint16_t knock_count;
};

struct __attribute__((packed)) TelemetryPacket
{
    uint8_t type;

    union
    {
        TelemetryFastPacket fast;
        TelemetrySlowPacket slow;
        TelemetryEventPacket event;
    } data;
};

#endif // TELEMETRY_H
