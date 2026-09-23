#include "TestSupport.h"
#include "TelemetrySender.h"
#include <sstream>
#include <vector>

namespace
{
static_assert(sizeof(TelemetryPacket) == 53, "Whole queue item");

EcuTelemetryState sampleState()
{
    EcuTelemetryState s = {};
    s.hasCanData = true;
    s.received_mask = 0xFFFF;
    for (size_t i = 0; i < 16; ++i) {
        s.last_received_ms[i] = 0x12345678u - (i % 2 ? 2001u : 0u);
    }
    s.rpm = 4353;
    s.tps_x10 = 4354;
    s.lambda_avg_x1000 = 65535;
    s.vehicle_speed_kph_x10 = 4356;
    s.status_bits = 896;
    s.rev_limit_rpm = 12000;
    s.gear = 65535;
    s.user_channel_1_x10 = 4360;
    s.battery_v_x100 = 4361;
    s.lambda_a_x1000 = 8705;
    s.lambda_b_x1000 = 8706;
    s.lambda_target_x1000 = -32768;
    s.fuel_inj_pulse_width_ms_x100 = 8710;
    s.fuel_inj_duty_x10 = 65535;
    s.intake_air_temp_c_x10 = 65535;
    s.coolant_temp_c_x10 = 8713;
    s.aero_pressure_1_pa = -32768;
    s.aero_pressure_2_pa = 32767;
    s.aero_ambient_temp_c_x100 = -1234;
    s.aero_ambient_pressure_hpa_x10 = 65535;
    s.aero_node_state = 3;
    s.aero_sensor_flags = 165;
    s.aero_fault_flags = 21930;
    s.aero_sequence = 255;
    s.acceleration_x_mg = -1;
    s.acceleration_y_mg = 32767;
    s.acceleration_z_mg = -32768;
    s.yaw_rate_dps_x100 = -12345;
    s.pitch_rate_dps_x100 = 23456;
    s.roll_rate_dps_x100 = -1;
    s.imu_node_state = 2;
    s.imu_sensor_flags = 90;
    s.imu_fault_flags = 43605;
    s.imu_sequence = 254;
    s.gps_latitude_deg_x1e7 = -2147483648;
    s.gps_longitude_deg_x1e7 = 2147483647;
    s.gps_ground_speed_kph_x100 = 17425;
    s.gps_course_deg_x100 = 35999;
    return s;
}
std::vector<uint8_t> hexBytes(const char* hex)
{
    std::istringstream input(hex);
    std::vector<uint8_t> result;
    unsigned byte = 0;
    while (input >> std::hex >> byte) {
        result.push_back(static_cast<uint8_t>(byte));
    }
    return result;
}
TelemetryPacket samplePacket(uint8_t type)
{
    TelemetryPacket packet;
    std::memset(&packet, 0xA5, sizeof(packet));
    packet.type = type;
    const EcuTelemetryState state = sampleState();
    switch (type) {
    case 1: populateFastPacket(packet.data.fast, state, 0x12345678, 0xABCD); break;
    case 2: populateSlowPacket(packet.data.slow, state, 0x12345678, 0xABCD); break;
    case 3: populateEventPacket(packet.data.event, state, 0x12345678, 0xABCD, 7); break;
    case 5: populateSensorsPacket(packet.data.sensors, state, 0x12345678, 0xABCD); break;
    }
    return packet;
}

void testPacketGoldensAndBounds()
{
    // Independent little-endian schema vectors; CRC generated with Python crc_hqx.
    const uint8_t types[] = {1, 2, 3, 5};
    const char* goldens[] = {
        "54 4d 03 01 1c 78 56 34 12 cd ab ff ff 55 55 01 11 02 11 ff ff 04 11 80 03 e0 2e ff ff 08 11 09 11 7d 81",
        "54 4d 03 02 18 78 56 34 12 cd ab ff ff 55 55 01 22 02 22 00 80 06 22 ff ff ff ff 09 22 b7 22",
        "54 4d 03 03 1a 78 56 34 12 cd ab ff ff 55 55 07 00 80 03 01 11 03 a5 aa 55 ff 02 5a 55 aa fe 4f 4f",
        "54 4d 03 05 34 78 56 34 12 cd ab ff ff 55 55 00 80 ff 7f 2e fb ff ff 03 a5 aa 55 ff ff ff ff 7f 00 80 c7 cf a0 5b ff ff 02 5a 55 aa fe 00 00 00 80 ff ff ff 7f 11 44 9f 8c 13 17",
    };
    for (size_t n = 0; n < 4; ++n) {
        const TelemetryPacket packet = samplePacket(types[n]);
        const auto expected = hexBytes(goldens[n]);
        GuardedPayload exact(expected.size());
        size_t length = 0;
        CHECK(buildTelemetryRadioPayload(packet, exact.data, expected.size(), &length));
        CHECK_EQ(length, expected.size());
        CHECK(std::memcmp(exact.data, expected.data(), length) == 0);
        for (size_t capacity = 0; capacity < expected.size(); ++capacity) {
            GuardedPayload shortOutput(capacity);
            if (capacity) std::memset(shortOutput.data, 0xA5, capacity);
            length = 999;
            CHECK(!buildTelemetryRadioPayload(packet, shortOutput.data, capacity, &length));
            CHECK_EQ(length, 0u);
            for (size_t i = 0; i < capacity; ++i) CHECK_EQ(shortOutput.data[i], 0xA5);
        }
        uint8_t output[96];
        std::memset(output, 0xA5, sizeof(output));
        CHECK(!buildTelemetryRadioPayload(packet, nullptr, sizeof(output), &length));
        CHECK_EQ(length, 0u);
        CHECK(!buildTelemetryRadioPayload(packet, output, sizeof(output), nullptr));
        CHECK(buildTelemetryRadioPayload(packet, output, sizeof(output), &length));
        CHECK(std::memcmp(output, expected.data(), length) == 0);
        for (size_t i = length; i < sizeof(output); ++i) CHECK_EQ(output[i], 0xA5);
    }
    for (uint8_t type : {0, 4, 255}) {
        TelemetryPacket packet = {};
        packet.type = type;
        GuardedPayload unreadable(0);
        size_t length = 999;
        CHECK(!buildTelemetryRadioPayload(packet, unreadable.data, 96, &length));
        CHECK_EQ(length, 0u);
    }
}

void testPacketValidityAndSignedTarget()
{
    EcuTelemetryState state = {};
    TelemetryProtocol::FastPacket fast;
    populateFastPacket(fast, state, 0, 0);
    CHECK_EQ(fast.received_mask, 0);
    CHECK_EQ(fast.fresh_mask, 0);
    const uint8_t speed[] = {0, 0, 0, 0, 0, 0, 0xD2, 0x04};
    CHECK(decodeEcuCanFrame(state, 0x522, speed, 8, 100));
    populateFastPacket(fast, state, 101, 1);
    CHECK_EQ(fast.vehicle_speed_kph_x10, 1234);
    CHECK_EQ(fast.received_mask, TelemetryProtocol::SOURCE_522);
    CHECK_EQ(fast.fresh_mask, TelemetryProtocol::SOURCE_522);
    populateFastPacket(fast, state, 2101, 2);
    CHECK_EQ(fast.vehicle_speed_kph_x10, 1234);
    CHECK_EQ(fast.received_mask, TelemetryProtocol::SOURCE_522);
    CHECK_EQ(fast.fresh_mask, 0);
    TelemetryProtocol::SensorsPacket sensors;
    populateSensorsPacket(sensors, state, 2101, 3);
    CHECK_EQ(sensors.received_mask, TelemetryProtocol::SOURCE_522);
    CHECK_EQ(sensors.gps_latitude_deg_x1e7, 0); // unavailable, not a mandatory source
    state.lambda_target_x1000 = -32768;
    TelemetryProtocol::SlowPacket slow;
    populateSlowPacket(slow, state, 2101, 4);
    CHECK_EQ(slow.lambda_target_x1000, -32768);
    const uint8_t negativeTarget[] = {0x00, 0x80};
    CHECK(std::memcmp(reinterpret_cast<const uint8_t*>(&slow) + 14, negativeTarget, 2) == 0);
}

void testStatusEvents()
{
    using namespace TelemetryProtocol;
    EcuTelemetryState state = sampleState();
    TelemetryEventTracker tracker;
    state.received_mask = 0;
    CHECK_EQ(observeTelemetryEvents(state, tracker), 0);
    state.received_mask = SOURCE_526 | SOURCE_602 | SOURCE_612;
    CHECK_EQ(observeTelemetryEvents(state, tracker), STATUS_CHANGED | AERO_STATUS_CHANGED | IMU_STATUS_CHANGED);
    CHECK_EQ(observeTelemetryEvents(state, tracker), 0);
    ++state.aero_sequence;
    ++state.imu_sequence;
    ++state.rev_limit_rpm;
    CHECK_EQ(observeTelemetryEvents(state, tracker), 0); // no event storm for sequence updates
    state.status_bits = 0;
    CHECK_EQ(observeTelemetryEvents(state, tracker), STATUS_CHANGED);
    state.aero_fault_flags = 0;
    CHECK_EQ(observeTelemetryEvents(state, tracker), AERO_STATUS_CHANGED);
    state.aero_sensor_flags = 0;
    CHECK_EQ(observeTelemetryEvents(state, tracker), AERO_STATUS_CHANGED);
    state.aero_node_state = 0;
    CHECK_EQ(observeTelemetryEvents(state, tracker), AERO_STATUS_CHANGED);
    state.imu_fault_flags = 0;
    CHECK_EQ(observeTelemetryEvents(state, tracker), IMU_STATUS_CHANGED);
    state.imu_sensor_flags = 0;
    CHECK_EQ(observeTelemetryEvents(state, tracker), IMU_STATUS_CHANGED);
    state.imu_node_state = 0;
    CHECK_EQ(observeTelemetryEvents(state, tracker), IMU_STATUS_CHANGED);
    CHECK_EQ(observeTelemetryEvents(state, tracker), 0);
}
} // namespace

void runSenderTests()
{
    testPacketGoldensAndBounds();
    testPacketValidityAndSignedTarget();
    testStatusEvents();
}
