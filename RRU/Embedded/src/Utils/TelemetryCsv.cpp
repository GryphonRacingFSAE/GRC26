#include "TelemetryCsv.h"

#include <stdio.h>
#include <string.h>

namespace
{

// DBC signals and transport metadata share one exact header/cell definition.
#define CSV_COLUMNS(X) \
    X(event) \
    X(rx_count) \
    X(rx_ms) \
    X(rssi_dbm) \
    X(snr_db) \
    X(radio_len) \
    X(packet_type) \
    X(seq) \
    X(tx_ms) \
    X(schema_version) \
    X(received_mask_hex) \
    X(fresh_mask_hex) \
    X(alert_flags_hex) \
    X(status_bits_hex) \
    X(error_code) \
    X(error_text) \
    X(rpm) \
    X(tps_pct) \
    X(lambda_avg) \
    X(lambda_a) \
    X(lambda_b) \
    X(fuel_inj_pulse_width_ms) \
    X(fuel_inj_duty_pct) \
    X(vehicle_speed_kph) \
    X(knock_detected) \
    X(brake_pedal_active) \
    X(clutch_pedal_active) \
    X(rev_limit_rpm) \
    X(lambda_target) \
    X(battery_v) \
    X(intake_air_temp_c) \
    X(coolant_temp_c) \
    X(gear) \
    X(user_channel_1) \
    X(aero_pressure_1_pa) \
    X(aero_pressure_2_pa) \
    X(aero_ambient_temp_c) \
    X(aero_ambient_pressure_hpa) \
    X(aero_node_state) \
    X(aero_sensor_flags) \
    X(aero_fault_flags) \
    X(aero_sequence) \
    X(acceleration_x_g) \
    X(acceleration_y_g) \
    X(acceleration_z_g) \
    X(yaw_rate_dps) \
    X(pitch_rate_dps) \
    X(roll_rate_dps) \
    X(imu_node_state) \
    X(imu_sensor_flags) \
    X(imu_fault_flags) \
    X(imu_sequence) \
    X(gps_latitude_deg) \
    X(gps_longitude_deg) \
    X(gps_ground_speed_kph) \
    X(gps_course_deg)

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

static_assert(COLUMN_COUNT == 56, "CSV contains 16 metadata and 40 DBC signal columns");
// Numeric/text cells fit 24 bytes except two float metadata values (up to 44
// each) and escaped error text (at most 96). Four extra 48-byte allowances cover
// those exceptions; each base allowance includes its comma or final terminator.
static_assert(TELEMETRY_CSV_BUFFER_SIZE >= COLUMN_COUNT * 24 + 4 * 48,
              "CSV buffer must cover the largest supported row");

enum class CellFormat : uint8_t { Empty, Unsigned, Signed, Hex, Fixed1, Fixed2, Fixed3, Fixed7 };

struct Cell {
    union {
        uint32_t unsigned_value;
        int32_t signed_value;
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
            decimals == 7 ? CellFormat::Fixed7
                          : (decimals == 1 ? CellFormat::Fixed1
                                           : (decimals == 2 ? CellFormat::Fixed2 : CellFormat::Fixed3));
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
        // Bound error text to 47 characters while escaping CSV punctuation.
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
    case CellFormat::Fixed1:
    case CellFormat::Fixed2:
    case CellFormat::Fixed3:
    case CellFormat::Fixed7: {
        const int decimals = cell.format == CellFormat::Fixed7
                                 ? 7
                                 : (cell.format == CellFormat::Fixed1 ? 1 : (cell.format == CellFormat::Fixed2 ? 2 : 3));
        const uint32_t divisor = decimals == 7 ? 10000000u : (decimals == 1 ? 10u : (decimals == 2 ? 100u : 1000u));
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

template <typename Packet> void packetCommon(Row& row, const Packet& packet)
{
    row.unsignedValue(COL_seq, packet.seq);
    row.unsignedValue(COL_tx_ms, packet.ms);
    row.unsignedValue(COL_schema_version, TelemetryProtocol::VERSION);
    row.hexValue(COL_received_mask_hex, packet.received_mask);
    row.hexValue(COL_fresh_mask_hex, packet.fresh_mask);
}

void ecuStatus(Row& row, uint16_t status)
{
    row.hexValue(COL_status_bits_hex, status);
    row.unsignedValue(COL_knock_detected, (status >> 7) & 1u);
    row.unsignedValue(COL_brake_pedal_active, (status >> 8) & 1u);
    row.unsignedValue(COL_clutch_pedal_active, (status >> 9) & 1u);
}

template <typename Packet> void nodeStatus(Row& row, const Packet& p)
{
    if (p.received_mask & TelemetryProtocol::SOURCE_602) {
        row.unsignedValue(COL_aero_node_state, p.aero_node_state);
        row.unsignedValue(COL_aero_sensor_flags, p.aero_sensor_flags);
        row.unsignedValue(COL_aero_fault_flags, p.aero_fault_flags);
        row.unsignedValue(COL_aero_sequence, p.aero_sequence);
    }
    if (p.received_mask & TelemetryProtocol::SOURCE_612) {
        row.unsignedValue(COL_imu_node_state, p.imu_node_state);
        row.unsignedValue(COL_imu_sensor_flags, p.imu_sensor_flags);
        row.unsignedValue(COL_imu_fault_flags, p.imu_fault_flags);
        row.unsignedValue(COL_imu_sequence, p.imu_sequence);
    }
}

const char* packetFields(Row& row, const TelemetryPacket& packet)
{
    using namespace TelemetryProtocol;
    switch (packet.type) {
    case TELEMETRY_PACKET_FAST: {
        const auto& p = packet.data.fast;
        packetCommon(row, p);
        if (p.received_mask & SOURCE_520) {
            row.unsignedValue(COL_rpm, p.rpm);
            row.scaled(COL_tps_pct, p.tps_x10, 1);
            row.scaled(COL_lambda_avg, p.lambda_avg_x1000, 3);
        }
        if (p.received_mask & SOURCE_522) {
            row.scaled(COL_vehicle_speed_kph, p.vehicle_speed_kph_x10, 1);
        }
        if (p.received_mask & SOURCE_526) {
            ecuStatus(row, p.status_bits);
            row.unsignedValue(COL_rev_limit_rpm, p.rev_limit_rpm);
        }
        if (p.received_mask & SOURCE_536) {
            row.unsignedValue(COL_gear, p.gear);
        }
        if (p.received_mask & SOURCE_538) {
            row.scaled(COL_user_channel_1, p.user_channel_1_x10, 1);
        }
        if (p.received_mask & SOURCE_530) {
            row.scaled(COL_battery_v, p.battery_v_x100, 2);
        }
        return "FAST";
    }
    case TELEMETRY_PACKET_SLOW: {
        const auto& p = packet.data.slow;
        packetCommon(row, p);
        if (p.received_mask & SOURCE_521) {
            row.scaled(COL_lambda_a, p.lambda_a_x1000, 3);
            row.scaled(COL_lambda_b, p.lambda_b_x1000, 3);
        }
        if (p.received_mask & SOURCE_527) {
            row.scaled(COL_lambda_target, p.lambda_target_x1000, 3);
        }
        if (p.received_mask & SOURCE_522) {
            row.scaled(COL_fuel_inj_pulse_width_ms, p.fuel_inj_pulse_width_ms_x100, 2);
            row.scaled(COL_fuel_inj_duty_pct, p.fuel_inj_duty_x10, 1);
        }
        if (p.received_mask & SOURCE_530) {
            row.scaled(COL_intake_air_temp_c, p.intake_air_temp_c_x10, 1);
            row.scaled(COL_coolant_temp_c, p.coolant_temp_c_x10, 1);
        }
        return "SLOW";
    }
    case TELEMETRY_PACKET_SENSORS: {
        const auto& p = packet.data.sensors;
        packetCommon(row, p);
        if (p.received_mask & SOURCE_600) {
            row.signedValue(COL_aero_pressure_1_pa, p.aero_pressure_1_pa);
            row.signedValue(COL_aero_pressure_2_pa, p.aero_pressure_2_pa);
        }
        if (p.received_mask & SOURCE_601) {
            row.scaled(COL_aero_ambient_temp_c, p.aero_ambient_temp_c_x100, 2);
            row.scaled(COL_aero_ambient_pressure_hpa, p.aero_ambient_pressure_hpa_x10, 1);
        }
        if (p.received_mask & SOURCE_610) {
            row.scaled(COL_acceleration_x_g, p.acceleration_x_mg, 3);
            row.scaled(COL_acceleration_y_g, p.acceleration_y_mg, 3);
            row.scaled(COL_acceleration_z_g, p.acceleration_z_mg, 3);
        }
        if (p.received_mask & SOURCE_611) {
            row.scaled(COL_yaw_rate_dps, p.yaw_rate_dps_x100, 2);
            row.scaled(COL_pitch_rate_dps, p.pitch_rate_dps_x100, 2);
            row.scaled(COL_roll_rate_dps, p.roll_rate_dps_x100, 2);
        }
        if (p.received_mask & SOURCE_620) {
            row.scaled(COL_gps_latitude_deg, p.gps_latitude_deg_x1e7, 7);
            row.scaled(COL_gps_longitude_deg, p.gps_longitude_deg_x1e7, 7);
        }
        if (p.received_mask & SOURCE_621) {
            row.scaled(COL_gps_ground_speed_kph, p.gps_ground_speed_kph_x100, 2);
            row.scaled(COL_gps_course_deg, p.gps_course_deg_x100, 2);
        }
        nodeStatus(row, p);
        return "SENSORS";
    }
    case TELEMETRY_PACKET_EVENT: {
        const auto& p = packet.data.event;
        packetCommon(row, p);
        row.hexValue(COL_alert_flags_hex, p.alert_flags);
        if (p.received_mask & SOURCE_526) {
            ecuStatus(row, p.status_bits);
        }
        if (p.received_mask & SOURCE_520) {
            row.unsignedValue(COL_rpm, p.rpm);
        }
        nodeStatus(row, p);
        return "EVENT";
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
    const char* packetType = received.version == TelemetryProtocol::VERSION
                                 ? packetFields(row, received.packet)
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
