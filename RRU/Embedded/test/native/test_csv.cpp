#include "Fixtures.h"
#include "TelemetryCsv.h"

#include <algorithm>
#include <limits>
#include <map>
#include <string>

namespace
{

const TelemetryRxMetadata metadata = {17, 654321, -96.25f, -4.5f, 65};
const char* const legacyHeader = "event,rx_count,rx_ms,rssi_dbm,snr_db,radio_len,packet_type,seq,tx_ms,alert_flags_hex,"
                                 "status_bits_hex,rpm,tps_pct,map_kpa,lambda_avg,lambda_error,oil_pressure_kpa,"
                                 "fuel_pressure_kpa,coolant_temp_c,battery_v,vehicle_speed_kph,gear,oil_temp_c,"
                                 "intake_air_temp_c,fuel_inj_duty_pct,fuel_trim_total_pct,lambda_corr_a_pct,"
                                 "lambda_corr_b_pct,ignition_timing_deg,ignition_cut_pct,fuel_cut_pct,ecu_error_count,"
                                 "ecu_lost_sync_count,ecu_temp_c,egt_highest_c,egt_delta_c,knock_count,"
                                 "knock_correction_deg,boost_target_kpa,boost_duty_pct,coolant_pressure_kpa,"
                                 "wastegate_pressure_kpa,error_code,error_text";

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
    CHECK_EQ(names.size(), 75u);
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

// Hand-calculated from the independent wire vectors, not production conversion
// helpers. Nonzero values over 32767 expose accidental sign extension.
const ExpectedField measurements[] = {
    {1, "rpm", "4353", 1u << 0},
    {1, "tps_pct", "435.4", 1u << 0},
    {1, "map_kpa", "435.5", 1u << 0},
    {1, "lambda_avg", "65.535", 1u << 0},
    {1, "oil_pressure_kpa", "3917.1", 1u << 10},
    {1, "battery_v", "304.65", 1u << 8},
    {1, "vehicle_speed_kph", "1306.0", 1u << 2},
    {1, "brake_pressure_kpa", "4787.3", 1u << 12},
    {1, "status_bits_hex", "0xA55A", 1u << 5},
    {2, "oil_temp_c", "-12.3", 1u << 10},
    {2, "coolant_temp_c", "6323.6", 1u << 8},
    {2, "intake_air_temp_c", "6323.5", 1u << 8},
    {2, "ecu_temp_c", "34818", 1u << 9},
    {2, "egt_delta_c", "34817", 1u << 9},
    {2, "fuel_pressure_kpa", "4352.1", 1u << 11},
    {2, "coolant_pressure_kpa", "4352.3", 1u << 11},
    {2, "ecu_error_count", "34819", 1u << 9},
    {2, "ecu_lost_sync_count", "34820", 1u << 9},
    {2, "knock_count", "26115", 1u << 7},
    {2, "last_knock_cylinder", "26116", 1u << 7},
    {3, "status_bits_hex", "0xA55A", 1u << 5},
    {3, "rpm", "4353", 1u << 0},
    {3, "oil_pressure_kpa", "3917.1", 1u << 10},
    {3, "fuel_pressure_kpa", "4352.1", 1u << 11},
    {3, "coolant_temp_c", "6323.6", 1u << 8},
    {3, "battery_v", "304.65", 1u << 8},
    {3, "lambda_error", "98.303", (1u << 0) | (1u << 6)},
    {3, "ecu_error_count", "34819", 1u << 9},
    {3, "ecu_lost_sync_count", "34820", 1u << 9},
    {3, "knock_count", "26115", 1u << 7},
    {3, "last_knock_cylinder", "26116", 1u << 7},
    {3, "fuel_cut_pct", "12", 1u << 2},
    {3, "ignition_cut_pct", "13", 1u << 1},
    {3, "traction_cut_request_pct", "2176.1", 1u << 4},
    {3, "knock_level_peak", "26113", 1u << 7},
    {3, "knock_correction_deg", "2611.4", 1u << 7},
    {4, "lambda_a", "8.705", 1u << 1},
    {4, "lambda_b", "8.706", 1u << 1},
    {4, "lambda_target", "-32.768", 1u << 6},
    {4, "lambda_error", "98.303", (1u << 0) | (1u << 6)},
    {4, "fuel_inj_pulse_width_ms", "130.57", 1u << 2},
    {4, "fuel_inj_duty_pct", "1305.8", 1u << 2},
    {4, "fuel_cut_pct", "12", 1u << 2},
    {4, "ignition_timing_deg", "870.7", 1u << 1},
    {4, "ignition_cut_pct", "13", 1u << 1},
    {4, "driven_wheel_speed_kph", "1741.0", 1u << 3},
    {4, "non_driven_wheel_speed_kph", "1740.9", 1u << 3},
    {4, "traction_slip_measured_pct", "1741.1", 1u << 3},
    {4, "traction_slip_target_pct", "1741.2", 1u << 3},
    {4, "traction_cut_request_pct", "2176.1", 1u << 4},
    {4, "lambda_corr_a_pct", "2176.2", 1u << 4},
    {4, "lambda_corr_b_pct", "2176.3", 1u << 4},
    {4, "gear", "39169", 1u << 10},
    {4, "boost_duty_pct", "3917.0", 1u << 10},
    {4, "knock_level_peak", "26113", 1u << 7},
    {4, "knock_correction_deg", "2611.4", 1u << 7},
    {4, "acceleration_x_g", "-0.321", 1u << 13},
    {4, "acceleration_y_g", "0.000", 1u << 13},
    {4, "acceleration_z_g", "32.767", 1u << 13},
};

void setMasks(TelemetryReceivedPacket& packet, uint16_t received, uint16_t fresh)
{
    switch (packet.packet.type) {
    case 1:
        packet.packet.data.fast_v2.received_mask = received;
        packet.packet.data.fast_v2.fresh_mask = fresh;
        break;
    case 2:
        packet.packet.data.slow_v2.received_mask = received;
        packet.packet.data.slow_v2.fresh_mask = fresh;
        break;
    case 3:
        packet.packet.data.event_v2.received_mask = received;
        packet.packet.data.event_v2.fresh_mask = fresh;
        break;
    case 4:
        packet.packet.data.powertrain_v2.received_mask = received;
        packet.packet.data.powertrain_v2.fresh_mask = fresh;
        break;
    }
}

void testAllMeasurementsAndMasks()
{
    const std::string header = telemetryCsvHeader();
    CHECK(header.substr(0, std::strlen(legacyHeader)) == legacyHeader);
    CHECK_EQ(header[std::strlen(legacyHeader)], ',');
    CHECK(header.find("ac_idle") == std::string::npos);
    CHECK(header.find("nitrous") == std::string::npos);
    CHECK(header.find("severity") == std::string::npos);

    for (uint8_t type = 1; type <= 4; ++type) {
        TelemetryReceivedPacket packet = decodeGolden(2, type);
        auto fields = formatted(packet);
        expect(fields, "event", "rx_packet");
        expect(fields, "rx_count", "17");
        expect(fields, "rx_ms", "654321");
        expect(fields, "rssi_dbm", "-96.25");
        expect(fields, "snr_db", "-4.50");
        expect(fields, "radio_len", "65");
        expect(fields, "schema_version", "2");
        expect(fields, "seq", "43981");
        expect(fields, "tx_ms", "305419896");
        expect(fields, "received_mask_hex", "0x3FFF");
        expect(fields, "fresh_mask_hex", "0x1555");
        for (const ExpectedField& measurement : measurements) {
            if (measurement.type == type) {
                expect(fields, measurement.name, measurement.value);
            }
        }
        // No obsolete V1 field is fabricated when it is absent from V2.
        for (const char* absent :
             {"fuel_trim_total_pct", "egt_highest_c", "boost_target_kpa", "wastegate_pressure_kpa"}) {
            expect(fields, absent, "");
        }
        if (type == 3) {
            expect(fields, "alert_flags_hex", "0x0E0F");
        } else {
            expect(fields, "alert_flags_hex", "");
        }
        if (type == 1) {
            expect(fields, "gear", "");
            expect(fields, "lambda_error", "");
            expect(fields, "fuel_pressure_kpa", "");
        }

        // Freshness reports the snapshot's age independently of receipt. Stale
        // measurements retain their numeric values; missing sources stay blank.
        setMasks(packet, 0x3FFF, 0);
        fields = formatted(packet);
        expect(fields, "fresh_mask_hex", "0x0000");
        for (const ExpectedField& measurement : measurements) {
            if (measurement.type == type) {
                expect(fields, measurement.name, measurement.value);
            }
        }
        for (unsigned missing = 0; missing < 14; ++missing) {
            const uint16_t received = static_cast<uint16_t>(0x3FFF & ~(1u << missing));
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
}

void testStatusBitsAndZeroValues()
{
    const char* const statusNames[] = {
        "shift_cut_active",
        "rev_limit_active",
        "anti_lag_active",
        "launch_control_active",
        "tc_power_limiter_active",
        "throttle_blip_active",
        nullptr,
        "knock_detected",
        "brake_pedal_active",
        "clutch_pedal_active",
        "speed_limiter_active",
        "gp_limiter_active",
        "user_cut_active",
        "ecu_logging",
        nullptr,
        nullptr,
    };
    for (uint8_t type : {1, 3}) {
        TelemetryReceivedPacket packet = decodeGolden(2, type);
        setMasks(packet, 0x3FFF, 0x3FFF);
        for (unsigned bit = 0; bit < 16; ++bit) {
            if (type == 1) {
                packet.packet.data.fast_v2.status_bits = static_cast<uint16_t>(1u << bit);
            } else {
                packet.packet.data.event_v2.status_bits = static_cast<uint16_t>(1u << bit);
            }
            const auto fields = formatted(packet);
            char rawStatus[8];
            std::snprintf(rawStatus, sizeof(rawStatus), "0x%04X", 1u << bit);
            expect(fields, "status_bits_hex", rawStatus);
            for (unsigned named = 0; named < 16; ++named) {
                if (statusNames[named] != nullptr) {
                    expect(fields, statusNames[named], named == bit ? "1" : "0");
                }
            }
        }
        setMasks(packet, 0x3FDF, 0x3FDF);
        const auto fields = formatted(packet);
        for (const char* name : statusNames) {
            if (name != nullptr) {
                expect(fields, name, "");
            }
        }
    }
    TelemetryReceivedPacket zero = {};
    zero.version = 2;
    zero.packet.type = 1;
    setMasks(zero, 0x3FFF, 0x3FFF);
    const auto fields = formatted(zero);
    expect(fields, "rpm", "0");
    expect(fields, "vehicle_speed_kph", "0.0");
    expect(fields, "brake_pressure_kpa", "0.0");
    expect(fields, "lambda_avg", "0.000");
    expect(fields, "battery_v", "0.00");
    expect(fields, "shift_cut_active", "0");

    TelemetryReceivedPacket powertrain = decodeGolden(2, 4);
    powertrain.packet.data.powertrain_v2.lambda_error_x1000 = -32767;
    powertrain.packet.data.powertrain_v2.acceleration_x_mg = -32768;
    const auto signedFields = formatted(powertrain);
    expect(signedFields, "lambda_error", "-32.767");
    expect(signedFields, "acceleration_x_g", "-32.768");
    powertrain.packet.data.powertrain_v2.lambda_error_x1000 = std::numeric_limits<int32_t>::min();
    expect(formatted(powertrain), "lambda_error", "-2147483.648");
    powertrain.packet.data.powertrain_v2.lambda_error_x1000 = std::numeric_limits<int32_t>::max();
    expect(formatted(powertrain), "lambda_error", "2147483.647");
}

void testV1Csv()
{
    for (uint8_t type = 1; type <= 3; ++type) {
        const auto fields = formatted(decodeGolden(1, type));
        expect(fields, "schema_version", "1");
        expect(fields, "received_mask_hex", "");
        expect(fields, "fresh_mask_hex", "");
        const auto names = split(telemetryCsvHeader());
        for (size_t index = 45; index < names.size(); ++index) {
            expect(fields, names[index].c_str(), "");
        }
        if (type == 1 || type == 3) {
            expect(fields, "lambda_error", "-0.123");
            expect(fields, "coolant_temp_c", "-12.3");
            expect(fields, "status_bits_hex", "0xA55A");
        }
        if (type == 1) {
            expect(fields, "gear", "-123");
            expect(fields, "lambda_avg", "65.535");
        }
        if (type == 2) {
            expect(fields, "oil_temp_c", "-12.3");
            expect(fields, "intake_air_temp_c", "-12.2");
            expect(fields, "wastegate_pressure_kpa", "462.6");
        }
    }
}

void testCsvBoundsAndErrors()
{
    for (uint8_t version : {1, 2}) {
        for (uint8_t type = 1; type <= (version == 1 ? 3 : 4); ++type) {
            const TelemetryReceivedPacket packet = decodeGolden(version, type);
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
    }
    char line[TELEMETRY_CSV_BUFFER_SIZE];
    for (uint8_t version : {0, 3, 255}) {
        TelemetryReceivedPacket invalid = decodeGolden(2, 1);
        invalid.version = version;
        std::memset(line, 0xA5, sizeof(line));
        CHECK(!formatTelemetryCsv(line, sizeof(line), invalid, metadata));
        CHECK_EQ(line[0], 0);
    }
    for (uint8_t type : {0, 5, 255}) {
        TelemetryReceivedPacket invalid = decodeGolden(2, 1);
        invalid.packet.type = type;
        CHECK(!formatTelemetryCsv(line, sizeof(line), invalid, metadata));
        CHECK_EQ(line[0], 0);
    }
    // A failed decode cannot be promoted to a normal measurement CSV row.
    TelemetryReceivedPacket rejected = decodeGolden(2, 1);
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
    expect(fields, "radio_len", "65");
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
    testAllMeasurementsAndMasks();
    testStatusBitsAndZeroValues();
    testV1Csv();
    testCsvBoundsAndErrors();
}
