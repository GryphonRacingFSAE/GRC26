#include "Fixtures.h"
#include "TelemetryCsv.h"

#include <map>
#include <string>

namespace
{
const TelemetryRxMetadata metadata = {17, 654321, -96.25f, -4.5f, 59};
// Independent expected schema: only transport metadata and the 40 DBC signals.
const char* const expectedHeader =
    "event,rx_count,rx_ms,rssi_dbm,snr_db,radio_len,packet_type,seq,tx_ms,schema_version,received_mask_he"
    "x,fresh_mask_hex,alert_flags_hex,status_bits_hex,error_code,error_text,rpm,tps_pct,lambda_avg,lambda"
    "_a,lambda_b,fuel_inj_pulse_width_ms,fuel_inj_duty_pct,vehicle_speed_kph,knock_detected,brake_pedal_a"
    "ctive,clutch_pedal_active,rev_limit_rpm,lambda_target,battery_v,intake_air_temp_c,coolant_temp_c,gea"
    "r,user_channel_1,aero_pressure_1_pa,aero_pressure_2_pa,aero_ambient_temp_c,aero_ambient_pressure_hpa"
    ",aero_node_state,aero_sensor_flags,aero_fault_flags,aero_sequence,acceleration_x_g,acceleration_y_g,"
    "acceleration_z_g,yaw_rate_dps,pitch_rate_dps,roll_rate_dps,imu_node_state,imu_sensor_flags,imu_fault"
    "_flags,imu_sequence,gps_latitude_deg,gps_longitude_deg,gps_ground_speed_kph,gps_course_deg";

std::vector<std::string> split(const std::string& value)
{
    std::vector<std::string> result;
    std::string cell;
    bool quoted = false;
    for (size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '"') {
            if (quoted && index + 1 < value.size() && value[index + 1] == '"') {
                cell += '"';
                ++index;
            } else {
                quoted = !quoted;
            }
        } else if (value[index] == ',' && !quoted) {
            result.push_back(cell);
            cell.clear();
        } else {
            cell += value[index];
        }
    }
    CHECK(!quoted);
    result.push_back(cell);
    return result;
}

std::map<std::string, std::string> rowFields(const char* line)
{
    const std::vector<std::string> names = split(telemetryCsvHeader());
    const std::vector<std::string> values = split(line);
    CHECK_EQ(names.size(), 56u);
    CHECK_EQ(values.size(), names.size());
    CHECK(std::strchr(line, '\n') == nullptr);
    CHECK(std::strchr(line, '\r') == nullptr);
    std::map<std::string, std::string> fields;
    for (size_t index = 0; index < names.size(); ++index) {
        CHECK(fields.insert(std::make_pair(names[index], values[index])).second);
    }
    return fields;
}

void expect(const std::map<std::string, std::string>& fields, const char* name, const char* expected)
{
    const auto found = fields.find(name);
    CHECK(found != fields.end());
    ++testChecks;
    if (found->second != expected) {
        std::cerr << "CSV field " << name << " expected [" << expected << "], got [" << found->second << "]\n";
        std::exit(1);
    }
}

std::map<std::string, std::string> formatted(const TelemetryReceivedPacket& packet)
{
    char line[TELEMETRY_CSV_BUFFER_SIZE];
    CHECK(formatTelemetryCsv(line, sizeof(line), packet, metadata));
    return rowFields(line);
}

struct ExpectedField {
    uint8_t type;
    const char* name;
    const char* value;
    uint16_t sources;
};

const ExpectedField measurements[] = {
    {1, "rpm", "4353", 1u << 0},
    {1, "tps_pct", "435.4", 1u << 0},
    {1, "lambda_avg", "65.535", 1u << 0},
    {1, "vehicle_speed_kph", "1306.0", 1u << 2},
    {1, "status_bits_hex", "0x0380", 1u << 3},
    {1, "knock_detected", "1", 1u << 3},
    {1, "brake_pedal_active", "1", 1u << 3},
    {1, "clutch_pedal_active", "1", 1u << 3},
    {1, "rev_limit_rpm", "9000", 1u << 3},
    {1, "gear", "65535", 1u << 6},
    {1, "user_channel_1", "4787.3", 1u << 7},
    {1, "battery_v", "304.65", 1u << 5},
    {2, "lambda_a", "8.705", 1u << 1},
    {2, "lambda_b", "8.706", 1u << 1},
    {2, "lambda_target", "-32.768", 1u << 4},
    {2, "fuel_inj_pulse_width_ms", "130.57", 1u << 2},
    {2, "fuel_inj_duty_pct", "1305.8", 1u << 2},
    {2, "intake_air_temp_c", "6323.5", 1u << 5},
    {2, "coolant_temp_c", "6323.6", 1u << 5},
    {3, "status_bits_hex", "0x0380", 1u << 3},
    {3, "rpm", "4353", 1u << 0},
    {3, "knock_detected", "1", 1u << 3},
    {3, "brake_pedal_active", "1", 1u << 3},
    {3, "clutch_pedal_active", "1", 1u << 3},
    {3, "aero_node_state", "3", 1u << 10},
    {3, "aero_sensor_flags", "165", 1u << 10},
    {3, "aero_fault_flags", "32769", 1u << 10},
    {3, "aero_sequence", "255", 1u << 10},
    {3, "imu_node_state", "2", 1u << 13},
    {3, "imu_sensor_flags", "90", 1u << 13},
    {3, "imu_fault_flags", "65535", 1u << 13},
    {3, "imu_sequence", "128", 1u << 13},
    {5, "aero_pressure_1_pa", "-32768", 1u << 8},
    {5, "aero_pressure_2_pa", "32767", 1u << 8},
    {5, "aero_ambient_temp_c", "-12.34", 1u << 9},
    {5, "aero_ambient_pressure_hpa", "6553.5", 1u << 9},
    {5, "aero_node_state", "3", 1u << 10},
    {5, "aero_sensor_flags", "165", 1u << 10},
    {5, "aero_fault_flags", "32769", 1u << 10},
    {5, "aero_sequence", "255", 1u << 10},
    {5, "acceleration_x_g", "-32.768", 1u << 11},
    {5, "acceleration_y_g", "0.000", 1u << 11},
    {5, "acceleration_z_g", "32.767", 1u << 11},
    {5, "yaw_rate_dps", "-327.68", 1u << 12},
    {5, "pitch_rate_dps", "-0.01", 1u << 12},
    {5, "roll_rate_dps", "327.67", 1u << 12},
    {5, "imu_node_state", "2", 1u << 13},
    {5, "imu_sensor_flags", "90", 1u << 13},
    {5, "imu_fault_flags", "65535", 1u << 13},
    {5, "imu_sequence", "128", 1u << 13},
    {5, "gps_latitude_deg", "-214.7483648", 1u << 14},
    {5, "gps_longitude_deg", "214.7483647", 1u << 14},
    {5, "gps_ground_speed_kph", "655.35", 1u << 15},
    {5, "gps_course_deg", "359.99", 1u << 15},
};

void setMasks(TelemetryReceivedPacket& packet, uint16_t received, uint16_t fresh)
{
    // Common offsets are part of the published wire contract.
    auto* body = reinterpret_cast<uint8_t*>(&packet.packet.data);
    body[6] = static_cast<uint8_t>(received);
    body[7] = static_cast<uint8_t>(received >> 8);
    body[8] = static_cast<uint8_t>(fresh);
    body[9] = static_cast<uint8_t>(fresh >> 8);
}

void testMeasurementsAndMasks()
{
    CHECK(std::string(telemetryCsvHeader()) == expectedHeader);
    for (uint8_t type : {1, 2, 3, 5}) {
        TelemetryReceivedPacket packet = decodeGolden(type);
        auto fields = formatted(packet);
        expect(fields, "schema_version", "3");
        expect(fields, "received_mask_hex", "0xFFFF");
        expect(fields, "fresh_mask_hex", "0xA555");
        expect(fields, "seq", "43981");
        expect(fields, "tx_ms", "305419896");
        expect(fields, "packet_type", type == 1 ? "FAST" : type == 2 ? "SLOW" : type == 3 ? "EVENT" : "SENSORS");
        expect(fields, "alert_flags_hex", type == 3 ? "0x0007" : "");
        for (const ExpectedField& measurement : measurements) {
            if (measurement.type == type) {
                expect(fields, measurement.name, measurement.value);
            }
        }
        setMasks(packet, 0xFFFF, 0);
        fields = formatted(packet);
        expect(fields, "fresh_mask_hex", "0x0000");
        for (const ExpectedField& measurement : measurements) {
            if (measurement.type == type) {
                expect(fields, measurement.name, measurement.value);
            }
        }
        // Each CAN source can independently disappear without hiding other
        // sources, waiting for a complete set, or manufacturing physical zeros.
        for (unsigned missing = 0; missing < 16; ++missing) {
            const uint16_t received = static_cast<uint16_t>(0xFFFFu & ~(1u << missing));
            setMasks(packet, received, received);
            fields = formatted(packet);
            for (const ExpectedField& measurement : measurements) {
                if (measurement.type == type) {
                    expect(fields, measurement.name,
                           (received & measurement.sources) == measurement.sources ? measurement.value : "");
                }
            }
        }
        setMasks(packet, 0, 0);
        fields = formatted(packet);
        for (const ExpectedField& measurement : measurements) {
            if (measurement.type == type) {
                expect(fields, measurement.name, "");
            }
        }
    }
    auto sensors = decodeGolden(5);
    sensors.packet.data.sensors.gps_latitude_deg_x1e7 = -1;
    sensors.packet.data.sensors.gps_longitude_deg_x1e7 = 0;
    auto fields = formatted(sensors);
    expect(fields, "gps_latitude_deg", "-0.0000001");
    expect(fields, "gps_longitude_deg", "0.0000000");
    // A later packet contains only its own fields, with no cached sensor values.
    fields = formatted(decodeGolden(1));
    expect(fields, "gps_latitude_deg", "");
    expect(fields, "acceleration_x_g", "");
}

void testStatusAndZeroValues()
{
    for (uint8_t type : {1, 3}) {
        auto packet = decodeGolden(type);
        for (uint16_t status : {0, 0x0080, 0x0100, 0x0200, 0x0380}) {
            if (type == 1) {
                packet.packet.data.fast.status_bits = status;
            } else {
                packet.packet.data.event.status_bits = status;
            }
            const auto fields = formatted(packet);
            expect(fields, "knock_detected", (status & 0x0080) ? "1" : "0");
            expect(fields, "brake_pedal_active", (status & 0x0100) ? "1" : "0");
            expect(fields, "clutch_pedal_active", (status & 0x0200) ? "1" : "0");
        }
    }
    TelemetryReceivedPacket zero = {};
    zero.version = TelemetryProtocol::VERSION;
    zero.packet.type = TELEMETRY_PACKET_FAST;
    setMasks(zero, 0xFFFF, 0xFFFF);
    const auto fields = formatted(zero);
    expect(fields, "rpm", "0");
    expect(fields, "vehicle_speed_kph", "0.0");
    expect(fields, "lambda_avg", "0.000");
    expect(fields, "battery_v", "0.00");
    expect(fields, "gear", "0");
    expect(fields, "knock_detected", "0");
}

void testCsvBoundsAndErrors()
{
    for (uint8_t type : {1, 2, 3, 5}) {
        const TelemetryReceivedPacket packet = decodeGolden(type);
        char line[TELEMETRY_CSV_BUFFER_SIZE];
        CHECK(formatTelemetryCsv(line, sizeof(line), packet, metadata));
        const size_t required = std::strlen(line) + 1;
        GuardedPayload exact(required);
        CHECK(formatTelemetryCsv(reinterpret_cast<char*>(exact.data), required, packet, metadata));
        CHECK(std::strcmp(reinterpret_cast<char*>(exact.data), line) == 0);
        for (size_t capacity = 0; capacity < required; ++capacity) {
            GuardedPayload shortOutput(capacity);
            if (capacity != 0) {
                std::memset(shortOutput.data, 0xA5, capacity);
            }
            CHECK(!formatTelemetryCsv(reinterpret_cast<char*>(shortOutput.data), capacity, packet, metadata));
            if (capacity != 0) {
                CHECK_EQ(shortOutput.data[0], 0);
            }
        }
        CHECK(!formatTelemetryCsv(nullptr, sizeof(line), packet, metadata));
    }
    char line[TELEMETRY_CSV_BUFFER_SIZE];
    for (uint8_t version : {0, 1, 2, 4, 255}) {
        TelemetryReceivedPacket invalid = decodeGolden(1);
        invalid.version = version;
        std::memset(line, 0xA5, sizeof(line));
        CHECK(!formatTelemetryCsv(line, sizeof(line), invalid, metadata));
        CHECK_EQ(line[0], 0);
    }
    for (uint8_t type : {0, 4, 255}) {
        TelemetryReceivedPacket invalid = decodeGolden(1);
        invalid.packet.type = type;
        CHECK(!formatTelemetryCsv(line, sizeof(line), invalid, metadata));
        CHECK_EQ(line[0], 0);
    }
    // A failed decode cannot be promoted to a normal measurement CSV row.
    TelemetryReceivedPacket rejected = decodeGolden(1);
    const uint8_t badFrame[] = {0, 0, 2, 1, 0, 0, 0};
    CHECK(decodeTelemetryRadioPayload(badFrame, sizeof(badFrame), rejected) != TelemetryDecodeResult::Ok);
    CHECK(!formatTelemetryCsv(line, sizeof(line), rejected, metadata));
    CHECK_EQ(line[0], 0);

    CHECK(formatTelemetryErrorCsv(line, sizeof(line), metadata, -7, "crc_mismatch"));
    auto fields = rowFields(line);
    expect(fields, "event", "rx_error");
    expect(fields, "error_code", "-7");
    expect(fields, "error_text", "crc_mismatch");
    expect(fields, "rx_count", "17");
    expect(fields, "radio_len", "59");
    expect(fields, "rpm", "");
    expect(fields, "schema_version", "");
    const size_t required = std::strlen(line) + 1;
    for (size_t capacity = 0; capacity < required; ++capacity) {
        GuardedPayload output(capacity);
        CHECK(!formatTelemetryErrorCsv(reinterpret_cast<char*>(output.data), capacity, metadata, -7, "crc_mismatch"));
        if (capacity != 0) {
            CHECK_EQ(output.data[0], 0);
        }
    }
    GuardedPayload exact(required);
    CHECK(formatTelemetryErrorCsv(reinterpret_cast<char*>(exact.data), required, metadata, -7, "crc_mismatch"));
    CHECK(std::strcmp(reinterpret_cast<char*>(exact.data), line) == 0);
    CHECK(!formatTelemetryErrorCsv(nullptr, sizeof(line), metadata, -7, "crc_mismatch"));
    CHECK(formatTelemetryErrorCsv(line, sizeof(line), metadata, -7, "bad,\"quoted\"\r\nmessage"));
    fields = rowFields(line);
    expect(fields, "error_text", "bad,\"quoted\"  message");
    CHECK(formatTelemetryErrorCsv(line, sizeof(line), metadata, -7, nullptr));
    expect(rowFields(line), "error_text", "unknown");
}

} // namespace

void runCsvTests()
{
    testMeasurementsAndMasks();
    testStatusAndZeroValues();
    testCsvBoundsAndErrors();
}
