#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

// Exercise the actual task adapter, scheduler, and queue policy without hardware.
#include "../src/Tasks/CAN.cpp"

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            std::cerr << __FILE__ << ':' << __LINE__ << ": " #condition " failed\n";                                   \
            std::exit(EXIT_FAILURE);                                                                                   \
        }                                                                                                              \
    } while (false)

namespace
{

struct MockQueue {
    size_t capacity = 24;
    size_t attempts = 0;
    std::vector<TelemetryPacket> packets;
};

struct ReceiveStep {
    uint32_t atMs;
    twai_message_t message;
    esp_err_t result;
};

struct TaskStopped {
};

uint32_t mockNowMs = 0;
size_t installCalls = 0;
size_t startCalls = 0;
size_t deleteCalls = 0;
size_t receiveIndex = 0;
esp_err_t installResult = ESP_OK;
esp_err_t startResult = ESP_OK;
std::vector<TickType_t> delayTicks;
std::vector<TickType_t> receiveWaitTicks;
std::vector<ReceiveStep> receiveSteps;

void resetTask()
{
    ecu = {};
    eventTracker = {};
    telemetrySeq = 0;
    lastFastTxMs = 0;
    lastSlowTxMs = 0;
    lastPowertrainTxMs = 0;
    mockNowMs = 0;
    installCalls = 0;
    startCalls = 0;
    deleteCalls = 0;
    receiveIndex = 0;
    installResult = ESP_OK;
    startResult = ESP_OK;
    delayTicks.clear();
    receiveWaitTicks.clear();
    receiveSteps.clear();
}

twai_message_t frame(uint32_t id, uint16_t a = 0, uint16_t b = 0, uint16_t c = 0, uint16_t d = 0)
{
    twai_message_t message = {};
    message.identifier = id;
    message.data_length_code = 8;
    const uint16_t words[] = {a, b, c, d};
    for (size_t i = 0; i < 4; ++i) {
        message.data[2 * i] = static_cast<uint8_t>(words[i]);
        message.data[2 * i + 1] = static_cast<uint8_t>(words[i] >> 8);
    }
    return message;
}

void decode(const twai_message_t& message)
{
    CHECK(decodeCanData(&message));
}

size_t packetCount(const MockQueue& queue, uint8_t type)
{
    size_t count = 0;
    for (const auto& packet : queue.packets) {
        count += packet.type == type;
    }
    return count;
}

uint16_t packetSequence(const TelemetryPacket& packet)
{
    switch (packet.type) {
    case TELEMETRY_PACKET_FAST:
        return packet.data.fast_v2.seq;
    case TELEMETRY_PACKET_SLOW:
        return packet.data.slow_v2.seq;
    case TELEMETRY_PACKET_EVENT:
        return packet.data.event_v2.seq;
    case TELEMETRY_PACKET_POWERTRAIN:
        return packet.data.powertrain_v2.seq;
    default:
        CHECK(false);
        return 0;
    }
}

void runScriptedTask(MockQueue& queue)
{
    CANTaskParameters parameters = {&queue};
    try {
        CANTask(&parameters);
    } catch (const TaskStopped&) {
        return;
    }
    CHECK(false); // A successful task should keep receiving until the script ends.
}

void testEmptyStateAndInvalidFrames()
{
    resetTask();
    MockQueue queue;
    mockNowMs = 6000;
    sendPeriodicPacketsIfDue(&queue);
    CHECK(queue.packets.empty());
    CHECK(telemetrySeq == 0);
    CHECK(!decodeCanData(nullptr));

    auto unknown = frame(0x123);
    CHECK(!decodeCanData(&unknown));
    auto invalid = frame(0x522, 125, 230, 0, 1234);
    invalid.data_length_code = 7;
    CHECK(!decodeCanData(&invalid));
    invalid.data_length_code = 9;
    CHECK(!decodeCanData(&invalid));
    invalid.data_length_code = 8;
    invalid.extd = true;
    CHECK(!decodeCanData(&invalid));
    invalid.extd = false;
    invalid.rtr = true;
    CHECK(!decodeCanData(&invalid));

    CHECK(!ecu.hasCanData);
    CHECK(ecu.received_mask == 0);
    CHECK(ecu.lastCanRxMs == 0);
    sendPeriodicPacketsIfDue(&queue);
    CHECK(queue.attempts == 0);
    CHECK(telemetryReceiveWaitTicks() == portMAX_DELAY);
}

void testTaskPipelineAndDriverConfiguration()
{
    resetTask();
    MockQueue queue;
    auto invalid = frame(0x522, 125, 230, 0, 999);
    invalid.data_length_code = 6;
    receiveSteps = {
        {500, frame(0x123), ESP_OK},
        {600, invalid, ESP_OK},
        {700, {}, ESP_FAIL},
        {1000, frame(0x522, 125, 230, 0, 1234), ESP_OK},
    };
    runScriptedTask(queue);

    CHECK(installCalls == 1);
    CHECK(startCalls == 1);
    CHECK(deleteCalls == 0);
    CHECK(delayTicks.size() == 1);
    CHECK(delayTicks[0] == CAN_TASK_PERIOD_MS);
    CHECK(queue.packets.size() == 1);
    const auto& packet = queue.packets[0];
    CHECK(packet.type == TELEMETRY_PACKET_FAST);
    CHECK(packet.data.fast_v2.ms == 1000);
    CHECK(packet.data.fast_v2.seq == 0);
    CHECK(packet.data.fast_v2.vehicle_speed_kph_x10 == 1234);
    CHECK(packet.data.fast_v2.received_mask == TelemetryV2::SOURCE_522);
    CHECK(packet.data.fast_v2.fresh_mask == TelemetryV2::SOURCE_522);
    CHECK(ecu.lastCanRxMs == 1000);
}

void testPeriodicBoundariesAndNoCatchUpBurst()
{
    resetTask();
    MockQueue queue;
    queue.capacity = 400;
    decode(frame(0x522, 125, 230, 0, 1234));
    for (uint32_t due = 20; due <= 6000; due += 20) {
        const size_t before = queue.packets.size();
        mockNowMs = due - 1;
        sendPeriodicPacketsIfDue(&queue);
        CHECK(queue.packets.size() == before);
        mockNowMs = due;
        sendPeriodicPacketsIfDue(&queue);
        const size_t expected = before + 1 + (due % 2000 == 0) + (due == 6000);
        CHECK(queue.packets.size() == expected);
        sendPeriodicPacketsIfDue(&queue);
        CHECK(queue.packets.size() == expected);
    }
    CHECK(packetCount(queue, TELEMETRY_PACKET_FAST) == 300);
    CHECK(packetCount(queue, TELEMETRY_PACKET_SLOW) == 3);
    CHECK(packetCount(queue, TELEMETRY_PACKET_POWERTRAIN) == 1);
    CHECK(queue.packets.back().data.powertrain_v2.ms == 6000);
    CHECK(queue.packets.back().data.powertrain_v2.fuel_inj_pulse_width_ms_x100 == 125);
    CHECK(queue.packets.back().data.powertrain_v2.fresh_mask == 0);
    CHECK(queue.packets.back().data.powertrain_v2.received_mask == TelemetryV2::SOURCE_522);
    for (size_t i = 0; i < queue.packets.size(); ++i) {
        CHECK(packetSequence(queue.packets[i]) == i);
    }

    const size_t beforeGap = queue.packets.size();
    mockNowMs = 18007;
    sendPeriodicPacketsIfDue(&queue);
    CHECK(queue.packets.size() == beforeGap + 3);
    CHECK(lastFastTxMs == 18000);
    CHECK(lastSlowTxMs == 18000);
    CHECK(lastPowertrainTxMs == 18000);
    CHECK(telemetryReceiveWaitTicks() == 13);
    sendPeriodicPacketsIfDue(&queue);
    CHECK(queue.packets.size() == beforeGap + 3);
    mockNowMs = 18019;
    sendPeriodicPacketsIfDue(&queue);
    CHECK(queue.packets.size() == beforeGap + 3);
    mockNowMs = 18020;
    sendPeriodicPacketsIfDue(&queue);
    CHECK(queue.packets.size() == beforeGap + 4);
    CHECK(queue.packets.back().data.fast_v2.ms == 18020);
}

void testSchedulerClockRollover()
{
    const uint32_t periods[] = {20, 2000, 6000};
    const uint8_t types[] = {TELEMETRY_PACKET_FAST, TELEMETRY_PACKET_SLOW, TELEMETRY_PACKET_POWERTRAIN};
    const uint32_t baseline = UINT32_MAX - 100;
    for (size_t i = 0; i < 3; ++i) {
        resetTask();
        MockQueue queue;
        mockNowMs = baseline;
        decode(frame(0x522));
        lastFastTxMs = baseline;
        lastSlowTxMs = baseline;
        lastPowertrainTxMs = baseline;
        mockNowMs = baseline + periods[i] - 1;
        sendPeriodicPacketsIfDue(&queue);
        CHECK(packetCount(queue, types[i]) == 0);
        queue.packets.clear();
        mockNowMs = baseline + periods[i];
        sendPeriodicPacketsIfDue(&queue);
        CHECK(packetCount(queue, types[i]) == 1);
    }

    resetTask();
    MockQueue queue;
    mockNowMs = UINT32_MAX - 9;
    decode(frame(0x522));
    lastFastTxMs = mockNowMs;
    lastSlowTxMs = mockNowMs;
    lastPowertrainTxMs = mockNowMs;
    CHECK(telemetryReceiveWaitTicks() == 20);
    mockNowMs = 9;
    CHECK(telemetryReceiveWaitTicks() == 1);
    sendPeriodicPacketsIfDue(&queue);
    CHECK(queue.packets.empty());
    mockNowMs = 10;
    CHECK(telemetryReceiveWaitTicks() == 0);
    sendPeriodicPacketsIfDue(&queue);
    CHECK(queue.packets.size() == 1);
    CHECK(lastFastTxMs == 10);
    CHECK(telemetryReceiveWaitTicks() == 20);
}

void testFullQueueDropsNewestAndConsumesSequence()
{
    resetTask();
    MockQueue queue;
    queue.capacity = 1;
    TelemetryPacket older = {};
    older.type = TELEMETRY_PACKET_FAST;
    older.data.fast_v2.seq = 777;
    older.data.fast_v2.vehicle_speed_kph_x10 = 2222;
    queue.packets.push_back(older);
    mockNowMs = 500;
    decode(frame(0x522, 125, 230, 0, 1234));
    sendPeriodicPacketsIfDue(&queue);
    CHECK(queue.attempts == 1);
    CHECK(queue.packets.size() == 1);
    CHECK(queue.packets[0].data.fast_v2.seq == 777);
    CHECK(queue.packets[0].data.fast_v2.vehicle_speed_kph_x10 == 2222);
    CHECK(telemetrySeq == 1);
    CHECK(lastFastTxMs == 500);

    queue.packets.clear();
    mockNowMs = 519;
    sendPeriodicPacketsIfDue(&queue);
    CHECK(queue.attempts == 1);
    mockNowMs = 520;
    sendPeriodicPacketsIfDue(&queue);
    CHECK(queue.packets.size() == 1);
    CHECK(queue.packets[0].data.fast_v2.seq == 1);
    CHECK(queue.packets[0].data.fast_v2.vehicle_speed_kph_x10 == 1234);
    CHECK(!sendTelemetryPacket(nullptr, older));
    CHECK(queue.attempts == 2);
}

void testTaskDeadlinesWithIrregularAndInvalidCan()
{
    resetTask();
    MockQueue queue;
    auto invalid = frame(0x520, 9999);
    invalid.data_length_code = 7;
    receiveSteps = {
        {5, frame(0x522, 125, 230, 0, 1234), ESP_OK},
        {13, frame(0x520, 1000), ESP_OK},
        {20, {}, ESP_ERR_TIMEOUT},
        {27, frame(0x520, 2000), ESP_OK},
        {40, {}, ESP_ERR_TIMEOUT},
        {59, frame(0x123), ESP_OK},
        {60, invalid, ESP_OK},
        {62, frame(0x520, 3000), ESP_OK},
        {80, {}, ESP_ERR_TIMEOUT},
        {100, {}, ESP_ERR_TIMEOUT},
    };
    runScriptedTask(queue);

    CHECK(queue.packets.size() == 5);
    const uint16_t expectedRpm[] = {1000, 2000, 2000, 3000, 3000};
    for (size_t i = 0; i < queue.packets.size(); ++i) {
        CHECK(queue.packets[i].type == TELEMETRY_PACKET_FAST);
        CHECK(queue.packets[i].data.fast_v2.ms == 20 * (i + 1));
        CHECK(queue.packets[i].data.fast_v2.rpm == expectedRpm[i]);
    }
    const std::vector<TickType_t> expectedWaits = {portMAX_DELAY, 15, 7, 20, 13, 20, 1, 20, 18, 20, 20};
    CHECK(receiveWaitTicks == expectedWaits);
    CHECK(delayTicks.empty()); // Deadline timeouts never add the old idle delay.
}

void testCanSilenceKeepsCadenceAndExpiresFreshness()
{
    resetTask();
    MockQueue queue;
    queue.capacity = 400;
    receiveSteps.push_back({0, frame(0x522, 125, 230, 0, 1234), ESP_OK});
    for (uint32_t atMs = 20; atMs <= 6020; atMs += 20) {
        receiveSteps.push_back({atMs, {}, ESP_ERR_TIMEOUT});
    }
    runScriptedTask(queue);
    CHECK(delayTicks.empty());
    CHECK(packetCount(queue, TELEMETRY_PACKET_FAST) == 301);
    CHECK(packetCount(queue, TELEMETRY_PACKET_SLOW) == 3);
    CHECK(packetCount(queue, TELEMETRY_PACKET_POWERTRAIN) == 1);
    CHECK(ecu.lastCanRxMs == 0);
    for (const auto& packet : queue.packets) {
        if (packet.type != TELEMETRY_PACKET_FAST) {
            continue;
        }
        const auto& fast = packet.data.fast_v2;
        CHECK(fast.vehicle_speed_kph_x10 == 1234);
        CHECK(fast.received_mask == TelemetryV2::SOURCE_522);
        CHECK(fast.fresh_mask == (fast.ms <= 2000 ? TelemetryV2::SOURCE_522 : 0));
    }
    CHECK(receiveWaitTicks.front() == portMAX_DELAY);
    for (size_t i = 1; i < receiveWaitTicks.size(); ++i) {
        CHECK(receiveWaitTicks[i] == 20);
    }
}

void testEventBaselineAdvancesWhenQueueDrops()
{
    resetTask();
    MockQueue queue;
    queue.capacity = 0;
    decode(frame(0x528, 100, 5, 7, 2));
    sendEventPacketIfNeeded(&queue);
    CHECK(queue.attempts == 0);
    CHECK(eventTracker.knock_count == 7);
    decode(frame(0x528, 100, 5, 8, 2));
    sendEventPacketIfNeeded(&queue);
    CHECK(queue.attempts == 1);
    CHECK(queue.packets.empty());
    CHECK(eventTracker.knock_count == 8);
    CHECK(telemetrySeq == 1);

    queue.capacity = 24;
    sendEventPacketIfNeeded(&queue);
    CHECK(queue.attempts == 1);
    decode(frame(0x528, 100, 5, 9, 2));
    sendEventPacketIfNeeded(&queue);
    CHECK(queue.packets.size() == 1);
    CHECK(queue.packets[0].type == TELEMETRY_PACKET_EVENT);
    CHECK(queue.packets[0].data.event_v2.seq == 1);
    CHECK(queue.packets[0].data.event_v2.knock_count == 9);
    CHECK(queue.packets[0].data.event_v2.alert_flags == TelemetryV2::KNOCK_COUNT_INCREMENTED);
    sendEventPacketIfNeeded(&queue);
    CHECK(queue.attempts == 2);
}

void testDriverInstallFailureStopsTask()
{
    resetTask();
    MockQueue queue;
    installResult = ESP_FAIL;
    CANTaskParameters parameters = {&queue};
    CANTask(&parameters);
    CHECK(installCalls == 1);
    CHECK(startCalls == 0);
    CHECK(deleteCalls == 1);
    CHECK(queue.attempts == 0);
}

} // namespace

MockSerial Serial;

uint32_t millis()
{
    return mockNowMs;
}

BaseType_t xQueueSend(QueueHandle_t handle, const void* packet, TickType_t waitTicks)
{
    CHECK(handle != nullptr);
    CHECK(packet != nullptr);
    CHECK(waitTicks == 0); // Every producer enqueue must remain nonblocking.
    auto& queue = *static_cast<MockQueue*>(handle);
    ++queue.attempts;
    if (queue.packets.size() >= queue.capacity) {
        return pdFALSE;
    }
    queue.packets.push_back(*static_cast<const TelemetryPacket*>(packet));
    return pdTRUE;
}

void vTaskDelay(TickType_t ticks)
{
    delayTicks.push_back(ticks);
    mockNowMs += ticks;
}

void vTaskDelete(void* task)
{
    CHECK(task == nullptr);
    ++deleteCalls;
}

esp_err_t twai_driver_install(const twai_general_config_t* general, const twai_timing_config_t* timing,
                              const twai_filter_config_t* filter)
{
    ++installCalls;
    CHECK(general->tx_io == CAN_TX);
    CHECK(general->rx_io == CAN_RX);
    CHECK(general->mode == TWAI_MODE_NORMAL);
    CHECK(timing->bitrate == 500000);
    CHECK(filter->accept_all);
    return installResult;
}

esp_err_t twai_start()
{
    ++startCalls;
    return startResult;
}

esp_err_t twai_receive(twai_message_t* message, uint32_t waitTicks)
{
    receiveWaitTicks.push_back(waitTicks);
    if (receiveIndex == receiveSteps.size()) {
        throw TaskStopped{};
    }
    const ReceiveStep& step = receiveSteps[receiveIndex++];
    mockNowMs = step.atMs;
    *message = step.message;
    return step.result;
}

int main()
{
    testEmptyStateAndInvalidFrames();
    testTaskPipelineAndDriverConfiguration();
    testPeriodicBoundariesAndNoCatchUpBurst();
    testSchedulerClockRollover();
    testFullQueueDropsNewestAndConsumesSequence();
    testTaskDeadlinesWithIrregularAndInvalidCan();
    testCanSilenceKeepsCadenceAndExpiresFreshness();
    testEventBaselineAdvancesWhenQueueDrops();
    testDriverInstallFailureStopsTask();
    std::cout << "CAN task integration tests passed (9 groups).\n";
    return 0;
}
