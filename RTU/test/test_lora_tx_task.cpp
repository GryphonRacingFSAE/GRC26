// Include the production task and link the production serializer. Compile in both
// board roles; the radio, queue, clock, and serial boundary alone are substituted.
#include <Arduino.h>
#include <LoRaAPI.h>
#include <TelemetrySender.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>

TestSerial Serial;
namespace
{
struct StopTask {
};
uint32_t clockMs = 0;
uint32_t millisecondsPerTick = 1;
int16_t initResult = 0;
int16_t txStartResult = 0;
int16_t rxStartResult = 0;
int16_t rxPollResult = RADIOLIB_ERR_RX_TIMEOUT;
bool initWasTx = false;
bool stopOnQueueEmpty = false;
bool stopOnRxStart = false;
unsigned initCalls = 0;
unsigned txPollCalls = 0;
unsigned rxStartCalls = 0;
unsigned checks = 0;
std::vector<TickType_t> delays;
std::vector<TickType_t> queueWaits;
std::vector<uint32_t> startTimes;
std::vector<std::vector<uint8_t>> sentFrames;
std::deque<int16_t> txResults;
std::vector<uint8_t> receivedFrame;

void check(bool condition, const char* expression, int line)
{
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "LoRa TX integration line %d: %s\n", line, expression);
        std::exit(EXIT_FAILURE);
    }
}
#define CHECK(expression) check((expression), #expression, __LINE__)
} // namespace

uint32_t millis()
{
    return clockMs;
}
TickType_t testMillisecondsToTicks(uint32_t milliseconds)
{
    return milliseconds / millisecondsPerTick;
}
void vTaskDelay(TickType_t ticks)
{
    delays.push_back(ticks);
    clockMs += ticks * millisecondsPerTick;
}
BaseType_t xQueueReceive(QueueHandle_t queue, void* packet, TickType_t wait)
{
    CHECK(queue != nullptr);
    queueWaits.push_back(wait);
    auto& packets = *static_cast<std::deque<TelemetryPacket>*>(queue);
    if (packets.empty()) {
        if (stopOnQueueEmpty) {
            throw StopTask{};
        }
        return 0;
    }
    *static_cast<TelemetryPacket*>(packet) = packets.front();
    packets.pop_front();
    return pdTRUE;
}
int16_t LoRaApiInit(bool txRole)
{
    ++initCalls;
    initWasTx = txRole;
    return initResult;
}
int16_t LoRaApiStartTransmit(const uint8_t* payload, size_t len)
{
    CHECK(payload != nullptr);
    CHECK(len > 0 && len <= TelemetryProtocol::MAX_RADIO_PAYLOAD);
    startTimes.push_back(clockMs);
    sentFrames.emplace_back(payload, payload + len);
    return txStartResult;
}
int16_t LoRaApiPollTransmit()
{
    ++txPollCalls;
    if (txResults.empty()) {
        return RADIOLIB_ERR_NONE;
    }
    const int16_t result = txResults.front();
    txResults.pop_front();
    return result;
}
int16_t LoRaApiStartReceive(uint32_t timeoutMs)
{
    ++rxStartCalls;
    CHECK(timeoutMs == LORA_RX_TIMEOUT_MS);
    if (stopOnRxStart) {
        throw StopTask{};
    }
    return rxStartResult;
}
int16_t LoRaApiPollReceive(uint8_t* buffer, size_t capacity, size_t* receivedLen)
{
    CHECK(buffer != nullptr && receivedLen != nullptr);
    CHECK(receivedFrame.size() <= capacity);
    *receivedLen = 0;
    if (rxPollResult == RADIOLIB_ERR_NONE) {
        std::copy(receivedFrame.begin(), receivedFrame.end(), buffer);
        *receivedLen = receivedFrame.size();
    }
    return rxPollResult;
}
float LoRaApiGetRSSI()
{
    return -85.0f;
}
float LoRaApiGetSNR()
{
    return 7.5f;
}

#include "../src/Tasks/LoRa.cpp"

namespace
{
void reset()
{
    clockMs = 0;
    millisecondsPerTick = 1;
    initResult = txStartResult = rxStartResult = RADIOLIB_ERR_NONE;
    rxPollResult = RADIOLIB_ERR_RX_TIMEOUT;
    initWasTx = stopOnQueueEmpty = stopOnRxStart = false;
    initCalls = txPollCalls = rxStartCalls = 0;
    txCount = lastTxReportMs = rxCount = 0;
    delays.clear();
    queueWaits.clear();
    startTimes.clear();
    sentFrames.clear();
    txResults.clear();
    receivedFrame.clear();
    Serial.lines.clear();
    Serial.pending.clear();
}
TelemetryPacket packet(uint8_t type, uint16_t sequence = 42)
{
    TelemetryPacket result = {};
    result.type = type;
    EcuTelemetryState ecu = {};
    populateFastPacket(result.data.fast, ecu, 123456, sequence);
    // All bodies share the populated ten-byte header; other fields are zero.
    return result;
}
bool logged(const char* text)
{
    for (const auto& line : Serial.lines) {
        if (line.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}
void testSerializerAndGuard()
{
    reset();
    CHECK(LORA_TX_GAP_MS == 3);
    const size_t lengths[] = {35, 31, 33, 59};
    const uint8_t types[] = {1, 2, 3, 5};
    for (size_t index = 0; index < 4; ++index) {
        const uint8_t type = types[index];
        const TelemetryPacket value = packet(type, type);
        txResults = {LORA_API_BUSY, LORA_API_BUSY, RADIOLIB_ERR_NONE};
        const uint32_t started = clockMs;
        transmitOnePacket(value);
        CHECK(sentFrames.size() == index + 1);
        const auto& frame = sentFrames.back();
        CHECK(frame.size() == lengths[index]);
        CHECK(frame[0] == 'T' && frame[1] == 'M' && frame[2] == 3 && frame[3] == type);
        CHECK(frame[4] == lengths[index] - 7);
        CHECK(frame[5] == 0x40 && frame[6] == 0xE2 && frame[7] == 1 && frame[8] == 0);
        CHECK(frame[9] == type && frame[10] == 0);
        const uint16_t crc = crc16_ccitt(frame.data(), frame.size() - 2);
        CHECK(frame[frame.size() - 2] == static_cast<uint8_t>(crc));
        CHECK(frame.back() == static_cast<uint8_t>(crc >> 8));
        CHECK(clockMs - started == 2 * LORA_TX_POLL_DELAY_MS + 3);
        CHECK(delays[delays.size() - 3] == LORA_TX_POLL_DELAY_MS);
        CHECK(delays[delays.size() - 2] == LORA_TX_POLL_DELAY_MS);
        CHECK(delays.back() == 3);
    }
    CHECK(txPollCalls == 12 && txCount == 4);
    CHECK(Serial.lines.empty());

    reset();
    millisecondsPerTick = 10;
    transmitOnePacket(packet(1));
    CHECK(delays.size() == 1 && delays[0] == 1);
    CHECK(clockMs == 10); // Minimum one tick even when a 3 ms conversion rounds down.
}
void testQueueAndErrors()
{
    reset();
    std::deque<TelemetryPacket> queue{packet(1, 41), packet(1, 42)};
    runTxTask(&queue);
    runTxTask(&queue);
    runTxTask(&queue);
    CHECK(queue.empty());
    CHECK(sentFrames.size() == 2 && txCount == 2);
    CHECK(startTimes[1] - startTimes[0] == 3);
    CHECK(sentFrames[0][9] == 41 && sentFrames[1][9] == 42);
    CHECK(queueWaits.size() == 3);
    CHECK(std::all_of(queueWaits.begin(), queueWaits.end(), [](TickType_t wait) { return wait == portMAX_DELAY; }));

    reset();
    transmitOnePacket(packet(99));
    CHECK(sentFrames.empty() && txPollCalls == 0 && delays.empty());
    CHECK(logged("Failed to build radio payload"));

    reset();
    txStartResult = -44;
    transmitOnePacket(packet(1));
    CHECK(sentFrames.size() == 1 && txPollCalls == 0 && txCount == 0);
    CHECK(logged("startTransmit failed: -44"));

    reset();
    txResults = {LORA_API_BUSY, RADIOLIB_ERR_TX_TIMEOUT};
    transmitOnePacket(packet(1));
    const uint32_t failedAt = clockMs - 3;
    CHECK(txCount == 0 && txPollCalls == 2 && sentFrames.size() == 1);
    CHECK(logged("[LoRa][TX] failed: -5") && delays.back() == 3);
    transmitOnePacket(packet(1));
    CHECK(startTimes[1] - failedAt == 3);
    CHECK(txCount == 1 && sentFrames.size() == 2);
}
void testRateLimitedLogging()
{
    reset();
    clockMs = 999;
    transmitOnePacket(packet(1));
    CHECK(Serial.lines.empty());
    transmitOnePacket(packet(1)); // Completes at 1002 ms, after the first guard.
    CHECK(Serial.lines.size() == 1 && logged("done count=2 bytes=35"));
    CHECK(lastTxReportMs == 1002);
    for (unsigned i = 0; i < 50; ++i) {
        clockMs = 1005 + i * 19;
        transmitOnePacket(packet(1));
    }
    CHECK(Serial.lines.size() == 1 && txCount == 52);
    clockMs = 1999;
    transmitOnePacket(packet(1));
    CHECK(Serial.lines.size() == 1);
    CHECK(clockMs == 2002);
    transmitOnePacket(packet(1));
    CHECK(Serial.lines.size() == 2 && logged("done count=54 bytes=35"));

    reset();
    lastTxReportMs = UINT32_MAX - 499;
    clockMs = 497;
    transmitOnePacket(packet(1));
    CHECK(Serial.lines.empty());
    CHECK(clockMs == 500);
    transmitOnePacket(packet(1));
    CHECK(Serial.lines.size() == 1 && lastTxReportMs == 500);
    txResults = {RADIOLIB_ERR_TX_TIMEOUT};
    transmitOnePacket(packet(1));
    CHECK(Serial.lines.size() == 2 && logged("failed: -5"));
    CHECK(lastTxReportMs == 500); // Error reports are never suppressed by success throttling.
}
void testTaskEntry()
{
    reset();
    std::deque<TelemetryPacket> queue{packet(1)};
    LoRaTaskParameters parameters{&queue};
    stopOnQueueEmpty = true;
    stopOnRxStart = true;
    bool stopped = false;
    try {
        LoRaTask(&parameters);
    } catch (const StopTask&) {
        stopped = true;
    }
    CHECK(stopped && initCalls == 1 && initWasTx == (LORA_ROLE_TX != 0));
    CHECK(logged("Task started") && logged("Radio initialized"));
#if LORA_ROLE_TX
    CHECK(sentFrames.size() == 1 && delays.back() == 3);
#else
    CHECK(sentFrames.empty() && rxStartCalls == 1);
#endif
    reset();
    initResult = -33;
    bool rejected = false;
    try {
        LoRaTask(&parameters);
    } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()).find("LoRaApiInit failed: -33") != std::string::npos;
    }
    CHECK(rejected && sentFrames.empty() && rxStartCalls == 0);
}
void testReceive()
{
    for (uint8_t type : {1, 2, 3, 5}) {
        reset();
        uint8_t output[96];
        size_t length = 0;
        CHECK(buildTelemetryRadioPayload(packet(type), output, sizeof(output), &length));
        receivedFrame.assign(output, output + length);
        runRxTask();
        rxPollResult = LORA_API_BUSY;
        runRxTask();
        CHECK(delays.size() == 1 && delays.back() == 5);
        rxPollResult = RADIOLIB_ERR_NONE;
        runRxTask();
        CHECK(rxCount == 1 && logged("seq=42") && logged("received=0x0 fresh=0x0"));
        CHECK(logged("body=") && !logged("invalid payload"));
        receivedFrame.back() ^= 1;
        runRxTask();
        runRxTask();
        CHECK(logged("invalid payload"));
    }
}
void testRejectUnsupportedProtocol()
{
    for (uint8_t type : {1, 2, 3, 5}) {
        uint8_t output[96];
        size_t length = 0;
        CHECK(buildTelemetryRadioPayload(packet(type), output, sizeof(output), &length));
        for (uint8_t version : {0, 1, 2, 4, 255}) {
            output[2] = version;
            const uint16_t crc = crc16_ccitt(output, length - 2);
            output[length - 2] = static_cast<uint8_t>(crc);
            output[length - 1] = static_cast<uint8_t>(crc >> 8);
            uint8_t decodedType = 99;
            const uint8_t* decodedBody = output;
            size_t decodedLength = 999;
            CHECK(!validateRadioPayload(output, length, &decodedType, &decodedBody, &decodedLength));
            CHECK(decodedType == 0 && decodedBody == nullptr && decodedLength == 0);
        }
    }
    for (uint8_t type : {0, 4, 255}) {
        uint8_t output[96];
        size_t length = 0;
        CHECK(buildTelemetryRadioPayload(packet(1), output, sizeof(output), &length));
        output[3] = type;
        const uint16_t crc = crc16_ccitt(output, length - 2);
        output[length - 2] = static_cast<uint8_t>(crc);
        output[length - 1] = static_cast<uint8_t>(crc >> 8);
        uint8_t decodedType = 0;
        const uint8_t* decodedBody = nullptr;
        size_t decodedLength = 0;
        CHECK(!validateRadioPayload(output, length, &decodedType, &decodedBody, &decodedLength));
    }
}
} // namespace

int main()
{
    testSerializerAndGuard();
    testQueueAndErrors();
    testRateLimitedLogging();
    testTaskEntry();
    testReceive();
    testRejectUnsupportedProtocol();
    std::printf("LoRa task integration: %u checks passed (LORA_ROLE_TX=%d)\n", checks, LORA_ROLE_TX);
    return EXIT_SUCCESS;
}
