#include "Fixtures.h"

#include <limits>

namespace
{
static_assert(sizeof(TelemetryProtocol::FastPacket) == 28, "Fast wire layout");
static_assert(sizeof(TelemetryProtocol::SlowPacket) == 24, "Slow wire layout");
static_assert(sizeof(TelemetryProtocol::EventPacket) == 26, "Event wire layout");
static_assert(sizeof(TelemetryProtocol::SensorsPacket) == 52, "Sensors wire layout");

void expectRejected(const uint8_t* data, size_t size)
{
    TelemetryReceivedPacket packet;
    std::memset(&packet, 0xA5, sizeof(packet));
    CHECK(decodeTelemetryRadioPayload(data, size, packet) != TelemetryDecodeResult::Ok);
    CHECK_EQ(packet.version, 0);
    CHECK_EQ(packet.packet.type, 0);
    const uint8_t* cleared = reinterpret_cast<const uint8_t*>(&packet.packet.data);
    for (size_t index = 0; index < sizeof(packet.packet.data); ++index) {
        CHECK_EQ(cleared[index], 0);
    }
}

void rewriteCrc(std::vector<uint8_t>& frame)
{
    CHECK(frame.size() >= 7);
    uint16_t crc = 0xFFFF;
    for (size_t index = 0; index < frame.size() - 2; ++index) {
        crc ^= static_cast<uint16_t>(frame[index]) << 8;
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
        }
    }
    frame[frame.size() - 2] = static_cast<uint8_t>(crc);
    frame.back() = static_cast<uint8_t>(crc >> 8);
}

void testFramingAndBounds()
{
    for (uint8_t type : {1, 2, 3, 5}) {
        const auto wire = hexBytes(golden(type));
        GuardedPayload exact(wire.size());
        std::memcpy(exact.data, wire.data(), wire.size());
        TelemetryReceivedPacket packet;
        std::memset(&packet, 0xA5, sizeof(packet));
        CHECK(decodeTelemetryRadioPayload(exact.data, wire.size(), packet) == TelemetryDecodeResult::Ok);
        CHECK_EQ(packet.version, TelemetryProtocol::VERSION);
        CHECK_EQ(packet.packet.type, type);
        CHECK(std::memcmp(&packet.packet.data, wire.data() + 5, wire[4]) == 0);
        // Every truncated prefix ends at an inaccessible guard page.
        for (size_t size = 0; size < wire.size(); ++size) {
            GuardedPayload truncated(size);
            if (size != 0) {
                std::memcpy(truncated.data, wire.data(), size);
            }
            expectRejected(truncated.data, size);
        }
        auto extra = wire;
        extra.push_back(0);
        expectRejected(extra.data(), extra.size());
        for (size_t index = 0; index < wire.size(); ++index) {
            for (unsigned bit = 0; bit < 8; ++bit) {
                auto corrupt = wire;
                corrupt[index] ^= static_cast<uint8_t>(1u << bit);
                expectRejected(corrupt.data(), corrupt.size());
            }
        }
        for (size_t index : {0u, 1u, 2u, 3u, 4u}) {
            auto corrupt = wire;
            corrupt[index] = 0xFF;
            rewriteCrc(corrupt);
            expectRejected(corrupt.data(), corrupt.size());
        }
        // No unsupported schema may be decoded even with an otherwise valid frame.
        for (uint8_t version : {0, 1, 2, 4, 255}) {
            auto corrupt = wire;
            corrupt[2] = version;
            rewriteCrc(corrupt);
            CHECK(decodeTelemetryRadioPayload(corrupt.data(), corrupt.size(), packet) ==
                  TelemetryDecodeResult::UnsupportedVersion);
            expectRejected(corrupt.data(), corrupt.size());
        }
        auto corrupt = wire;
        corrupt[3] = 4;
        rewriteCrc(corrupt);
        expectRejected(corrupt.data(), corrupt.size());
        // Every fresh source requires receipt, including high source bits.
        for (unsigned bit = 0; bit < 16; ++bit) {
            corrupt = wire;
            corrupt[11] = corrupt[12] = 0;
            corrupt[13] = static_cast<uint8_t>(1u << bit);
            corrupt[14] = static_cast<uint8_t>((1u << bit) >> 8);
            rewriteCrc(corrupt);
            CHECK(decodeTelemetryRadioPayload(corrupt.data(), corrupt.size(), packet) ==
                  TelemetryDecodeResult::InvalidSourceMasks);
            expectRejected(corrupt.data(), corrupt.size());
        }
    }
    expectRejected(nullptr, 0);
    expectRejected(nullptr, 7);
    expectRejected(nullptr, std::numeric_limits<size_t>::max());
    GuardedPayload empty(0);
    expectRejected(empty.data, 0);
    expectRejected(empty.data, TelemetryProtocol::MAX_RADIO_PAYLOAD + 1);
    expectRejected(empty.data, std::numeric_limits<size_t>::max());
}

void testFields()
{
    const auto fast = decodeGolden(1).packet.data.fast;
    CHECK_EQ(fast.ms, 0x12345678u);
    CHECK_EQ(fast.seq, 0xABCD);
    CHECK_EQ(fast.received_mask, 0xFFFF);
    CHECK_EQ(fast.fresh_mask, 0xA555);
    CHECK_EQ(fast.rpm, 0x1101);
    CHECK_EQ(fast.tps_x10, 0x1102);
    CHECK_EQ(fast.lambda_avg_x1000, 65535);
    CHECK_EQ(fast.vehicle_speed_kph_x10, 0x3304);
    CHECK_EQ(fast.status_bits, 0x0380);
    CHECK_EQ(fast.rev_limit_rpm, 9000);
    CHECK_EQ(fast.gear, 65535);
    CHECK_EQ(fast.user_channel_1_x10, 0xBB01);
    CHECK_EQ(fast.battery_v_x100, 0x7701);
    const auto slow = decodeGolden(2).packet.data.slow;
    CHECK_EQ(slow.lambda_a_x1000, 0x2201);
    CHECK_EQ(slow.lambda_b_x1000, 0x2202);
    CHECK_EQ(slow.lambda_target_x1000, -32768);
    CHECK_EQ(slow.fuel_inj_pulse_width_ms_x100, 0x3301);
    CHECK_EQ(slow.fuel_inj_duty_x10, 0x3302);
    CHECK_EQ(slow.intake_air_temp_c_x10, 0xF703);
    CHECK_EQ(slow.coolant_temp_c_x10, 0xF704);
    const auto sensors = decodeGolden(5).packet.data.sensors;
    CHECK_EQ(sensors.aero_pressure_1_pa, -32768);
    CHECK_EQ(sensors.aero_pressure_2_pa, 32767);
    CHECK_EQ(sensors.aero_ambient_temp_c_x100, -1234);
    CHECK_EQ(sensors.aero_ambient_pressure_hpa_x10, 65535);
    CHECK_EQ(sensors.aero_node_state, 3);
    CHECK_EQ(sensors.aero_sensor_flags, 0xA5);
    CHECK_EQ(sensors.aero_fault_flags, 0x8001);
    CHECK_EQ(sensors.aero_sequence, 255);
    CHECK_EQ(sensors.acceleration_x_mg, -32768);
    CHECK_EQ(sensors.acceleration_y_mg, 0);
    CHECK_EQ(sensors.acceleration_z_mg, 32767);
    CHECK_EQ(sensors.yaw_rate_dps_x100, -32768);
    CHECK_EQ(sensors.pitch_rate_dps_x100, -1);
    CHECK_EQ(sensors.roll_rate_dps_x100, 32767);
    CHECK_EQ(sensors.imu_node_state, 2);
    CHECK_EQ(sensors.imu_sensor_flags, 0x5A);
    CHECK_EQ(sensors.imu_fault_flags, 65535);
    CHECK_EQ(sensors.imu_sequence, 128);
    CHECK_EQ(sensors.gps_latitude_deg_x1e7, std::numeric_limits<int32_t>::min());
    CHECK_EQ(sensors.gps_longitude_deg_x1e7, std::numeric_limits<int32_t>::max());
    CHECK_EQ(sensors.gps_ground_speed_kph_x100, 65535);
    CHECK_EQ(sensors.gps_course_deg_x100, 35999);
    const auto event = decodeGolden(3).packet.data.event;
    CHECK_EQ(event.alert_flags, 7);
    CHECK_EQ(event.status_bits, 0x0380);
    CHECK_EQ(event.rpm, 0x1101);
    CHECK_EQ(event.aero_node_state, 3);
    CHECK_EQ(event.aero_sensor_flags, 0xA5);
    CHECK_EQ(event.aero_fault_flags, 0x8001);
    CHECK_EQ(event.aero_sequence, 255);
    CHECK_EQ(event.imu_node_state, 2);
    CHECK_EQ(event.imu_sensor_flags, 0x5A);
    CHECK_EQ(event.imu_fault_flags, 65535);
    CHECK_EQ(event.imu_sequence, 128);
}

} // namespace

void runDecoderTests()
{
    testFramingAndBounds();
    testFields();
}
