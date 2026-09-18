#include "TestSupport.h"
#include "EcuTelemetry.h"

#include <type_traits>

namespace
{

const uint32_t frameIds[] = {0x520, 0x521, 0x522, 0x523, 0x524, 0x526, 0x527,
                             0x528, 0x530, 0x534, 0x536, 0x537, 0x538, 0x600};
const uint8_t rawWords[] = {0x34, 0x12, 0xCD, 0xAB, 0x78, 0x56, 0xDC, 0xFE};

static_assert(std::is_same<decltype(EcuTelemetryState::ignition_timing_deg_x10), uint16_t>::value,
              "DBC ignition timing is unsigned");
static_assert(std::is_same<decltype(EcuTelemetryState::coolant_temp_c_x10), uint16_t>::value,
              "DBC coolant temperature is unsigned");
static_assert(std::is_same<decltype(EcuTelemetryState::intake_air_temp_c_x10), uint16_t>::value,
              "DBC intake temperature is unsigned");
static_assert(std::is_same<decltype(EcuTelemetryState::gear), uint16_t>::value, "DBC gear is unsigned");
static_assert(std::is_same<decltype(EcuTelemetryState::oil_temp_c_x10), int16_t>::value,
              "DBC oil temperature is signed");
static_assert(std::is_same<decltype(EcuTelemetryState::lambda_target_x1000), int16_t>::value,
              "DBC lambda target is signed");
static_assert(std::is_same<decltype(EcuTelemetryState::lambda_error_x1000), int32_t>::value,
              "Lambda difference needs 32 bits to preserve its complete domain");

void testRepresentativeFrames()
{
    for (size_t index = 0; index < TelemetryV2::SOURCE_COUNT; ++index) {
        EcuTelemetryState state = {};
        GuardedPayload payload(8);
        std::memcpy(payload.data, rawWords, sizeof(rawWords));
        CHECK(decodeEcuCanFrame(state, frameIds[index], payload.data, 8, 12345));
        CHECK(state.hasCanData);
        CHECK_EQ(state.lastCanRxMs, 12345u);
        CHECK_EQ(state.received_mask, 1u << index);
        CHECK_EQ(ecuFreshMask(state, 12345), 1u << index);
        for (size_t source = 0; source < TelemetryV2::SOURCE_COUNT; ++source) {
            CHECK_EQ(state.last_received_ms[source], source == index ? 12345u : 0u);
        }

        // Independent DBC byte/word expectations; large unsigned values catch
        // accidental sign extension, clamping, or scaling a raw fixed-point word twice.
        switch (frameIds[index]) {
        case 0x520:
            CHECK_EQ(state.rpm, 0x1234);
            CHECK_EQ(state.tps_x10, 0xABCD);
            CHECK_EQ(state.map_kpa_x10, 0x5678);
            CHECK_EQ(state.lambda_avg_x1000, 0xFEDC);
            break;
        case 0x521:
            CHECK_EQ(state.lambda_a_x1000, 0x1234);
            CHECK_EQ(state.lambda_b_x1000, 0xABCD);
            CHECK_EQ(state.ignition_timing_deg_x10, 0x5678);
            CHECK_EQ(state.ignition_cut_percent, 0xFEDC);
            break;
        case 0x522:
            CHECK_EQ(state.fuel_inj_pulse_width_ms_x100, 0x1234);
            CHECK_EQ(state.fuel_inj_duty_x10, 0xABCD);
            CHECK_EQ(state.fuel_cut_percent, 0x5678);
            CHECK_EQ(state.vehicle_speed_kph_x10, 0xFEDC);
            break;
        case 0x523:
            CHECK_EQ(state.non_driven_wheel_speed_kph_x10, 0x1234);
            CHECK_EQ(state.driven_wheel_speed_kph_x10, 0xABCD);
            CHECK_EQ(state.traction_slip_measured_x10, 0x5678);
            CHECK_EQ(state.traction_slip_target_x10, 0xFEDC);
            break;
        case 0x524:
            CHECK_EQ(state.traction_cut_request_x10, 0x1234);
            CHECK_EQ(state.lambda_corr_a_x10, 0xABCD);
            CHECK_EQ(state.lambda_corr_b_x10, 0x5678);
            break;
        case 0x526:
            CHECK_EQ(state.status_bits, 0x1234);
            break;
        case 0x527:
            CHECK_EQ(state.lambda_target_x1000, -292);
            break;
        case 0x528:
            CHECK_EQ(state.knock_level_peak, 0x1234);
            CHECK_EQ(state.knock_correction_deg_x10, 0xABCD);
            CHECK_EQ(state.knock_count, 0x5678);
            CHECK_EQ(state.last_knock_cylinder, 0xFEDC);
            break;
        case 0x530:
            CHECK_EQ(state.battery_v_x100, 0x1234);
            CHECK_EQ(state.baro_kpa_x10, 0xABCD);
            CHECK_EQ(state.intake_air_temp_c_x10, 0x5678);
            CHECK_EQ(state.coolant_temp_c_x10, 0xFEDC);
            break;
        case 0x534:
            CHECK_EQ(state.egt_delta_c, 0x1234);
            CHECK_EQ(state.ecu_temp_c, 0xABCD);
            CHECK_EQ(state.ecu_error_count, 0x5678);
            CHECK_EQ(state.ecu_lost_sync_count, 0xFEDC);
            break;
        case 0x536:
            CHECK_EQ(state.gear, 0x1234);
            CHECK_EQ(state.boost_solenoid_duty_x10, 0xABCD);
            CHECK_EQ(state.oil_pressure_kpa_x10, 0x5678);
            CHECK_EQ(state.oil_temp_c_x10, -292);
            break;
        case 0x537:
            CHECK_EQ(state.fuel_pressure_kpa_x10, 0x1234);
            CHECK_EQ(state.coolant_pressure_kpa_x10, 0x5678);
            break;
        case 0x538:
            CHECK_EQ(state.brake_pressure_kpa_x10, 0x1234);
            break;
        case 0x600:
            CHECK_EQ(state.acceleration_x_mg, 0x1234);
            CHECK_EQ(state.acceleration_y_mg, -21555);
            CHECK_EQ(state.acceleration_z_mg, 0x5678);
            break;
        }
    }
}

void testEngineeringScalesAndSpeedRegression()
{
    EcuTelemetryState state = {};
    // 3.21 ms, 110.0% injection duty, 25% fuel cut, 123.4 km/h.
    const uint8_t injectors[] = {0x41, 0x01, 0x4C, 0x04, 0x19, 0x00, 0xD2, 0x04};
    CHECK_EQ(state.vehicle_speed_kph_x10, 0);
    CHECK(decodeEcuCanFrame(state, 0x522, injectors, 8, 0));
    CHECK_EQ(state.fuel_inj_pulse_width_ms_x100, 321);
    CHECK_EQ(state.fuel_inj_duty_x10, 1100);
    CHECK_EQ(state.fuel_cut_percent, 25);
    CHECK_EQ(state.vehicle_speed_kph_x10, 1234);

    // Lambda 0.987/1.023, timing 35.7 degrees, ignition cut 12%.
    const uint8_t combustion[] = {0xDB, 0x03, 0xFF, 0x03, 0x65, 0x01, 0x0C, 0x00};
    CHECK(decodeEcuCanFrame(state, 0x521, combustion, 8, 1));
    CHECK_EQ(state.lambda_a_x1000, 987);
    CHECK_EQ(state.lambda_b_x1000, 1023);
    CHECK_EQ(state.ignition_timing_deg_x10, 357);
    CHECK_EQ(state.ignition_cut_percent, 12);

    // Fourth gear, 45.6% boost duty, 345.6 kPa oil, -12.3 C oil.
    const uint8_t health[] = {0x04, 0x00, 0xC8, 0x01, 0x80, 0x0D, 0x85, 0xFF};
    CHECK(decodeEcuCanFrame(state, 0x536, health, 8, 2));
    CHECK_EQ(state.gear, 4);
    CHECK_EQ(state.boost_solenoid_duty_x10, 456);
    CHECK_EQ(state.oil_pressure_kpa_x10, 3456);
    CHECK_EQ(state.oil_temp_c_x10, -123);
}

void testStatusBits()
{
    struct NamedBit {
        unsigned bit;
        bool EcuStatusState::* member;
    };
    const NamedBit named[] = {
        {0, &EcuStatusState::shift_cut_active},
        {1, &EcuStatusState::rev_limit_active},
        {2, &EcuStatusState::anti_lag_active},
        {3, &EcuStatusState::launch_control_active},
        {4, &EcuStatusState::traction_power_limiter_active},
        {5, &EcuStatusState::throttle_blip_active},
        {7, &EcuStatusState::knock_detected},
        {8, &EcuStatusState::brake_pedal_active},
        {9, &EcuStatusState::clutch_pedal_active},
        {10, &EcuStatusState::speed_limiter_active},
        {11, &EcuStatusState::gp_limiter_active},
        {12, &EcuStatusState::user_cut_active},
        {13, &EcuStatusState::ecu_logging},
    };
    EcuTelemetryState state = {};
    for (unsigned bit = 0; bit < 16; ++bit) {
        const uint16_t raw = static_cast<uint16_t>(1u << bit);
        uint8_t frame[] = {static_cast<uint8_t>(raw), static_cast<uint8_t>(raw >> 8), 0, 0, 0, 0, 0, 0};
        CHECK(decodeEcuCanFrame(state, 0x526, frame, 8, bit));
        CHECK_EQ(state.status_bits, raw);
        for (const NamedBit& entry : named) {
            CHECK_EQ(state.status.*(entry.member), entry.bit == bit);
        }
    }
    const uint8_t cleared[8] = {};
    CHECK(decodeEcuCanFrame(state, 0x526, cleared, 8, 100));
    for (const NamedBit& entry : named) {
        CHECK(!(state.status.*(entry.member)));
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
    for (uint32_t id : {0u, 0x525u, 0x529u, 0x7FFu, 0x1522u, 0xFFFFFFFFu}) {
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
    CHECK_EQ(ecuFreshMask(state, 2000), TelemetryV2::SOURCE_520);
    CHECK_EQ(ecuFreshMask(state, 2001), 0);
    CHECK_EQ(state.received_mask, TelemetryV2::SOURCE_520);
    CHECK(decodeEcuCanFrame(state, 0x522, rawWords, 8, 2001));
    CHECK_EQ(ecuFreshMask(state, 2001), TelemetryV2::SOURCE_522);
    CHECK_EQ(state.received_mask, TelemetryV2::SOURCE_520 | TelemetryV2::SOURCE_522);
    CHECK_EQ(state.last_received_ms[0], 0u);
    CHECK_EQ(state.last_received_ms[2], 2001u);

    state = {};
    const uint32_t then = 0xFFFFFF00u;
    CHECK(decodeEcuCanFrame(state, 0x536, rawWords, 8, then));
    CHECK_EQ(ecuFreshMask(state, then + 2000u), TelemetryV2::SOURCE_536);
    CHECK_EQ(ecuFreshMask(state, then + 2001u), 0);
    CHECK_EQ(state.received_mask, TelemetryV2::SOURCE_536);
}

void testLambdaDomain()
{
    EcuTelemetryState state = {};
    const uint8_t averageMax[] = {0, 0, 0, 0, 0, 0, 0xFF, 0xFF};
    const uint8_t targetMin[] = {0, 0, 0, 0, 0, 0, 0x00, 0x80};
    CHECK(decodeEcuCanFrame(state, 0x520, averageMax, 8, 100));
    CHECK_EQ(state.lambda_error_x1000, 0); // target missing
    CHECK(decodeEcuCanFrame(state, 0x527, targetMin, 8, 101));
    CHECK_EQ(state.lambda_target_x1000, -32768);
    CHECK_EQ(state.lambda_error_x1000, 98303);
    const uint8_t averageZero[8] = {};
    const uint8_t targetMax[] = {0, 0, 0, 0, 0, 0, 0xFF, 0x7F};
    CHECK(decodeEcuCanFrame(state, 0x520, averageZero, 8, 102));
    CHECK_EQ(state.lambda_error_x1000, 32768);
    CHECK(decodeEcuCanFrame(state, 0x527, targetMax, 8, 103));
    CHECK_EQ(state.lambda_error_x1000, -32767);
    state = {};
    CHECK(decodeEcuCanFrame(state, 0x527, targetMax, 8, 0));
    CHECK_EQ(state.lambda_error_x1000, 0); // average missing
    CHECK(decodeEcuCanFrame(state, 0x520, averageZero, 8, 1));
    CHECK_EQ(state.lambda_error_x1000, -32767);
}

} // namespace

void runDecoderTests()
{
    testRepresentativeFrames();
    testEngineeringScalesAndSpeedRegression();
    testStatusBits();
    testRejectedFramesAndBounds();
    testFreshnessAndRollover();
    testLambdaDomain();
}
