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
    const size_t expectedLengths[] = {0, 35, 31, 33, 0, 59};
    const uint32_t periods[] = {0, 20, 2000, 0, 0, 200};
    size_t counts[] = {0, 0, 0, 0, 0, 0};
    bool eventSeen[13] = {};
    uint32_t previousTimestamp = 0;
    uint16_t expectedSequence = 0;

    for (const auto& packet : queuedPackets) {
        CHECK(packet.type == 1 || packet.type == 2 || packet.type == 3 || packet.type == 5);
        uint8_t bytes[TelemetryProtocol::MAX_RADIO_PAYLOAD] = {};
        size_t length = 0;
        CHECK(buildTelemetryRadioPayload(packet, bytes, sizeof(bytes), &length));
        CHECK(length == expectedLengths[packet.type]);
        CHECK(bytes[0] == 'T' && bytes[1] == 'M' && bytes[2] == 3);
        CHECK(bytes[3] == packet.type && bytes[4] == length - 7);
        CHECK(readU16(bytes + length - 2) == referenceCrc(bytes, length - 2));

        const uint32_t timestamp = readU32(bytes + 5);
        CHECK(timestamp >= previousTimestamp && timestamp <= endTimeMs);
        CHECK(readU16(bytes + 9) == expectedSequence++);
        CHECK(readU16(bytes + 11) == 0xFFFF); // All sixteen CAN sources received.
        CHECK(readU16(bytes + 13) == 0xFFFF); // Every source remains fresh.
        previousTimestamp = timestamp;
        ++counts[packet.type];

        if (periods[packet.type] != 0) {
            CHECK(timestamp == counts[packet.type] * periods[packet.type]);
        }

        const uint32_t seconds = timestamp / 1000;
        const uint32_t step = seconds % 60;
        const uint16_t status = (seconds / 5) % 2 ? ECU_STATUS_BRAKE_PEDAL_ACTIVE : 0;
        if (packet.type == TELEMETRY_PACKET_FAST) {
            CHECK(packet.data.fast.rpm == 1000 + 100 * step);
            CHECK(packet.data.fast.vehicle_speed_kph_x10 == 10 * step);
            CHECK(packet.data.fast.status_bits == status);
        } else if (packet.type == TELEMETRY_PACKET_SLOW) {
            CHECK(packet.data.slow.coolant_temp_c_x10 == 800 + step);
            CHECK(packet.data.slow.intake_air_temp_c_x10 == 250 + step);
            CHECK(packet.data.slow.lambda_target_x1000 == 1000);
        } else if (packet.type == TELEMETRY_PACKET_SENSORS) {
            const auto& sensors = packet.data.sensors;
            CHECK(sensors.acceleration_x_mg == static_cast<int32_t>(step) * 10 - 300);
            CHECK(sensors.acceleration_y_mg == 300 - static_cast<int32_t>(step) * 10);
            CHECK(sensors.acceleration_z_mg == 1000 + static_cast<int32_t>(step));
            CHECK(sensors.aero_pressure_1_pa == -100 - static_cast<int32_t>(step));
            CHECK(sensors.aero_pressure_2_pa == 200 + static_cast<int32_t>(step));
            CHECK(sensors.aero_ambient_temp_c_x100 == 2500 + static_cast<int32_t>(step));
            CHECK(sensors.aero_ambient_pressure_hpa_x10 == 10132);
            CHECK(sensors.aero_node_state == 1 && sensors.imu_node_state == 1);
            CHECK(sensors.aero_sensor_flags == 3 && sensors.imu_sensor_flags == 3);
            CHECK(sensors.aero_fault_flags == 0 && sensors.imu_fault_flags == 0);
            CHECK(sensors.aero_sequence == static_cast<uint8_t>(timestamp / 20));
            CHECK(sensors.imu_sequence == static_cast<uint8_t>(timestamp / 20));
            CHECK(sensors.yaw_rate_dps_x100 == -500 + static_cast<int32_t>(step));
            CHECK(sensors.pitch_rate_dps_x100 == 100 + static_cast<int32_t>(step));
            CHECK(sensors.roll_rate_dps_x100 == 200 + static_cast<int32_t>(step));
            CHECK(sensors.gps_latitude_deg_x1e7 == 430000000 + static_cast<int32_t>(step) * 100);
            CHECK(sensors.gps_longitude_deg_x1e7 == -790000000 + static_cast<int32_t>(step) * 100);
            CHECK(sensors.gps_ground_speed_kph_x100 == 100 * step);
            CHECK(sensors.gps_course_deg_x100 == 9000 + step);
        } else if (packet.type == TELEMETRY_PACKET_EVENT) {
            CHECK(timestamp % 5000 == 0);
            CHECK(packet.data.event.rpm == 1000 + 100 * step);
            CHECK(packet.data.event.status_bits == status);
            const size_t eventIndex = timestamp / 5000;
            CHECK(!eventSeen[eventIndex]);
            eventSeen[eventIndex] = true;
            const uint16_t expectedFlags = timestamp == 0
                ? TelemetryProtocol::AERO_STATUS_CHANGED | TelemetryProtocol::IMU_STATUS_CHANGED : TelemetryProtocol::STATUS_CHANGED;
            CHECK(packet.data.event.alert_flags == expectedFlags);
        }
    }

    CHECK(counts[TELEMETRY_PACKET_FAST] == 3000);
    CHECK(counts[TELEMETRY_PACKET_SLOW] == 30);
    CHECK(counts[TELEMETRY_PACKET_SENSORS] == 300);
    CHECK(counts[TELEMETRY_PACKET_EVENT] == 13);
    for (bool seen : eventSeen) {
        CHECK(seen);
    }
    CHECK(ecu.hasCanData && ecu.received_mask == 0xFFFF);
    CHECK(ecu.lastCanRxMs == endTimeMs);
    for (uint32_t receivedAt : ecu.last_received_ms) {
        CHECK(receivedAt == endTimeMs);
    }
    // Driving values repeat after a minute; node sequence bytes continue to roll over.
    CHECK(ecu.rpm == 1000 && ecu.vehicle_speed_kph_x10 == 0);
    CHECK(ecu.aero_sequence == static_cast<uint8_t>(3000));
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
