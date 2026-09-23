#include "TestSupport.h"
#include "EcuTelemetry.h"
#include <type_traits>

namespace
{
const uint32_t frameIds[] = {0x520, 0x521, 0x522, 0x526, 0x527, 0x530, 0x536, 0x538,
                             0x600, 0x601, 0x602, 0x610, 0x611, 0x612, 0x620, 0x621};
const uint8_t rawWords[] = {0x34, 0x12, 0xCD, 0xAB, 0x78, 0x56, 0xDC, 0xFE};
static_assert(std::is_same<decltype(EcuTelemetryState::coolant_temp_c_x10), uint16_t>::value, "Unsigned DBC temperature");
static_assert(std::is_same<decltype(EcuTelemetryState::gear), uint16_t>::value, "Unsigned DBC gear");
static_assert(std::is_same<decltype(EcuTelemetryState::gps_latitude_deg_x1e7), int32_t>::value, "Signed GPS");
static_assert(std::is_same<decltype(EcuTelemetryState::lambda_target_x1000), int16_t>::value, "Signed target");

void testRepresentativeFrames()
{
    for (size_t index = 0; index < TelemetryProtocol::SOURCE_COUNT; ++index) {
        EcuTelemetryState state = {};
        GuardedPayload payload(8);
        std::memcpy(payload.data, rawWords, sizeof(rawWords));
        CHECK(decodeEcuCanFrame(state, frameIds[index], payload.data, 8, 12345));
        CHECK(state.hasCanData);
        CHECK_EQ(state.lastCanRxMs, 12345u);
        CHECK_EQ(state.received_mask, 1u << index);
        CHECK_EQ(ecuFreshMask(state, 12345), 1u << index);
        for (size_t source = 0; source < TelemetryProtocol::SOURCE_COUNT; ++source) {
            CHECK_EQ(state.last_received_ms[source], source == index ? 12345u : 0u);
        }
        switch (frameIds[index]) {
        case 0x520:
            CHECK_EQ(state.rpm, 0x1234);
            CHECK_EQ(state.tps_x10, 0xABCD);
            CHECK_EQ(state.lambda_avg_x1000, 0xFEDC);
            break;
        case 0x521:
            CHECK_EQ(state.lambda_a_x1000, 0x1234);
            CHECK_EQ(state.lambda_b_x1000, 0xABCD);
            break;
        case 0x522:
            CHECK_EQ(state.fuel_inj_pulse_width_ms_x100, 0x1234);
            CHECK_EQ(state.fuel_inj_duty_x10, 0xABCD);
            CHECK_EQ(state.vehicle_speed_kph_x10, 0xFEDC);
            break;
        case 0x526:
            CHECK_EQ(state.status_bits, 0x0200);
            CHECK_EQ(state.rev_limit_rpm, 0x5678);
            break;
        case 0x527:
            CHECK_EQ(state.lambda_target_x1000, -292);
            break;
        case 0x530:
            CHECK_EQ(state.battery_v_x100, 0x1234);
            CHECK_EQ(state.intake_air_temp_c_x10, 0x5678);
            CHECK_EQ(state.coolant_temp_c_x10, 0xFEDC);
            break;
        case 0x536:
            CHECK_EQ(state.gear, 0x1234);
            break;
        case 0x538:
            CHECK_EQ(state.user_channel_1_x10, 0x1234);
            break;
        case 0x600:
            CHECK_EQ(state.aero_pressure_1_pa, 0x1234);
            CHECK_EQ(state.aero_pressure_2_pa, -21555);
            CHECK_EQ(state.acceleration_x_mg, 0); // Old IMU mapping must not leak.
            break;
        case 0x601:
            CHECK_EQ(state.aero_ambient_temp_c_x100, 0x1234);
            CHECK_EQ(state.aero_ambient_pressure_hpa_x10, 0xABCD);
            break;
        case 0x602:
            CHECK_EQ(state.aero_node_state, 0x34);
            CHECK_EQ(state.aero_sensor_flags, 0x12);
            CHECK_EQ(state.aero_fault_flags, 0xABCD);
            CHECK_EQ(state.aero_sequence, 0x78);
            break;
        case 0x610:
            CHECK_EQ(state.acceleration_x_mg, 0x1234);
            CHECK_EQ(state.acceleration_y_mg, -21555);
            CHECK_EQ(state.acceleration_z_mg, 0x5678);
            CHECK_EQ(state.aero_pressure_1_pa, 0);
            break;
        case 0x611:
            CHECK_EQ(state.yaw_rate_dps_x100, 0x1234);
            CHECK_EQ(state.pitch_rate_dps_x100, -21555);
            CHECK_EQ(state.roll_rate_dps_x100, 0x5678);
            break;
        case 0x612:
            CHECK_EQ(state.imu_node_state, 0x34);
            CHECK_EQ(state.imu_sensor_flags, 0x12);
            CHECK_EQ(state.imu_fault_flags, 0xABCD);
            CHECK_EQ(state.imu_sequence, 0x78);
            break;
        case 0x620:
            CHECK_EQ(state.gps_latitude_deg_x1e7, -1412623820);
            CHECK_EQ(state.gps_longitude_deg_x1e7, -19114376);
            break;
        case 0x621:
            CHECK_EQ(state.gps_ground_speed_kph_x100, 0x1234);
            CHECK_EQ(state.gps_course_deg_x100, 0xABCD);
            break;
        }
    }
}

void testEngineeringScalesAndSpeedRegression()
{
    EcuTelemetryState state = {};
    const uint8_t injectors[] = {0x41, 0x01, 0x4C, 0x04, 0x19, 0x00, 0xD2, 0x04};
    CHECK(decodeEcuCanFrame(state, 0x522, injectors, 8, 0));
    CHECK_EQ(state.fuel_inj_pulse_width_ms_x100, 321); // 3.21 ms
    CHECK_EQ(state.fuel_inj_duty_x10, 1100); // 110.0%
    CHECK_EQ(state.vehicle_speed_kph_x10, 1234); // 123.4 km/h
    const uint8_t extrema[] = {0, 0x80, 0xFF, 0x7F, 0xFF, 0xFF, 0, 0};
    CHECK(decodeEcuCanFrame(state, 0x600, extrema, 8, 0));
    CHECK_EQ(state.aero_pressure_1_pa, -32768);
    CHECK_EQ(state.aero_pressure_2_pa, 32767);
    CHECK(decodeEcuCanFrame(state, 0x601, extrema, 8, 0));
    CHECK_EQ(state.aero_ambient_temp_c_x100, -32768); // -327.68 C
    CHECK_EQ(state.aero_ambient_pressure_hpa_x10, 32767); // 3276.7 hPa
    CHECK(decodeEcuCanFrame(state, 0x610, extrema, 8, 0));
    CHECK_EQ(state.acceleration_x_mg, -32768);
    CHECK_EQ(state.acceleration_y_mg, 32767);
    CHECK_EQ(state.acceleration_z_mg, -1);
    CHECK(decodeEcuCanFrame(state, 0x611, extrema, 8, 0));
    CHECK_EQ(state.yaw_rate_dps_x100, -32768);
    CHECK_EQ(state.pitch_rate_dps_x100, 32767);
    CHECK_EQ(state.roll_rate_dps_x100, -1);
    const uint8_t gpsExtrema[] = {0, 0, 0, 0x80, 0xFF, 0xFF, 0xFF, 0x7F};
    CHECK(decodeEcuCanFrame(state, 0x620, gpsExtrema, 8, 0));
    CHECK_EQ(state.gps_latitude_deg_x1e7, INT32_MIN);
    CHECK_EQ(state.gps_longitude_deg_x1e7, INT32_MAX);
}

void testStatusBits()
{
    EcuTelemetryState state = {};
    for (unsigned bit = 0; bit < 16; ++bit) {
        const uint16_t raw = static_cast<uint16_t>(1u << bit);
        const uint8_t frame[] = {static_cast<uint8_t>(raw), static_cast<uint8_t>(raw >> 8), 0, 0, 0xE0, 0x2E, 0, 0};
        CHECK(decodeEcuCanFrame(state, 0x526, frame, 8, bit));
        CHECK_EQ(state.status_bits, raw & 0x0380);
        CHECK_EQ(state.status.knock_detected, bit == 7);
        CHECK_EQ(state.status.brake_pedal_active, bit == 8);
        CHECK_EQ(state.status.clutch_pedal_active, bit == 9);
        CHECK_EQ(state.rev_limit_rpm, 12000);
    }
}

void testRejectedFramesAndBounds()
{
    EcuTelemetryState state = {};
    for (uint32_t id : frameIds) {
        CHECK(decodeEcuCanFrame(state, id, rawWords, 8, id));
    }
    unsigned char before[sizeof(state)];
    std::memcpy(before, &state, sizeof(state));
    for (uint32_t id : frameIds) {
        for (size_t dlc = 0; dlc < 8; ++dlc) {
            GuardedPayload payload(dlc);
            if (dlc > 0) {
                std::memcpy(payload.data, rawWords, dlc);
            }
            CHECK(!decodeEcuCanFrame(state, id, payload.data, dlc, 90000));
            CHECK(std::memcmp(before, &state, sizeof(state)) == 0);
        }
        CHECK(!decodeEcuCanFrame(state, id, rawWords, 9, 90000));
        CHECK(!decodeEcuCanFrame(state, id, nullptr, 8, 90000));
        GuardedPayload unreadable(0);
        CHECK(!decodeEcuCanFrame(state, id, unreadable.data, 8, 90000, true, false));
        CHECK(!decodeEcuCanFrame(state, id, unreadable.data, 8, 90000, false, true));
        CHECK(std::memcmp(before, &state, sizeof(state)) == 0);
    }
    GuardedPayload unreadable(0);
    for (uint32_t id : {0u, 0x523u, 0x524u, 0x525u, 0x528u, 0x529u, 0x534u, 0x537u, 0x7FFu, 0x1522u, 0xFFFFFFFFu}) {
        CHECK(!decodeEcuCanFrame(state, id, unreadable.data, 8, 90000));
        CHECK(std::memcmp(before, &state, sizeof(state)) == 0);
    }
}

void testFreshnessAndRollover()
{
    EcuTelemetryState state = {};
    CHECK_EQ(ecuFreshMask(state, 0), 0);
    CHECK_EQ(ecuFreshMask(state, 0xFFFFFFFFu), 0);
    const uint8_t zero[8] = {};
    // Receiving an all-zero frame is different from never seeing its source.
    CHECK(decodeEcuCanFrame(state, 0x520, zero, 8, 0));
    CHECK_EQ(ecuFreshMask(state, 2000), TelemetryProtocol::SOURCE_520);
    CHECK_EQ(ecuFreshMask(state, 2001), 0);
    CHECK_EQ(state.received_mask, TelemetryProtocol::SOURCE_520);
    CHECK(decodeEcuCanFrame(state, 0x522, rawWords, 8, 2001));
    CHECK_EQ(ecuFreshMask(state, 2001), TelemetryProtocol::SOURCE_522);
    CHECK_EQ(state.received_mask, TelemetryProtocol::SOURCE_520 | TelemetryProtocol::SOURCE_522);
    CHECK_EQ(state.last_received_ms[0], 0u);
    CHECK_EQ(state.last_received_ms[2], 2001u);

    state = {};
    const uint32_t then = 0xFFFFFF00u;
    CHECK(decodeEcuCanFrame(state, 0x536, rawWords, 8, then));
    CHECK_EQ(ecuFreshMask(state, then + 2000u), TelemetryProtocol::SOURCE_536);
    CHECK_EQ(ecuFreshMask(state, then + 2001u), 0);
    CHECK_EQ(state.received_mask, TelemetryProtocol::SOURCE_536);
}

void testLambdaBoundaries()
{
    EcuTelemetryState state = {};
    const uint8_t averageMax[] = {0, 0, 0, 0, 0, 0, 0xFF, 0xFF};
    const uint8_t targetMin[] = {0, 0, 0, 0, 0, 0, 0x00, 0x80};
    CHECK(decodeEcuCanFrame(state, 0x520, averageMax, 8, 100));
    CHECK_EQ(state.lambda_avg_x1000, 65535);
    CHECK(decodeEcuCanFrame(state, 0x527, targetMin, 8, 101));
    CHECK_EQ(state.lambda_target_x1000, -32768);
    const uint8_t targetMax[] = {0, 0, 0, 0, 0, 0, 0xFF, 0x7F};
    state = {};
    CHECK(decodeEcuCanFrame(state, 0x527, targetMax, 8, 102));
    CHECK_EQ(state.lambda_target_x1000, 32767);
    CHECK_EQ(state.received_mask, TelemetryProtocol::SOURCE_527); // independent of average source
}

} // namespace

void runDecoderTests()
{
    testRepresentativeFrames();
    testEngineeringScalesAndSpeedRegression();
    testStatusBits();
    testRejectedFramesAndBounds();
    testFreshnessAndRollover();
    testLambdaBoundaries();
}
