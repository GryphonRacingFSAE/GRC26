#include <cstdlib>
#include <iostream>
#include <vector>

#include "driver/twai.h"
#include "../src/Tasks/CAN.cpp"

#if !TELEMETRY_MOCK_DATA
#error "Compile this integration test with TELEMETRY_MOCK_DATA=1"
#endif

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            std::cerr << __FILE__ << ':' << __LINE__ << ": " #condition " failed\n";                                   \
            std::exit(EXIT_FAILURE);                                                                                   \
        }                                                                                                              \
    } while (false)

namespace
{
struct TaskStopped {
};

constexpr uint32_t endTimeMs = 60000;
uint32_t clockMs = 0;
unsigned canCalls = 0;
unsigned deleteCalls = 0;
unsigned delayCalls = 0;
std::vector<TelemetryPacket> queuedPackets;

uint16_t readU16(const uint8_t* bytes)
{
    return static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8);
}

uint32_t readU32(const uint8_t* bytes)
{
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

// Independent byte-folded CRC-CCITT reference, unlike the production bit loop.
uint16_t referenceCrc(const uint8_t* bytes, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        uint16_t folded = static_cast<uint16_t>((crc >> 8) ^ bytes[i]);
        folded ^= folded >> 4;
        crc = static_cast<uint16_t>((crc << 8) ^ (folded << 12) ^ (folded << 5) ^ folded);
    }
    return crc;
}

void verifyMockStream()
{
    const size_t expectedLengths[] = {0, 35, 39, 53, 65};
    const uint32_t periods[] = {0, 20, 2000, 0, 6000};
    size_t counts[] = {0, 0, 0, 0, 0};
    bool eventSeen[13] = {};
    uint32_t previousTimestamp = 0;
    uint16_t expectedSequence = 0;

    for (const auto& packet : queuedPackets) {
        CHECK(packet.type >= 1 && packet.type <= 4);
        uint8_t bytes[TelemetryV2::MAX_RADIO_PAYLOAD] = {};
        size_t length = 0;
        CHECK(buildTelemetryRadioPayload(packet, bytes, sizeof(bytes), &length));
        CHECK(length == expectedLengths[packet.type]);
        CHECK(bytes[0] == 'T' && bytes[1] == 'M' && bytes[2] == 2);
        CHECK(bytes[3] == packet.type && bytes[4] == length - 7);
        CHECK(readU16(bytes + length - 2) == referenceCrc(bytes, length - 2));

        const uint32_t timestamp = readU32(bytes + 5);
        CHECK(timestamp >= previousTimestamp && timestamp <= endTimeMs);
        CHECK(readU16(bytes + 9) == expectedSequence++);
        CHECK(readU16(bytes + 11) == 0x3FFF); // All fourteen CAN sources received.
        CHECK(readU16(bytes + 13) == 0x3FFF); // Every source remains fresh.
        previousTimestamp = timestamp;
        ++counts[packet.type];

        if (periods[packet.type] != 0) {
            CHECK(timestamp == counts[packet.type] * periods[packet.type]);
        }

        const uint32_t seconds = timestamp / 1000;
        const uint32_t step = seconds % 60;
        const uint16_t status = ECU_STATUS_ECU_IS_LOGGING |
                                ((seconds / 5) % 2 ? ECU_STATUS_BRAKE_PEDAL_ACTIVE : 0);
        if (packet.type == TELEMETRY_PACKET_FAST) {
            CHECK(packet.data.fast_v2.rpm == 1000 + 100 * step);
            CHECK(packet.data.fast_v2.vehicle_speed_kph_x10 == 10 * step);
            CHECK(packet.data.fast_v2.status_bits == status);
        } else if (packet.type == TELEMETRY_PACKET_SLOW) {
            CHECK(packet.data.slow_v2.oil_temp_c_x10 == 850 + static_cast<int32_t>(step));
            CHECK(packet.data.slow_v2.coolant_temp_c_x10 == 800 + step);
            CHECK(packet.data.slow_v2.intake_air_temp_c_x10 == 250 + step);
            CHECK(packet.data.slow_v2.knock_count == seconds / 5);
            CHECK(packet.data.slow_v2.ecu_error_count == seconds / 10);
            CHECK(packet.data.slow_v2.ecu_lost_sync_count == seconds / 15);
        } else if (packet.type == TELEMETRY_PACKET_POWERTRAIN) {
            CHECK(packet.data.powertrain_v2.lambda_target_x1000 == 1000);
            CHECK(packet.data.powertrain_v2.lambda_error_x1000 == static_cast<int32_t>(step) - 50);
            CHECK(packet.data.powertrain_v2.acceleration_x_mg == static_cast<int32_t>(step) * 10 - 300);
            CHECK(packet.data.powertrain_v2.acceleration_y_mg == 300 - static_cast<int32_t>(step) * 10);
            CHECK(packet.data.powertrain_v2.acceleration_z_mg == 1000 + static_cast<int32_t>(step));
            CHECK(packet.data.powertrain_v2.gear == 1 + step / 10);
        } else if (packet.type == TELEMETRY_PACKET_EVENT) {
            CHECK(timestamp % 5000 == 0);
            CHECK(packet.data.event_v2.rpm == 1000 + 100 * step);
            CHECK(packet.data.event_v2.status_bits == status);
            CHECK(packet.data.event_v2.knock_count == seconds / 5);
            CHECK(packet.data.event_v2.ecu_error_count == seconds / 10);
            CHECK(packet.data.event_v2.ecu_lost_sync_count == seconds / 15);
            if (timestamp % 5000 == 0) {
                const size_t eventIndex = timestamp / 5000;
                CHECK(!eventSeen[eventIndex]);
                eventSeen[eventIndex] = true;
                uint16_t expectedFlags = TelemetryV2::STATUS_CHANGED;
                if (timestamp > 0) {
                    expectedFlags |= TelemetryV2::KNOCK_COUNT_INCREMENTED;
                    expectedFlags |= TelemetryV2::FUEL_CUT_CHANGED | TelemetryV2::IGNITION_CUT_CHANGED |
                                     TelemetryV2::TRACTION_CUT_CHANGED;
                    if (timestamp % 10000 == 0) {
                        expectedFlags |= TelemetryV2::ECU_ERROR_CHANGED;
                    }
                    if (timestamp % 15000 == 0) {
                        expectedFlags |= TelemetryV2::LOST_SYNC_CHANGED;
                    }
                }
                CHECK(packet.data.event_v2.alert_flags == expectedFlags);
            }
        }
    }

    CHECK(counts[TELEMETRY_PACKET_FAST] == 3000);
    CHECK(counts[TELEMETRY_PACKET_SLOW] == 30);
    CHECK(counts[TELEMETRY_PACKET_POWERTRAIN] == 10);
    CHECK(counts[TELEMETRY_PACKET_EVENT] == 13);
    for (bool seen : eventSeen) {
        CHECK(seen);
    }
    CHECK(ecu.hasCanData && ecu.received_mask == 0x3FFF);
    CHECK(ecu.lastCanRxMs == endTimeMs);
    for (uint32_t receivedAt : ecu.last_received_ms) {
        CHECK(receivedAt == endTimeMs);
    }
    // The mock source loops its driving values after a minute; counters keep rising.
    CHECK(ecu.rpm == 1000 && ecu.vehicle_speed_kph_x10 == 0);
    CHECK(ecu.knock_count == 12 && ecu.ecu_error_count == 6 && ecu.ecu_lost_sync_count == 4);
}
} // namespace

MockSerial Serial;

uint32_t millis()
{
    return clockMs;
}

BaseType_t xQueueSend(QueueHandle_t queue, const void* packet, TickType_t waitTicks)
{
    CHECK(queue == &queuedPackets && packet != nullptr);
    CHECK(waitTicks == 0);
    queuedPackets.push_back(*static_cast<const TelemetryPacket*>(packet));
    return pdTRUE;
}

void vTaskDelay(TickType_t ticks)
{
    CHECK(ticks > 0 && ticks <= 20);
    ++delayCalls;
    if (clockMs + ticks > endTimeMs) {
        throw TaskStopped{};
    }
    clockMs += ticks;
}

void vTaskDelete(void*)
{
    ++deleteCalls;
    throw TaskStopped{};
}

esp_err_t twai_driver_install(const twai_general_config_t*, const twai_timing_config_t*, const twai_filter_config_t*)
{
    ++canCalls;
    throw TaskStopped{};
}

esp_err_t twai_start()
{
    ++canCalls;
    throw TaskStopped{};
}

esp_err_t twai_receive(twai_message_t*, uint32_t)
{
    ++canCalls;
    throw TaskStopped{};
}

int main()
{
    CANTaskParameters parameters = {&queuedPackets};
    try {
        CANTask(&parameters);
        CHECK(false); // A live producer must continue until the simulated clock stops it.
    } catch (const TaskStopped&) {
    }
    CHECK(canCalls == 0 && deleteCalls == 0);
    CHECK(clockMs == endTimeMs && delayCalls > 0);
    verifyMockStream();
    std::cout << "Mock CAN task tests passed (60 seconds, " << queuedPackets.size() << " serialized packets).\n";
}
