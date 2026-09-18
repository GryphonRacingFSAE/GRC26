#include "TestSupport.h"
#include "TelemetrySender.h"

#include <sstream>
#include <vector>

namespace
{

static_assert(sizeof(TelemetryFastPacket) == 30, "Legacy RX fast layout changed");
static_assert(sizeof(TelemetrySlowPacket) == 46, "Legacy RX slow layout changed");
static_assert(sizeof(TelemetryEventPacket) == 28, "Legacy RX event layout changed");
static_assert(sizeof(TelemetryV2::FastPacket) == 28, "V2 fast wire size changed");
static_assert(sizeof(TelemetryV2::SlowPacket) == 32, "V2 slow wire size changed");
static_assert(sizeof(TelemetryV2::EventPacket) == 46, "V2 event wire size changed");
static_assert(sizeof(TelemetryV2::PowertrainPacket) == 58, "V2 powertrain wire size changed");
static_assert(sizeof(TelemetryPacket) == 59, "Queue must hold a whole supplemental packet");
static_assert(offsetof(TelemetryV2::PowertrainPacket, lambda_error_x1000) == 16,
              "Powertrain signed 32-bit lambda error offset changed");
static_assert(offsetof(TelemetryV2::EventPacket, lambda_error_x1000) == 24,
              "Event signed 32-bit lambda error offset changed");

const uint32_t snapshotTime = 0x12345678;
const uint16_t snapshotSequence = 0xABCD;

EcuTelemetryState sampleState()
{
    EcuTelemetryState state = {};
    state.hasCanData = true;
    state.received_mask = 0x3FFF;
    for (size_t index = 0; index < TelemetryV2::SOURCE_COUNT; ++index) {
        state.last_received_ms[index] = snapshotTime - (index % 2 == 0 ? 0u : 2001u);
    }
    state.rpm = 0x1101;
    state.tps_x10 = 0x1102;
    state.map_kpa_x10 = 0x1103;
    state.lambda_avg_x1000 = 0xFFFF;
    state.lambda_a_x1000 = 0x2201;
    state.lambda_b_x1000 = 0x2202;
    state.ignition_timing_deg_x10 = 0x2203;
    state.ignition_cut_percent = 13;
    state.lambda_target_x1000 = -32768;
    state.lambda_error_x1000 = 98303;
    state.fuel_inj_pulse_width_ms_x100 = 0x3301;
    state.fuel_inj_duty_x10 = 0x3302;
    state.fuel_cut_percent = 12;
    state.vehicle_speed_kph_x10 = 0x3304;
    state.non_driven_wheel_speed_kph_x10 = 0x4401;
    state.driven_wheel_speed_kph_x10 = 0x4402;
    state.traction_slip_measured_x10 = 0x4403;
    state.traction_slip_target_x10 = 0x4404;
    state.traction_cut_request_x10 = 0x5501;
    state.lambda_corr_a_x10 = 0x5502;
    state.lambda_corr_b_x10 = 0x5503;
    state.status_bits = 0xA55A;
    state.knock_level_peak = 0x6601;
    state.knock_correction_deg_x10 = 0x6602;
    state.knock_count = 0x6603;
    state.last_knock_cylinder = 0x6604;
    state.battery_v_x100 = 0x7701;
    state.baro_kpa_x10 = 0x7702;
    state.intake_air_temp_c_x10 = 0xF703;
    state.coolant_temp_c_x10 = 0xF704;
    state.egt_delta_c = 0x8801;
    state.ecu_temp_c = 0x8802;
    state.ecu_error_count = 0x8803;
    state.ecu_lost_sync_count = 0x8804;
    state.gear = 0x9901;
    state.boost_solenoid_duty_x10 = 0x9902;
    state.oil_pressure_kpa_x10 = 0x9903;
    state.oil_temp_c_x10 = -123;
    state.fuel_pressure_kpa_x10 = 0xAA01;
    state.coolant_pressure_kpa_x10 = 0xAA03;
    state.brake_pressure_kpa_x10 = 0xBB01;
    state.acceleration_x_mg = -321;
    state.acceleration_y_mg = 0;
    state.acceleration_z_mg = 32767;
    return state;
}

std::vector<uint8_t> hexBytes(const char* hex)
{
    std::istringstream input(hex);
    std::vector<uint8_t> result;
    unsigned byte = 0;
    while (input >> std::hex >> byte) {
        CHECK(byte <= 255);
        result.push_back(static_cast<uint8_t>(byte));
    }
    return result;
}

TelemetryPacket samplePacket(uint8_t type)
{
    const EcuTelemetryState state = sampleState();
    TelemetryPacket packet;
    // Every serialized byte must be written by the builder, including its header.
    std::memset(&packet, 0xA5, sizeof(packet));
    packet.type = type;
    switch (type) {
    case TELEMETRY_PACKET_FAST:
        populateFastPacket(packet.data.fast_v2, state, snapshotTime, snapshotSequence);
        break;
    case TELEMETRY_PACKET_SLOW:
        populateSlowPacket(packet.data.slow_v2, state, snapshotTime, snapshotSequence);
        break;
    case TELEMETRY_PACKET_EVENT:
        populateEventPacket(packet.data.event_v2, state, snapshotTime, snapshotSequence, 0x0E0F);
        break;
    case TELEMETRY_PACKET_POWERTRAIN:
        populatePowertrainPacket(packet.data.powertrain_v2, state, snapshotTime, snapshotSequence);
        break;
    }
    return packet;
}

void testPacketGoldensAndBounds()
{
    // Hand-specified schema bytes. CRC trailers independently cross-checked with
    // Python binascii.crc_hqx(header + payload, 0xFFFF), then encoded little-endian.
    const char* goldens[] = {
        "54 4D 02 01 1C 78 56 34 12 CD AB FF 3F 55 15 "
        "01 11 02 11 03 11 FF FF 03 99 01 77 04 33 01 BB 5A A5 77 EF",
        "54 4D 02 02 20 78 56 34 12 CD AB FF 3F 55 15 "
        "85 FF 04 F7 03 F7 02 88 01 88 01 AA 03 AA 03 88 04 88 03 66 04 66 AC 2F",
        "54 4D 02 03 2E 78 56 34 12 CD AB FF 3F 55 15 "
        "0F 0E 5A A5 01 11 03 99 01 AA 04 F7 01 77 FF 7F 01 00 03 88 04 88 "
        "03 66 04 66 0C 00 0D 00 01 55 01 66 02 66 8F 34",
        "54 4D 02 04 3A 78 56 34 12 CD AB FF 3F 55 15 "
        "01 22 02 22 00 80 FF 7F 01 00 01 33 02 33 0C 00 03 22 0D 00 02 44 "
        "01 44 03 44 04 44 01 55 02 55 03 55 01 99 02 99 01 66 02 66 BF FE "
        "00 00 FF 7F 85 C0",
    };
    const size_t lengths[] = {35, 39, 53, 65};
    for (uint8_t type = 1; type <= 4; ++type) {
        const TelemetryPacket packet = samplePacket(type);
        const std::vector<uint8_t> expected = hexBytes(goldens[type - 1]);
        CHECK_EQ(expected.size(), lengths[type - 1]);
        GuardedPayload exactOutput(expected.size());
        size_t length = 0;
        CHECK(buildTelemetryRadioPayload(packet, exactOutput.data, expected.size(), &length));
        CHECK_EQ(length, expected.size());
        for (size_t index = 0; index < expected.size(); ++index) {
            CHECK_EQ(exactOutput.data[index], expected[index]);
        }

        // No partial header, payload, or CRC may be emitted into a short buffer.
        for (size_t capacity = 0; capacity < expected.size(); ++capacity) {
            GuardedPayload shortOutput(capacity);
            if (capacity > 0) {
                std::memset(shortOutput.data, 0xA5, capacity);
            }
            length = 999;
            CHECK(!buildTelemetryRadioPayload(packet, shortOutput.data, capacity, &length));
            CHECK_EQ(length, 0u);
            for (size_t index = 0; index < capacity; ++index) {
                CHECK_EQ(shortOutput.data[index], 0xA5);
            }
        }
        uint8_t output[96];
        std::memset(output, 0xA5, sizeof(output));
        length = 999;
        CHECK(!buildTelemetryRadioPayload(packet, nullptr, sizeof(output), &length));
        CHECK_EQ(length, 0u);
        CHECK(!buildTelemetryRadioPayload(packet, output, sizeof(output), nullptr));
        for (uint8_t byte : output) {
            CHECK_EQ(byte, 0xA5);
        }
        CHECK(buildTelemetryRadioPayload(packet, output, sizeof(output), &length));
        CHECK_EQ(length, expected.size());
        CHECK(std::memcmp(output, expected.data(), expected.size()) == 0);
        for (size_t index = length; index < sizeof(output); ++index) {
            CHECK_EQ(output[index], 0xA5);
        }
    }
    for (uint8_t type : {0, 5, 255}) {
        TelemetryPacket packet = {};
        packet.type = type;
        GuardedPayload unreadable(0);
        size_t length = 999;
        CHECK(!buildTelemetryRadioPayload(packet, unreadable.data, 96, &length));
        CHECK_EQ(length, 0u);
    }
}

void testPacketValidityAndNegativeLambda()
{
    EcuTelemetryState state = {};
    TelemetryV2::FastPacket fast;
    populateFastPacket(fast, state, 0, 0);
    CHECK_EQ(fast.received_mask, 0);
    CHECK_EQ(fast.fresh_mask, 0);
    CHECK_EQ(fast.vehicle_speed_kph_x10, 0);
    const uint8_t speed[] = {0, 0, 0, 0, 0, 0, 0xD2, 0x04};
    CHECK(decodeEcuCanFrame(state, 0x522, speed, 8, 100));
    populateFastPacket(fast, state, 101, 1);
    CHECK_EQ(fast.vehicle_speed_kph_x10, 1234);
    CHECK_EQ(fast.received_mask, TelemetryV2::SOURCE_522);
    CHECK_EQ(fast.fresh_mask, TelemetryV2::SOURCE_522);
    populateFastPacket(fast, state, 2101, 2);
    CHECK_EQ(fast.vehicle_speed_kph_x10, 1234);
    CHECK_EQ(fast.received_mask, TelemetryV2::SOURCE_522);
    CHECK_EQ(fast.fresh_mask, 0);

    state.lambda_error_x1000 = -32767;
    TelemetryV2::PowertrainPacket powertrain;
    TelemetryV2::EventPacket event;
    populatePowertrainPacket(powertrain, state, 2101, 3);
    populateEventPacket(event, state, 2101, 4, 0);
    CHECK_EQ(powertrain.lambda_error_x1000, -32767);
    CHECK_EQ(event.lambda_error_x1000, -32767);
    const uint8_t negativeError[] = {0x01, 0x80, 0xFF, 0xFF};
    CHECK(std::memcmp(reinterpret_cast<const uint8_t*>(&powertrain) + 16, negativeError, sizeof(negativeError)) == 0);
    CHECK(std::memcmp(reinterpret_cast<const uint8_t*>(&event) + 24, negativeError, sizeof(negativeError)) == 0);
}

void testCounterEvents()
{
    using namespace TelemetryV2;
    EcuTelemetryState state = {};
    TelemetryEventTracker tracker;
    state.received_mask = SOURCE_528;
    state.knock_count = 100;
    CHECK_EQ(observeTelemetryEvents(state, tracker), 0); // initial count is a baseline
    CHECK_EQ(observeTelemetryEvents(state, tracker), 0);
    state.knock_count = 101;
    CHECK_EQ(observeTelemetryEvents(state, tracker), KNOCK_COUNT_INCREMENTED);
    CHECK_EQ(observeTelemetryEvents(state, tracker), 0); // no phantom clearing event
    state.knock_count = 500;
    CHECK_EQ(observeTelemetryEvents(state, tracker), KNOCK_COUNT_INCREMENTED);
    state.knock_count = 1;
    CHECK_EQ(observeTelemetryEvents(state, tracker), 0); // reset/decrease establishes baseline
    state.knock_count = 2;
    CHECK_EQ(observeTelemetryEvents(state, tracker), KNOCK_COUNT_INCREMENTED);
    state.knock_count = UINT16_MAX;
    CHECK_EQ(observeTelemetryEvents(state, tracker), KNOCK_COUNT_INCREMENTED);
    state.knock_count = 0;
    CHECK_EQ(observeTelemetryEvents(state, tracker), KNOCK_COUNT_INCREMENTED);
    state.knock_count = 65530;
    CHECK_EQ(observeTelemetryEvents(state, tracker), KNOCK_COUNT_INCREMENTED);
    state.knock_count = 3;
    CHECK_EQ(observeTelemetryEvents(state, tracker), 0); // ambiguous decrease is not an increment
    state.knock_count = 4;
    CHECK_EQ(observeTelemetryEvents(state, tracker), KNOCK_COUNT_INCREMENTED);

    state = {};
    tracker = {};
    state.received_mask = SOURCE_534;
    state.ecu_error_count = 65535;
    state.ecu_lost_sync_count = 60000;
    CHECK_EQ(observeTelemetryEvents(state, tracker), ECU_ERROR_CHANGED | LOST_SYNC_CHANGED);
    CHECK_EQ(observeTelemetryEvents(state, tracker), 0);
    state.ecu_error_count = 0;
    CHECK_EQ(observeTelemetryEvents(state, tracker), ECU_ERROR_CHANGED);
    state.ecu_lost_sync_count = 1;
    CHECK_EQ(observeTelemetryEvents(state, tracker), LOST_SYNC_CHANGED);
    state.ecu_lost_sync_count = 2;
    CHECK_EQ(observeTelemetryEvents(state, tracker), LOST_SYNC_CHANGED);
    state.knock_level_peak = UINT16_MAX;
    state.knock_correction_deg_x10 = UINT16_MAX;
    CHECK_EQ(observeTelemetryEvents(state, tracker), 0); // magnitudes are never severity flags
}

void testStatusAndCutEvents()
{
    using namespace TelemetryV2;
    EcuTelemetryState state = sampleState();
    state.received_mask = 0;
    TelemetryEventTracker tracker;
    CHECK_EQ(observeTelemetryEvents(state, tracker), 0); // unavailable sources cannot fire
    state.received_mask = SOURCE_526 | SOURCE_522 | SOURCE_521 | SOURCE_524;
    const uint16_t allEdges = STATUS_CHANGED | FUEL_CUT_CHANGED | IGNITION_CUT_CHANGED | TRACTION_CUT_CHANGED;
    CHECK_EQ(observeTelemetryEvents(state, tracker), allEdges);
    CHECK_EQ(observeTelemetryEvents(state, tracker), 0);
    ++state.fuel_cut_percent;
    ++state.ignition_cut_percent;
    ++state.traction_cut_request_x10;
    CHECK_EQ(observeTelemetryEvents(state, tracker), 0); // positive magnitude changes are not edges
    state.fuel_cut_percent = 0;
    CHECK_EQ(observeTelemetryEvents(state, tracker), FUEL_CUT_CHANGED);
    state.ignition_cut_percent = 0;
    CHECK_EQ(observeTelemetryEvents(state, tracker), IGNITION_CUT_CHANGED);
    state.traction_cut_request_x10 = 0;
    CHECK_EQ(observeTelemetryEvents(state, tracker), TRACTION_CUT_CHANGED);
    state.status_bits = 0;
    CHECK_EQ(observeTelemetryEvents(state, tracker), STATUS_CHANGED);
    state.status_bits = 0x8000;
    CHECK_EQ(observeTelemetryEvents(state, tracker), STATUS_CHANGED); // raw spare bit is preserved
    CHECK_EQ(observeTelemetryEvents(state, tracker), 0);
    state.fuel_cut_percent = 1;
    state.ignition_cut_percent = 1;
    state.traction_cut_request_x10 = 1;
    CHECK_EQ(observeTelemetryEvents(state, tracker), FUEL_CUT_CHANGED | IGNITION_CUT_CHANGED | TRACTION_CUT_CHANGED);
    state.rpm = 0;
    state.oil_pressure_kpa_x10 = 0;
    state.coolant_temp_c_x10 = UINT16_MAX;
    state.lambda_error_x1000 = 98303;
    CHECK_EQ(observeTelemetryEvents(state, tracker), 0);
}

} // namespace

void runSenderTests()
{
    testPacketGoldensAndBounds();
    testPacketValidityAndNegativeLambda();
    testCounterEvents();
    testStatusAndCutEvents();
}
