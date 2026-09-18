#include "TelemetryCsv.h"

#include <stdio.h>
#include <string.h>

namespace
{

// Keep the original 44 columns in their original order. One definition controls
// both the header and the cell indices, including appended V2 columns.
#define CSV_COLUMNS(X)                                                                                                 \
    X(event)                                                                                                           \
    X(rx_count)                                                                                                        \
    X(rx_ms)                                                                                                           \
    X(rssi_dbm)                                                                                                        \
    X(snr_db)                                                                                                          \
    X(radio_len)                                                                                                       \
    X(packet_type)                                                                                                     \
    X(seq)                                                                                                             \
    X(tx_ms)                                                                                                           \
    X(alert_flags_hex)                                                                                                 \
    X(status_bits_hex)                                                                                                 \
    X(rpm)                                                                                                             \
    X(tps_pct)                                                                                                         \
    X(map_kpa)                                                                                                         \
    X(lambda_avg)                                                                                                      \
    X(lambda_error)                                                                                                    \
    X(oil_pressure_kpa)                                                                                                \
    X(fuel_pressure_kpa)                                                                                               \
    X(coolant_temp_c)                                                                                                  \
    X(battery_v)                                                                                                       \
    X(vehicle_speed_kph)                                                                                               \
    X(gear)                                                                                                            \
    X(oil_temp_c)                                                                                                      \
    X(intake_air_temp_c)                                                                                               \
    X(fuel_inj_duty_pct)                                                                                               \
    X(fuel_trim_total_pct)                                                                                             \
    X(lambda_corr_a_pct)                                                                                               \
    X(lambda_corr_b_pct)                                                                                               \
    X(ignition_timing_deg)                                                                                             \
    X(ignition_cut_pct)                                                                                                \
    X(fuel_cut_pct)                                                                                                    \
    X(ecu_error_count)                                                                                                 \
    X(ecu_lost_sync_count)                                                                                             \
    X(ecu_temp_c)                                                                                                      \
    X(egt_highest_c)                                                                                                   \
    X(egt_delta_c)                                                                                                     \
    X(knock_count)                                                                                                     \
    X(knock_correction_deg)                                                                                            \
    X(boost_target_kpa)                                                                                                \
    X(boost_duty_pct)                                                                                                  \
    X(coolant_pressure_kpa)                                                                                            \
    X(wastegate_pressure_kpa)                                                                                          \
    X(error_code)                                                                                                      \
    X(error_text)                                                                                                      \
    X(schema_version)                                                                                                  \
    X(received_mask_hex)                                                                                               \
    X(fresh_mask_hex)                                                                                                  \
    X(brake_pressure_kpa)                                                                                              \
    X(lambda_a)                                                                                                        \
    X(lambda_b)                                                                                                        \
    X(lambda_target)                                                                                                   \
    X(fuel_inj_pulse_width_ms)                                                                                         \
    X(driven_wheel_speed_kph)                                                                                          \
    X(non_driven_wheel_speed_kph)                                                                                      \
    X(traction_slip_measured_pct)                                                                                      \
    X(traction_slip_target_pct)                                                                                        \
    X(traction_cut_request_pct)                                                                                        \
    X(knock_level_peak)                                                                                                \
    X(last_knock_cylinder)                                                                                             \
    X(acceleration_x_g)                                                                                                \
    X(acceleration_y_g)                                                                                                \
    X(acceleration_z_g)                                                                                                \
    X(shift_cut_active)                                                                                                \
    X(rev_limit_active)                                                                                                \
    X(anti_lag_active)                                                                                                 \
    X(launch_control_active)                                                                                           \
    X(tc_power_limiter_active)                                                                                         \
    X(throttle_blip_active)                                                                                            \
    X(knock_detected)                                                                                                  \
    X(brake_pedal_active)                                                                                              \
    X(clutch_pedal_active)                                                                                             \
    X(speed_limiter_active)                                                                                            \
    X(gp_limiter_active)                                                                                               \
    X(user_cut_active)                                                                                                 \
    X(ecu_logging)

enum Column {
#define COLUMN_ENUM(name) COL_##name,
    CSV_COLUMNS(COLUMN_ENUM)
#undef COLUMN_ENUM
        COLUMN_COUNT
};

// The leading comma lets the public accessor skip it without a trailing empty cell.
constexpr char CSV_HEADER[] =
#define COLUMN_HEADER(name) "," #name
    CSV_COLUMNS(COLUMN_HEADER);
#undef COLUMN_HEADER
#undef CSV_COLUMNS

static_assert(COL_schema_version == 44, "The legacy CSV prefix must remain unchanged");
static_assert(COLUMN_COUNT == 75, "Update the documented CSV schema when adding columns");
// Numeric/text cells fit 24 bytes except two float metadata values (up to 44
// each) and escaped error text (at most 96). Four extra 48-byte allowances cover
// those exceptions; each base allowance includes its comma or final terminator.
static_assert(TELEMETRY_CSV_BUFFER_SIZE >= COLUMN_COUNT * 24 + 4 * 48,
              "CSV buffer must cover the largest supported row");

enum class CellFormat : uint8_t { Empty, Unsigned, Signed, Hex, Fixed1, Fixed2, Fixed3, Legacy1, Legacy2, Legacy3 };

struct Cell {
    union {
        uint32_t unsigned_value;
        int32_t signed_value;
        float float_value;
    } value;
    CellFormat format;
};

struct Row {
    Cell cells[COLUMN_COUNT] = {};

    void unsignedValue(Column column, uint32_t value)
    {
        cells[column].value.unsigned_value = value;
        cells[column].format = CellFormat::Unsigned;
    }

    void signedValue(Column column, int32_t value)
    {
        cells[column].value.signed_value = value;
        cells[column].format = CellFormat::Signed;
    }

    void hexValue(Column column, uint16_t value)
    {
        cells[column].value.unsigned_value = value;
        cells[column].format = CellFormat::Hex;
    }

    void scaled(Column column, int32_t value, uint8_t decimals)
    {
        cells[column].value.signed_value = value;
        cells[column].format =
            decimals == 1 ? CellFormat::Fixed1 : (decimals == 2 ? CellFormat::Fixed2 : CellFormat::Fixed3);
    }

    void legacyScaled(Column column, int32_t value, uint8_t decimals)
    {
        // Preserve the V1 float conversion and formatting behavior.
        const float divisor = decimals == 1 ? 10.0f : (decimals == 2 ? 100.0f : 1000.0f);
        cells[column].value.float_value = value / divisor;
        cells[column].format =
            decimals == 1 ? CellFormat::Legacy1 : (decimals == 2 ? CellFormat::Legacy2 : CellFormat::Legacy3);
    }
};

class Writer
{
  public:
    Writer(char* output, size_t capacity) : output_(output), capacity_(capacity)
    {
        if (output_ != nullptr && capacity_ != 0) {
            output_[0] = '\0';
        } else {
            ok_ = false;
        }
    }

    void append(const char* text)
    {
        if (!ok_) {
            return;
        }
        const size_t length = strlen(text);
        if (length >= capacity_ - used_) {
            ok_ = false;
            return;
        }
        memcpy(output_ + used_, text, length + 1);
        used_ += length;
    }

    void errorText(const char* text)
    {
        // Preserve the legacy 47-character limit while escaping CSV punctuation.
        const char* value = text != nullptr ? text : "unknown";
        size_t length = 0;
        bool quote = false;
        while (length < 47 && value[length] != '\0') {
            const char ch = value[length++];
            quote = quote || ch == ',' || ch == '"' || ch == '\r' || ch == '\n';
        }
        if (quote) {
            append("\"");
        }
        for (size_t i = 0; i < length; ++i) {
            const char ch = value[i];
            if (ch == '"') {
                append("\"");
            }
            // Keep each telemetry record on one serial line.
            const char character[2] = {ch == '\r' || ch == '\n' ? ' ' : ch, '\0'};
            append(character);
        }
        if (quote) {
            append("\"");
        }
    }

    bool finish()
    {
        if (!ok_ && output_ != nullptr && capacity_ != 0) {
            output_[0] = '\0';
        }
        return ok_;
    }

  private:
    char* output_;
    size_t capacity_;
    size_t used_ = 0;
    bool ok_ = true;
};

void formatCell(char* destination, size_t capacity, const Cell& cell)
{
    destination[0] = '\0';
    switch (cell.format) {
    case CellFormat::Empty:
        return;
    case CellFormat::Unsigned:
        snprintf(destination, capacity, "%lu", static_cast<unsigned long>(cell.value.unsigned_value));
        return;
    case CellFormat::Signed:
        snprintf(destination, capacity, "%ld", static_cast<long>(cell.value.signed_value));
        return;
    case CellFormat::Hex:
        snprintf(destination, capacity, "0x%04X", static_cast<unsigned int>(cell.value.unsigned_value));
        return;
    case CellFormat::Legacy1:
    case CellFormat::Legacy2:
    case CellFormat::Legacy3: {
        const int decimals = cell.format == CellFormat::Legacy1 ? 1 : (cell.format == CellFormat::Legacy2 ? 2 : 3);
        snprintf(destination, capacity, "%.*f", decimals, static_cast<double>(cell.value.float_value));
        return;
    }
    case CellFormat::Fixed1:
    case CellFormat::Fixed2:
    case CellFormat::Fixed3: {
        const int decimals = cell.format == CellFormat::Fixed1 ? 1 : (cell.format == CellFormat::Fixed2 ? 2 : 3);
        const uint32_t divisor = decimals == 1 ? 10u : (decimals == 2 ? 100u : 1000u);
        const int32_t value = cell.value.signed_value;
        // Unsigned subtraction also handles INT32_MIN without signed overflow.
        const uint32_t magnitude = value < 0 ? 0u - static_cast<uint32_t>(value) : static_cast<uint32_t>(value);
        snprintf(destination, capacity, "%s%lu.%0*lu", value < 0 ? "-" : "",
                 static_cast<unsigned long>(magnitude / divisor), decimals,
                 static_cast<unsigned long>(magnitude % divisor));
        return;
    }
    }
}

bool writeRow(char* out, size_t capacity, const TelemetryRxMetadata& metadata, const Row& row, const char* packetType,
              const char* errorText, bool error)
{
    Writer writer(out, capacity);
    char formatted[64];
    for (size_t i = 0; i < COLUMN_COUNT; ++i) {
        if (i != 0) {
            writer.append(",");
        }
        switch (i) {
        case COL_event:
            writer.append(error ? "rx_error" : "rx_packet");
            continue;
        case COL_rx_count:
            snprintf(formatted, sizeof(formatted), "%lu", static_cast<unsigned long>(metadata.rx_count));
            break;
        case COL_rx_ms:
            snprintf(formatted, sizeof(formatted), "%lu", static_cast<unsigned long>(metadata.rx_ms));
            break;
        case COL_rssi_dbm:
            snprintf(formatted, sizeof(formatted), "%.2f", static_cast<double>(metadata.rssi_dbm));
            break;
        case COL_snr_db:
            snprintf(formatted, sizeof(formatted), "%.2f", static_cast<double>(metadata.snr_db));
            break;
        case COL_radio_len:
            snprintf(formatted, sizeof(formatted), "%lu", static_cast<unsigned long>(metadata.radio_len));
            break;
        case COL_packet_type:
            writer.append(packetType);
            continue;
        case COL_error_text:
            if (error) {
                writer.errorText(errorText);
            }
            continue;
        default:
            formatCell(formatted, sizeof(formatted), row.cells[i]);
            break;
        }
        writer.append(formatted);
    }
    return writer.finish();
}

template <typename Packet> void v1Common(Row& row, const Packet& packet)
{
    row.unsignedValue(COL_seq, packet.seq);
    row.unsignedValue(COL_tx_ms, packet.ms);
    row.unsignedValue(COL_schema_version, 1);
}

template <typename Packet> void v2Common(Row& row, const Packet& packet)
{
    row.unsignedValue(COL_seq, packet.seq);
    row.unsignedValue(COL_tx_ms, packet.ms);
    row.unsignedValue(COL_schema_version, 2);
    row.hexValue(COL_received_mask_hex, packet.received_mask);
    row.hexValue(COL_fresh_mask_hex, packet.fresh_mask);
}

void v2Status(Row& row, uint16_t status)
{
    row.hexValue(COL_status_bits_hex, status);
    const Column columns[] = {COL_shift_cut_active,
                              COL_rev_limit_active,
                              COL_anti_lag_active,
                              COL_launch_control_active,
                              COL_tc_power_limiter_active,
                              COL_throttle_blip_active,
                              COL_knock_detected,
                              COL_brake_pedal_active,
                              COL_clutch_pedal_active,
                              COL_speed_limiter_active,
                              COL_gp_limiter_active,
                              COL_user_cut_active,
                              COL_ecu_logging};
    const uint8_t bits[] = {0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12, 13};
    for (size_t i = 0; i < sizeof(bits); ++i) {
        row.unsignedValue(columns[i], (status >> bits[i]) & 1u);
    }
}

const char* v1Fields(Row& row, const TelemetryPacket& packet)
{
    switch (packet.type) {
    case TELEMETRY_PACKET_FAST: {
        const auto& p = packet.data.fast;
        v1Common(row, p);
        row.hexValue(COL_status_bits_hex, p.status_bits);
        row.unsignedValue(COL_rpm, p.rpm);
        row.legacyScaled(COL_tps_pct, p.tps_x10, 1);
        row.legacyScaled(COL_map_kpa, p.map_kpa_x10, 1);
        row.legacyScaled(COL_lambda_avg, p.lambda_avg_x1000, 3);
        row.legacyScaled(COL_lambda_error, p.lambda_error_x1000, 3);
        row.legacyScaled(COL_oil_pressure_kpa, p.oil_pressure_kpa_x10, 1);
        row.legacyScaled(COL_fuel_pressure_kpa, p.fuel_pressure_kpa_x10, 1);
        row.legacyScaled(COL_coolant_temp_c, p.coolant_temp_c_x10, 1);
        row.legacyScaled(COL_battery_v, p.battery_v_x100, 2);
        row.legacyScaled(COL_vehicle_speed_kph, p.vehicle_speed_kph_x10, 1);
        row.signedValue(COL_gear, p.gear);
        return "FAST";
    }
    case TELEMETRY_PACKET_SLOW: {
        const auto& p = packet.data.slow;
        v1Common(row, p);
        row.legacyScaled(COL_oil_temp_c, p.oil_temp_c_x10, 1);
        row.legacyScaled(COL_intake_air_temp_c, p.intake_air_temp_c_x10, 1);
        row.legacyScaled(COL_fuel_inj_duty_pct, p.fuel_inj_duty_x10, 1);
        row.legacyScaled(COL_fuel_trim_total_pct, p.fuel_trim_total_x10, 1);
        row.legacyScaled(COL_lambda_corr_a_pct, p.lambda_corr_a_x10, 1);
        row.legacyScaled(COL_lambda_corr_b_pct, p.lambda_corr_b_x10, 1);
        row.legacyScaled(COL_ignition_timing_deg, p.ignition_timing_deg_x10, 1);
        row.unsignedValue(COL_ignition_cut_pct, p.ignition_cut_percent);
        row.unsignedValue(COL_fuel_cut_pct, p.fuel_cut_percent);
        row.unsignedValue(COL_ecu_error_count, p.ecu_error_count);
        row.unsignedValue(COL_ecu_lost_sync_count, p.ecu_lost_sync_count);
        row.unsignedValue(COL_ecu_temp_c, p.ecu_temp_c);
        row.unsignedValue(COL_egt_highest_c, p.egt_highest_c);
        row.unsignedValue(COL_egt_delta_c, p.egt_delta_c);
        row.unsignedValue(COL_knock_count, p.knock_count);
        row.legacyScaled(COL_knock_correction_deg, p.knock_correction_deg_x10, 1);
        row.legacyScaled(COL_boost_target_kpa, p.boost_target_kpa_x10, 1);
        row.legacyScaled(COL_boost_duty_pct, p.boost_duty_x10, 1);
        row.legacyScaled(COL_coolant_pressure_kpa, p.coolant_pressure_kpa_x10, 1);
        row.legacyScaled(COL_wastegate_pressure_kpa, p.wastegate_pressure_kpa_x10, 1);
        return "SLOW";
    }
    case TELEMETRY_PACKET_EVENT: {
        const auto& p = packet.data.event;
        v1Common(row, p);
        row.hexValue(COL_alert_flags_hex, p.alert_flags);
        row.hexValue(COL_status_bits_hex, p.status_bits);
        row.unsignedValue(COL_rpm, p.rpm);
        row.legacyScaled(COL_lambda_error, p.lambda_error_x1000, 3);
        row.legacyScaled(COL_oil_pressure_kpa, p.oil_pressure_kpa_x10, 1);
        row.legacyScaled(COL_fuel_pressure_kpa, p.fuel_pressure_kpa_x10, 1);
        row.legacyScaled(COL_coolant_temp_c, p.coolant_temp_c_x10, 1);
        row.legacyScaled(COL_battery_v, p.battery_v_x100, 2);
        row.unsignedValue(COL_ecu_error_count, p.ecu_error_count);
        row.unsignedValue(COL_ecu_lost_sync_count, p.ecu_lost_sync_count);
        row.unsignedValue(COL_knock_count, p.knock_count);
        return "EVENT";
    }
    default:
        return nullptr;
    }
}

const char* v2Fields(Row& row, const TelemetryPacket& packet)
{
    using namespace TelemetryV2;
    switch (packet.type) {
    case TELEMETRY_PACKET_FAST: {
        const auto& p = packet.data.fast_v2;
        v2Common(row, p);
        if (p.received_mask & SOURCE_520) {
            row.unsignedValue(COL_rpm, p.rpm);
            row.scaled(COL_tps_pct, p.tps_x10, 1);
            row.scaled(COL_map_kpa, p.map_kpa_x10, 1);
            row.scaled(COL_lambda_avg, p.lambda_avg_x1000, 3);
        }
        if (p.received_mask & SOURCE_536) {
            row.scaled(COL_oil_pressure_kpa, p.oil_pressure_kpa_x10, 1);
        }
        if (p.received_mask & SOURCE_530) {
            row.scaled(COL_battery_v, p.battery_v_x100, 2);
        }
        if (p.received_mask & SOURCE_522) {
            row.scaled(COL_vehicle_speed_kph, p.vehicle_speed_kph_x10, 1);
        }
        if (p.received_mask & SOURCE_538) {
            row.scaled(COL_brake_pressure_kpa, p.brake_pressure_kpa_x10, 1);
        }
        if (p.received_mask & SOURCE_526) {
            v2Status(row, p.status_bits);
        }
        return "FAST";
    }
    case TELEMETRY_PACKET_SLOW: {
        const auto& p = packet.data.slow_v2;
        v2Common(row, p);
        if (p.received_mask & SOURCE_536) {
            row.scaled(COL_oil_temp_c, p.oil_temp_c_x10, 1);
        }
        if (p.received_mask & SOURCE_530) {
            row.scaled(COL_coolant_temp_c, p.coolant_temp_c_x10, 1);
            row.scaled(COL_intake_air_temp_c, p.intake_air_temp_c_x10, 1);
        }
        if (p.received_mask & SOURCE_534) {
            row.unsignedValue(COL_ecu_temp_c, p.ecu_temp_c);
            row.unsignedValue(COL_egt_delta_c, p.egt_delta_c);
            row.unsignedValue(COL_ecu_error_count, p.ecu_error_count);
            row.unsignedValue(COL_ecu_lost_sync_count, p.ecu_lost_sync_count);
        }
        if (p.received_mask & SOURCE_537) {
            row.scaled(COL_fuel_pressure_kpa, p.fuel_pressure_kpa_x10, 1);
            row.scaled(COL_coolant_pressure_kpa, p.coolant_pressure_kpa_x10, 1);
        }
        if (p.received_mask & SOURCE_528) {
            row.unsignedValue(COL_knock_count, p.knock_count);
            row.unsignedValue(COL_last_knock_cylinder, p.last_knock_cylinder);
        }
        return "SLOW";
    }
    case TELEMETRY_PACKET_EVENT: {
        const auto& p = packet.data.event_v2;
        v2Common(row, p);
        row.hexValue(COL_alert_flags_hex, p.alert_flags);
        if (p.received_mask & SOURCE_526) {
            v2Status(row, p.status_bits);
        }
        if (p.received_mask & SOURCE_520) {
            row.unsignedValue(COL_rpm, p.rpm);
        }
        if (p.received_mask & SOURCE_536) {
            row.scaled(COL_oil_pressure_kpa, p.oil_pressure_kpa_x10, 1);
        }
        if (p.received_mask & SOURCE_537) {
            row.scaled(COL_fuel_pressure_kpa, p.fuel_pressure_kpa_x10, 1);
        }
        if (p.received_mask & SOURCE_530) {
            row.scaled(COL_coolant_temp_c, p.coolant_temp_c_x10, 1);
            row.scaled(COL_battery_v, p.battery_v_x100, 2);
        }
        if ((p.received_mask & (SOURCE_520 | SOURCE_527)) == (SOURCE_520 | SOURCE_527)) {
            row.scaled(COL_lambda_error, p.lambda_error_x1000, 3);
        }
        if (p.received_mask & SOURCE_534) {
            row.unsignedValue(COL_ecu_error_count, p.ecu_error_count);
            row.unsignedValue(COL_ecu_lost_sync_count, p.ecu_lost_sync_count);
        }
        if (p.received_mask & SOURCE_528) {
            row.unsignedValue(COL_knock_count, p.knock_count);
            row.unsignedValue(COL_last_knock_cylinder, p.last_knock_cylinder);
            row.unsignedValue(COL_knock_level_peak, p.knock_level_peak);
            row.scaled(COL_knock_correction_deg, p.knock_correction_deg_x10, 1);
        }
        if (p.received_mask & SOURCE_522) {
            row.unsignedValue(COL_fuel_cut_pct, p.fuel_cut_percent);
        }
        if (p.received_mask & SOURCE_521) {
            row.unsignedValue(COL_ignition_cut_pct, p.ignition_cut_percent);
        }
        if (p.received_mask & SOURCE_524) {
            row.scaled(COL_traction_cut_request_pct, p.traction_cut_request_x10, 1);
        }
        return "EVENT";
    }
    case TELEMETRY_PACKET_POWERTRAIN: {
        const auto& p = packet.data.powertrain_v2;
        v2Common(row, p);
        if (p.received_mask & SOURCE_521) {
            row.scaled(COL_lambda_a, p.lambda_a_x1000, 3);
            row.scaled(COL_lambda_b, p.lambda_b_x1000, 3);
            row.scaled(COL_ignition_timing_deg, p.ignition_timing_deg_x10, 1);
            row.unsignedValue(COL_ignition_cut_pct, p.ignition_cut_percent);
        }
        if (p.received_mask & SOURCE_527) {
            row.scaled(COL_lambda_target, p.lambda_target_x1000, 3);
        }
        if ((p.received_mask & (SOURCE_520 | SOURCE_527)) == (SOURCE_520 | SOURCE_527)) {
            row.scaled(COL_lambda_error, p.lambda_error_x1000, 3);
        }
        if (p.received_mask & SOURCE_522) {
            row.scaled(COL_fuel_inj_pulse_width_ms, p.fuel_inj_pulse_width_ms_x100, 2);
            row.scaled(COL_fuel_inj_duty_pct, p.fuel_inj_duty_x10, 1);
            row.unsignedValue(COL_fuel_cut_pct, p.fuel_cut_percent);
        }
        if (p.received_mask & SOURCE_523) {
            row.scaled(COL_driven_wheel_speed_kph, p.driven_wheel_speed_kph_x10, 1);
            row.scaled(COL_non_driven_wheel_speed_kph, p.non_driven_wheel_speed_kph_x10, 1);
            row.scaled(COL_traction_slip_measured_pct, p.traction_slip_measured_x10, 1);
            row.scaled(COL_traction_slip_target_pct, p.traction_slip_target_x10, 1);
        }
        if (p.received_mask & SOURCE_524) {
            row.scaled(COL_traction_cut_request_pct, p.traction_cut_request_x10, 1);
            row.scaled(COL_lambda_corr_a_pct, p.lambda_corr_a_x10, 1);
            row.scaled(COL_lambda_corr_b_pct, p.lambda_corr_b_x10, 1);
        }
        if (p.received_mask & SOURCE_536) {
            row.unsignedValue(COL_gear, p.gear);
            row.scaled(COL_boost_duty_pct, p.boost_solenoid_duty_x10, 1);
        }
        if (p.received_mask & SOURCE_528) {
            row.unsignedValue(COL_knock_level_peak, p.knock_level_peak);
            row.scaled(COL_knock_correction_deg, p.knock_correction_deg_x10, 1);
        }
        if (p.received_mask & SOURCE_600) {
            row.scaled(COL_acceleration_x_g, p.acceleration_x_mg, 3);
            row.scaled(COL_acceleration_y_g, p.acceleration_y_mg, 3);
            row.scaled(COL_acceleration_z_g, p.acceleration_z_mg, 3);
        }
        return "POWERTRAIN";
    }
    default:
        return nullptr;
    }
}

} // namespace

const char* telemetryCsvHeader()
{
    return CSV_HEADER + 1;
}

bool formatTelemetryCsv(char* out, size_t capacity, const TelemetryReceivedPacket& received,
                        const TelemetryRxMetadata& metadata)
{
    if (out == nullptr || capacity == 0) {
        return false;
    }
    out[0] = '\0';
    Row row;
    const char* packetType = received.version == 1   ? v1Fields(row, received.packet)
                             : received.version == 2 ? v2Fields(row, received.packet)
                                                     : nullptr;
    return packetType != nullptr && writeRow(out, capacity, metadata, row, packetType, nullptr, false);
}

bool formatTelemetryErrorCsv(char* out, size_t capacity, const TelemetryRxMetadata& metadata, int16_t errorCode,
                             const char* errorText)
{
    Row row;
    row.signedValue(COL_error_code, errorCode);
    return writeRow(out, capacity, metadata, row, "UNKNOWN", errorText, true);
}
