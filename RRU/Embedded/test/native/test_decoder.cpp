#include "Fixtures.h"

#include <limits>

namespace
{

static_assert(sizeof(TelemetryFastPacket) == 30, "V1 fast compatibility");
static_assert(sizeof(TelemetrySlowPacket) == 46, "V1 slow compatibility");
static_assert(sizeof(TelemetryEventPacket) == 28, "V1 event compatibility");
static_assert(sizeof(TelemetryV2::FastPacket) == 28, "V2 fast wire layout");
static_assert(sizeof(TelemetryV2::SlowPacket) == 32, "V2 slow wire layout");
static_assert(sizeof(TelemetryV2::EventPacket) == 46, "V2 event wire layout");
static_assert(sizeof(TelemetryV2::PowertrainPacket) == 58, "V2 powertrain wire layout");

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
    for (uint8_t version : {1, 2}) {
        for (uint8_t type = 1; type <= (version == 1 ? 3 : 4); ++type) {
            const std::vector<uint8_t> wire = hexBytes(version == 1 ? V1_GOLDENS[type - 1] : V2_GOLDENS[type - 1]);
            GuardedPayload exact(wire.size());
            std::memcpy(exact.data, wire.data(), wire.size());
            TelemetryReceivedPacket packet;
            std::memset(&packet, 0xA5, sizeof(packet));
            CHECK(decodeTelemetryRadioPayload(exact.data, wire.size(), packet) == TelemetryDecodeResult::Ok);
            CHECK_EQ(packet.version, version);
            CHECK_EQ(packet.packet.type, type);
            CHECK(std::memcmp(&packet.packet.data, wire.data() + 5, wire[4]) == 0);

            // Every truncated prefix ends at an inaccessible page. No decoder read
            // may reach the absent next byte, even for a complete header.
            for (size_t size = 0; size < wire.size(); ++size) {
                GuardedPayload truncated(size);
                if (size != 0) {
                    std::memcpy(truncated.data, wire.data(), size);
                }
                expectRejected(truncated.data, size);
            }
            std::vector<uint8_t> extra = wire;
            extra.push_back(0);
            expectRejected(extra.data(), extra.size());

            // Every single-bit corruption, including both CRC bytes, is rejected.
            for (size_t index = 0; index < wire.size(); ++index) {
                for (unsigned bit = 0; bit < 8; ++bit) {
                    std::vector<uint8_t> corrupt = wire;
                    corrupt[index] ^= static_cast<uint8_t>(1u << bit);
                    expectRejected(corrupt.data(), corrupt.size());
                }
            }

            // Correct CRC cannot make an unknown type/version or wrong magic valid.
            for (size_t index : {0u, 1u, 2u, 3u, 4u}) {
                std::vector<uint8_t> corrupt = wire;
                corrupt[index] = 0xFF;
                rewriteCrc(corrupt);
                expectRejected(corrupt.data(), corrupt.size());
            }
            if (version == 2) {
                // Only the 14 defined source bits are meaningful, and fresh bits
                // must be a subset of received bits.
                for (uint8_t invalidHighByte : {0x40, 0x80}) {
                    std::vector<uint8_t> corrupt = wire;
                    corrupt[12] |= invalidHighByte;
                    rewriteCrc(corrupt);
                    expectRejected(corrupt.data(), corrupt.size());
                }
                std::vector<uint8_t> corrupt = wire;
                corrupt[11] &= static_cast<uint8_t>(~1u);
                rewriteCrc(corrupt);
                expectRejected(corrupt.data(), corrupt.size());
            }
        }
    }
    expectRejected(nullptr, 0);
    expectRejected(nullptr, 7);
    expectRejected(nullptr, std::numeric_limits<size_t>::max());
    GuardedPayload empty(0);
    expectRejected(empty.data, 0);
    // An oversized frame must be rejected before reading any declared bytes.
    expectRejected(empty.data, TelemetryV2::MAX_RADIO_PAYLOAD + 1);
    expectRejected(empty.data, std::numeric_limits<size_t>::max());
}

void testV2Fields()
{
    const auto fast = decodeGolden(2, 1).packet.data.fast_v2;
    CHECK_EQ(fast.ms, 0x12345678u);
    CHECK_EQ(fast.seq, 0xABCD);
    CHECK_EQ(fast.received_mask, 0x3FFF);
    CHECK_EQ(fast.fresh_mask, 0x1555);
    CHECK_EQ(fast.rpm, 0x1101);
    CHECK_EQ(fast.tps_x10, 0x1102);
    CHECK_EQ(fast.map_kpa_x10, 0x1103);
    CHECK_EQ(fast.lambda_avg_x1000, 65535);
    CHECK_EQ(fast.oil_pressure_kpa_x10, 0x9903);
    CHECK_EQ(fast.battery_v_x100, 0x7701);
    CHECK_EQ(fast.vehicle_speed_kph_x10, 0x3304);
    CHECK_EQ(fast.brake_pressure_kpa_x10, 0xBB01);
    CHECK_EQ(fast.status_bits, 0xA55A);

    const auto slow = decodeGolden(2, 2).packet.data.slow_v2;
    CHECK_EQ(slow.ms, 0x12345678u);
    CHECK_EQ(slow.seq, 0xABCD);
    CHECK_EQ(slow.received_mask, 0x3FFF);
    CHECK_EQ(slow.fresh_mask, 0x1555);
    CHECK_EQ(slow.oil_temp_c_x10, -123);
    CHECK_EQ(slow.coolant_temp_c_x10, 0xF704);
    CHECK_EQ(slow.intake_air_temp_c_x10, 0xF703);
    CHECK_EQ(slow.ecu_temp_c, 0x8802);
    CHECK_EQ(slow.egt_delta_c, 0x8801);
    CHECK_EQ(slow.fuel_pressure_kpa_x10, 0xAA01);
    CHECK_EQ(slow.coolant_pressure_kpa_x10, 0xAA03);
    CHECK_EQ(slow.ecu_error_count, 0x8803);
    CHECK_EQ(slow.ecu_lost_sync_count, 0x8804);
    CHECK_EQ(slow.knock_count, 0x6603);
    CHECK_EQ(slow.last_knock_cylinder, 0x6604);

    const auto event = decodeGolden(2, 3).packet.data.event_v2;
    CHECK_EQ(event.ms, 0x12345678u);
    CHECK_EQ(event.seq, 0xABCD);
    CHECK_EQ(event.received_mask, 0x3FFF);
    CHECK_EQ(event.fresh_mask, 0x1555);
    CHECK_EQ(event.alert_flags, 0x0E0F);
    CHECK_EQ(event.status_bits, 0xA55A);
    CHECK_EQ(event.rpm, 0x1101);
    CHECK_EQ(event.oil_pressure_kpa_x10, 0x9903);
    CHECK_EQ(event.fuel_pressure_kpa_x10, 0xAA01);
    CHECK_EQ(event.coolant_temp_c_x10, 0xF704);
    CHECK_EQ(event.battery_v_x100, 0x7701);
    CHECK_EQ(event.lambda_error_x1000, 98303);
    CHECK_EQ(event.ecu_error_count, 0x8803);
    CHECK_EQ(event.ecu_lost_sync_count, 0x8804);
    CHECK_EQ(event.knock_count, 0x6603);
    CHECK_EQ(event.last_knock_cylinder, 0x6604);
    CHECK_EQ(event.fuel_cut_percent, 12);
    CHECK_EQ(event.ignition_cut_percent, 13);
    CHECK_EQ(event.traction_cut_request_x10, 0x5501);
    CHECK_EQ(event.knock_level_peak, 0x6601);
    CHECK_EQ(event.knock_correction_deg_x10, 0x6602);

    const auto powertrain = decodeGolden(2, 4).packet.data.powertrain_v2;
    CHECK_EQ(powertrain.ms, 0x12345678u);
    CHECK_EQ(powertrain.seq, 0xABCD);
    CHECK_EQ(powertrain.received_mask, 0x3FFF);
    CHECK_EQ(powertrain.fresh_mask, 0x1555);
    CHECK_EQ(powertrain.lambda_a_x1000, 0x2201);
    CHECK_EQ(powertrain.lambda_b_x1000, 0x2202);
    CHECK_EQ(powertrain.lambda_target_x1000, -32768);
    CHECK_EQ(powertrain.lambda_error_x1000, 98303);
    CHECK_EQ(powertrain.fuel_inj_pulse_width_ms_x100, 0x3301);
    CHECK_EQ(powertrain.fuel_inj_duty_x10, 0x3302);
    CHECK_EQ(powertrain.fuel_cut_percent, 12);
    CHECK_EQ(powertrain.ignition_timing_deg_x10, 0x2203);
    CHECK_EQ(powertrain.ignition_cut_percent, 13);
    CHECK_EQ(powertrain.driven_wheel_speed_kph_x10, 0x4402);
    CHECK_EQ(powertrain.non_driven_wheel_speed_kph_x10, 0x4401);
    CHECK_EQ(powertrain.traction_slip_measured_x10, 0x4403);
    CHECK_EQ(powertrain.traction_slip_target_x10, 0x4404);
    CHECK_EQ(powertrain.traction_cut_request_x10, 0x5501);
    CHECK_EQ(powertrain.lambda_corr_a_x10, 0x5502);
    CHECK_EQ(powertrain.lambda_corr_b_x10, 0x5503);
    CHECK_EQ(powertrain.gear, 0x9901);
    CHECK_EQ(powertrain.boost_solenoid_duty_x10, 0x9902);
    CHECK_EQ(powertrain.knock_level_peak, 0x6601);
    CHECK_EQ(powertrain.knock_correction_deg_x10, 0x6602);
    CHECK_EQ(powertrain.acceleration_x_mg, -321);
    CHECK_EQ(powertrain.acceleration_y_mg, 0);
    CHECK_EQ(powertrain.acceleration_z_mg, 32767);
}

void testV1Compatibility()
{
    const auto fast = decodeGolden(1, 1).packet.data.fast;
    CHECK_EQ(fast.ms, 0x12345678u);
    CHECK_EQ(fast.seq, 0xABCD);
    CHECK_EQ(fast.lambda_avg_x1000, 65535);
    CHECK_EQ(fast.lambda_error_x1000, -123);
    CHECK_EQ(fast.coolant_temp_c_x10, -123);
    CHECK_EQ(fast.gear, -123);
    CHECK_EQ(fast.status_bits, 0xA55A);
    const auto slow = decodeGolden(1, 2).packet.data.slow;
    CHECK_EQ(slow.ms, 0x12345678u);
    CHECK_EQ(slow.seq, 0xABCD);
    CHECK_EQ(slow.oil_temp_c_x10, -123);
    CHECK_EQ(slow.intake_air_temp_c_x10, -122);
    CHECK_EQ(slow.wastegate_pressure_kpa_x10, 0x1212);
    const auto event = decodeGolden(1, 3).packet.data.event;
    CHECK_EQ(event.ms, 0x12345678u);
    CHECK_EQ(event.seq, 0xABCD);
    CHECK_EQ(event.alert_flags, 0x0E0F);
    CHECK_EQ(event.status_bits, 0xA55A);
    CHECK_EQ(event.coolant_temp_c_x10, -123);
    CHECK_EQ(event.lambda_error_x1000, -123);
    CHECK_EQ(event.knock_count, 0x6603);
}

} // namespace

void runDecoderTests()
{
    testFramingAndBounds();
    testV2Fields();
    testV1Compatibility();
}
