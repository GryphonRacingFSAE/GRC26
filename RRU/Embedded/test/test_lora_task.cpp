// Compile the actual firmware task against serial/radio fakes. Decoder and CSV
// formatter are linked unchanged so these tests cover the complete receive path.
#include <Arduino.h>
#include <RadioLib.h>
#include <LoRaAPI.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <stdexcept>

TestSerial Serial;

namespace
{
struct StopTask {
};
uint32_t clockMs = 987654;
int16_t initResult = RADIOLIB_ERR_NONE;
int16_t startResult = RADIOLIB_ERR_NONE;
int16_t pollResult = RADIOLIB_ERR_NONE;
unsigned initCalls = 0;
unsigned startCalls = 0;
unsigned pollCalls = 0;
bool lastInitWasTx = true;
bool stopAfterLoopDelay = false;
uint32_t lastTimeoutMs = 0;
std::vector<uint32_t> delays;
std::vector<uint8_t> radioBytes;
std::vector<std::string> operations;
float radioRssi = -92.25f;
float radioSnr = 7.5f;
unsigned checks = 0;
unsigned groups = 0;

void check(bool condition, const char* expression, int line)
{
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "LoRa integration line %d: %s\n", line, expression);
        std::exit(1);
    }
}
#define CHECK(expression) check((expression), #expression, __LINE__)
} // namespace

uint32_t millis()
{
    operations.emplace_back("millis");
    return clockMs;
}

void testSerialPrinted()
{
    operations.emplace_back("print");
}

void vTaskDelay(uint32_t ticks)
{
    delays.push_back(ticks);
    if (stopAfterLoopDelay && ticks == 1) {
        throw StopTask{};
    }
}

int16_t LoRaApiInit(bool txRole)
{
    ++initCalls;
    lastInitWasTx = txRole;
    return initResult;
}

int16_t LoRaApiStartReceive(uint32_t timeoutMs)
{
    operations.emplace_back("start");
    ++startCalls;
    lastTimeoutMs = timeoutMs;
    if (startCalls > 1) {
        // Model metadata changing as soon as the radio enters its next receive.
        radioRssi = -120.0f;
        radioSnr = -3.0f;
        clockMs += 17;
    }
    return startResult;
}

int16_t LoRaApiPollReceive(uint8_t* buffer, size_t bufferSize, size_t* receivedLen)
{
    operations.emplace_back("poll");
    ++pollCalls;
    CHECK(buffer != nullptr);
    CHECK(receivedLen != nullptr);
    CHECK(bufferSize == RADIOLIB_LR11X0_MAX_PACKET_LENGTH);
    *receivedLen = 0;
    if (pollResult == RADIOLIB_ERR_NONE) {
        CHECK(radioBytes.size() <= bufferSize);
        std::copy(radioBytes.begin(), radioBytes.end(), buffer);
        *receivedLen = radioBytes.size();
    }
    return pollResult;
}

float LoRaApiGetRSSI()
{
    operations.emplace_back("rssi");
    return radioRssi;
}
float LoRaApiGetSNR()
{
    operations.emplace_back("snr");
    return radioSnr;
}

#include "../src/Tasks/LoRa.cpp"

namespace
{

void reset()
{
    rxActive = false;
    rxPacketCount = 0;
    csvHeaderPrinted = false;
    std::memset(csvBuffer, 0xA5, sizeof(csvBuffer));
    Serial.lines.clear();
    delays.clear();
    radioBytes.clear();
    operations.clear();
    radioRssi = -92.25f;
    radioSnr = 7.5f;
    initCalls = startCalls = pollCalls = 0;
    initResult = startResult = pollResult = RADIOLIB_ERR_NONE;
    lastInitWasTx = true;
    lastTimeoutMs = 0;
    stopAfterLoopDelay = false;
    clockMs = 987654;
}

void put16(std::vector<uint8_t>& bytes, size_t offset, uint16_t value)
{
    bytes.at(offset) = static_cast<uint8_t>(value);
    bytes.at(offset + 1) = static_cast<uint8_t>(value >> 8);
}

void put32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value)
{
    put16(bytes, offset, static_cast<uint16_t>(value));
    put16(bytes, offset + 2, static_cast<uint16_t>(value >> 16));
}

std::vector<uint8_t> frame(uint8_t version, uint8_t type, const std::vector<uint8_t>& body)
{
    std::vector<uint8_t> bytes{'T', 'M', version, type, static_cast<uint8_t>(body.size())};
    bytes.insert(bytes.end(), body.begin(), body.end());
    uint16_t crc = 0xFFFF;
    for (uint8_t value : bytes) {
        crc ^= static_cast<uint16_t>(value) << 8;
        for (unsigned bit = 0; bit < 8; ++bit) {
            const bool high = (crc & 0x8000) != 0;
            crc = static_cast<uint16_t>(crc << 1);
            if (high) {
                crc ^= 0x1021;
            }
        }
    }
    bytes.push_back(static_cast<uint8_t>(crc));
    bytes.push_back(static_cast<uint8_t>(crc >> 8));
    return bytes;
}

std::vector<uint8_t> sensorsFrame()
{
    std::vector<uint8_t> body(52, 0);
    put32(body, 0, 123456);
    put16(body, 4, 42);
    put16(body, 6, 0xFFFF);
    put16(body, 8, 0xFFFF);
    put16(body, 10, static_cast<uint16_t>(-1000));
    put16(body, 14, static_cast<uint16_t>(-1234));
    put16(body, 23, static_cast<uint16_t>(-1000));
    put32(body, 40, static_cast<uint32_t>(-1));
    put32(body, 44, 1234567890);
    return frame(3, 5, body);
}

std::vector<std::string> split(const std::string& line)
{
    std::vector<std::string> fields;
    size_t begin = 0;
    for (;;) {
        const size_t comma = line.find(',', begin);
        fields.push_back(line.substr(begin, comma == std::string::npos ? comma : comma - begin));
        if (comma == std::string::npos) {
            return fields;
        }
        begin = comma + 1;
    }
}

std::map<std::string, std::string> row(const std::string& line)
{
    const auto names = split(telemetryCsvHeader());
    const auto values = split(line);
    CHECK(names.size() == 56);
    CHECK(values.size() == names.size());
    std::map<std::string, std::string> result;
    for (size_t index = 0; index < names.size(); ++index) {
        result[names[index]] = values[index];
    }
    return result;
}

void receive()
{
    LoRaRxTaskPoll();
    CHECK(rxActive);
    CHECK(lastTimeoutMs == LORA_RX_TIMEOUT_MS);
    operations.clear();
    LoRaRxTaskPoll();
}

void checkPacketOperationOrder()
{
    const std::vector<std::string> expected{"poll", "rssi", "snr", "millis", "start", "print"};
    CHECK(operations == expected);
}

void testSensors()
{
    reset();
    radioBytes = sensorsFrame();
    receive();
    CHECK(rxActive);
    CHECK(startCalls == 2);
    checkPacketOperationOrder();
    CHECK(rxPacketCount == 1);
    CHECK(Serial.lines.size() == 1);
    const auto values = row(Serial.lines.front());
    CHECK(values.at("event") == "rx_packet");
    CHECK(values.at("packet_type") == "SENSORS");
    CHECK(values.at("schema_version") == "3");
    CHECK(values.at("rx_count") == "1");
    CHECK(values.at("rx_ms") == "987654");
    CHECK(values.at("rssi_dbm") == "-92.25");
    CHECK(values.at("snr_db") == "7.50");
    CHECK(values.at("radio_len") == "59");
    CHECK(values.at("seq") == "42");
    CHECK(values.at("tx_ms") == "123456");
    CHECK(values.at("received_mask_hex") == "0xFFFF");
    CHECK(values.at("fresh_mask_hex") == "0xFFFF");
    CHECK(values.at("aero_pressure_1_pa") == "-1000");
    CHECK(values.at("aero_ambient_temp_c") == "-12.34");
    CHECK(values.at("acceleration_x_g") == "-1.000");
    CHECK(values.at("gps_latitude_deg") == "-0.0000001");
    CHECK(values.at("gps_longitude_deg") == "123.4567890");
    CHECK(values.at("rpm").empty());
    ++groups;
}

void testUnsupportedVersions()
{
    for (uint8_t version : {1, 2}) {
        reset();
        std::vector<uint8_t> body(28, 0);
        radioBytes = frame(version, 1, body);
        receive();
        CHECK(rxActive);
        checkPacketOperationOrder();
        CHECK(Serial.lines.size() == 1);
        const auto values = row(Serial.lines.front());
        CHECK(values.at("event") == "rx_error");
        CHECK(values.at("error_text") == "telemetry_version_unsupported");
        CHECK(values.at("schema_version").empty());
        CHECK(values.at("rpm").empty());
    }
    ++groups;
}

void testIndependentPackets()
{
    reset();
    LoRaRxTaskPoll();
    CHECK(rxActive);
    for (uint8_t type : {1, 2, 3, 5}) {
        std::vector<uint8_t> body(type == 5 ? 52 : type == 3 ? 26 : type == 2 ? 24 : 28, 0);
        put32(body, 0, 2000);
        put16(body, 4, type);
        // Only one CAN source has arrived. None of these independent packets
        // should be held waiting for other CAN IDs or other telemetry types.
        const uint16_t source = type == 5 ? 0x4000 : type == 2 ? 0x0002 : type == 3 ? 0x2000 : 0x0001;
        put16(body, 6, source);
        put16(body, 8, type == 5 ? 0 : source);
        if (type == 1) {
            put16(body, 10, 7500);
        } else if (type == 2) {
            put16(body, 10, 1001);
        } else if (type == 3) {
            put16(body, 10, 4);
            body[21] = 2;
        } else {
            put32(body, 40, static_cast<uint32_t>(-1));
            put32(body, 44, 1234567890);
        }
        radioBytes = frame(3, type, body);
        operations.clear();
        LoRaRxTaskPoll();
        checkPacketOperationOrder();
        CHECK(rxActive);
        CHECK(delays.empty());
        const auto values = row(Serial.lines.back());
        CHECK(values.at("event") == "rx_packet");
        CHECK(values.at("schema_version") == "3");
        CHECK(values.at("packet_type") == (type == 1 ? "FAST" : type == 2 ? "SLOW" : type == 3 ? "EVENT" : "SENSORS"));
        CHECK(values.at("gear").empty());
        CHECK(values.at("aero_node_state").empty());
        if (type == 1) {
            CHECK(values.at("rpm") == "7500");
            CHECK(values.at("gps_latitude_deg").empty());
        } else if (type == 2) {
            CHECK(values.at("lambda_a") == "1.001");
            CHECK(values.at("lambda_target").empty());
        } else if (type == 3) {
            CHECK(values.at("imu_node_state") == "2");
            CHECK(values.at("alert_flags_hex") == "0x0004");
        } else {
            CHECK(values.at("gps_latitude_deg") == "-0.0000001");
            CHECK(values.at("gps_longitude_deg") == "123.4567890");
            CHECK(values.at("fresh_mask_hex") == "0x0000");
            CHECK(values.at("gps_ground_speed_kph").empty());
        }
    }
    CHECK(rxPacketCount == 4);
    CHECK(Serial.lines.size() == 4);
    ++groups;
}

void testInvalidCrc()
{
    reset();
    radioBytes = sensorsFrame();
    radioBytes.back() ^= 0x40;
    receive();
    CHECK(rxActive);
    checkPacketOperationOrder();
    CHECK(rxPacketCount == 1); // Count denotes completed RF reads, including bad application CRC.
    CHECK(Serial.lines.size() == 1);
    const auto values = row(Serial.lines.front());
    CHECK(values.at("event") == "rx_error");
    CHECK(values.at("error_code") == "-5");
    CHECK(values.at("error_text") == "telemetry_crc_invalid");
    CHECK(values.at("lambda_a").empty());
    ++groups;
}

void testBusyTimeoutAndRearm()
{
    reset();
    startResult = LORA_API_BUSY;
    LoRaRxTaskPoll();
    CHECK(!rxActive);
    CHECK(pollCalls == 0);
    CHECK(Serial.lines.empty());
    startResult = RADIOLIB_ERR_NONE;
    LoRaRxTaskPoll();
    CHECK(rxActive);
    pollResult = LORA_API_BUSY;
    operations.clear();
    LoRaRxTaskPoll();
    CHECK(rxActive);
    CHECK(rxPacketCount == 0);
    CHECK(startCalls == 2);
    CHECK(Serial.lines.empty());
    CHECK(operations == std::vector<std::string>{"poll"});
    pollResult = RADIOLIB_ERR_RX_TIMEOUT;
    operations.clear();
    LoRaRxTaskPoll();
    CHECK(rxActive);
    CHECK(rxPacketCount == 0);
    CHECK(Serial.lines.empty());
    CHECK(startCalls == 3);
    CHECK(pollCalls == 2);
    CHECK(operations == (std::vector<std::string>{"poll", "start"}));
    pollResult = LORA_API_BUSY;
    LoRaRxTaskPoll();
    CHECK(startCalls == 3);
    CHECK(pollCalls == 3);
    CHECK(delays.empty());
    ++groups;
}

void testRadioErrors()
{
    for (int16_t state : {RADIOLIB_ERR_CRC_MISMATCH, static_cast<int16_t>(-99)}) {
        reset();
        pollResult = state;
        receive();
        CHECK(rxActive);
        checkPacketOperationOrder();
        CHECK(rxPacketCount == 0);
        CHECK(Serial.lines.size() == 1);
        const auto values = row(Serial.lines.front());
        CHECK(values.at("event") == "rx_error");
        CHECK(values.at("error_code") == std::to_string(state));
        CHECK(values.at("error_text") ==
              (state == RADIOLIB_ERR_CRC_MISMATCH ? "radio_crc_or_header_mismatch" : "receive_failed"));
        CHECK(values.at("schema_version").empty());
        pollResult = LORA_API_BUSY;
        LoRaRxTaskPoll();
        CHECK(rxActive);
    }
    ++groups;
}

void testStartFailureBackoff()
{
    reset();
    startResult = -12;
    LoRaRxTaskPoll();
    CHECK(!rxActive);
    CHECK(pollCalls == 0);
    CHECK(delays.size() == 1);
    CHECK(delays.front() == 100);
    CHECK(Serial.lines.size() == 1);
    const auto values = row(Serial.lines.front());
    CHECK(values.at("event") == "rx_error");
    CHECK(values.at("error_code") == "-12");
    CHECK(values.at("error_text") == "start_receive_failed");
    startResult = RADIOLIB_ERR_NONE;
    LoRaRxTaskPoll();
    CHECK(rxActive);
    CHECK(startCalls == 2);
    ++groups;
}

void testTaskInitializationAndHeader()
{
    reset();
    printCsvHeaderOnce();
    printCsvHeaderOnce();
    CHECK(Serial.lines.size() == 1);
    CHECK(Serial.lines.front() == telemetryCsvHeader());
    reset();
    stopAfterLoopDelay = true;
    try {
        LoRaTask(nullptr);
        CHECK(false);
    } catch (const StopTask&) {
    }
    CHECK(initCalls == 1);
    CHECK(!lastInitWasTx);
    CHECK(startCalls == 1);
    CHECK(rxActive);
    CHECK(lastTimeoutMs == 5000);
    CHECK(delays.size() == 1);
    CHECK(delays.front() == 1);
    CHECK(Serial.lines.size() == 1);
    CHECK(Serial.lines.front() == telemetryCsvHeader());
    printCsvHeaderOnce();
    CHECK(Serial.lines.size() == 1);
    ++groups;
}

void testRearmFailureRetainsCompletedPacket()
{
    for (int16_t nextStart : {static_cast<int16_t>(-12), LORA_API_BUSY}) {
        reset();
        radioBytes = sensorsFrame();
        LoRaRxTaskPoll();
        CHECK(rxActive);
        startResult = nextStart;
        operations.clear();
        LoRaRxTaskPoll();
        CHECK(!rxActive);
        CHECK(rxPacketCount == 1);
        CHECK(startCalls == 2);
        CHECK(Serial.lines.size() == (nextStart == LORA_API_BUSY ? 1u : 2u));
        const auto packet = row(Serial.lines.front());
        CHECK(packet.at("event") == "rx_packet");
        CHECK(packet.at("rx_ms") == "987654");
        CHECK(packet.at("rssi_dbm") == "-92.25");
        CHECK(packet.at("snr_db") == "7.50");
        CHECK(std::find(operations.begin(), operations.end(), "start") <
              std::find(operations.begin(), operations.end(), "print"));
        if (nextStart == LORA_API_BUSY) {
            CHECK(delays.empty());
        } else {
            CHECK(delays == std::vector<uint32_t>{100});
            const auto error = row(Serial.lines.back());
            CHECK(error.at("event") == "rx_error");
            CHECK(error.at("error_text") == "start_receive_failed");
            CHECK(error.at("error_code") == "-12");
        }
        const size_t lineCount = Serial.lines.size();
        startResult = RADIOLIB_ERR_NONE;
        LoRaRxTaskPoll();
        CHECK(rxActive);
        CHECK(startCalls == 3);
        CHECK(Serial.lines.size() == lineCount);
        CHECK(rxPacketCount == 1);
    }
    ++groups;
}

void testConsecutiveFastPackets()
{
    reset();
    LoRaRxTaskPoll();
    CHECK(rxActive);
    for (unsigned index = 0; index < 50; ++index) {
        std::vector<uint8_t> body(28, 0);
        put32(body, 0, index * 20);
        put16(body, 4, static_cast<uint16_t>(index));
        put16(body, 6, 0xFFFF);
        put16(body, 8, 0xFFFF);
        put16(body, 10, static_cast<uint16_t>(7000 + index));
        radioBytes = frame(3, 1, body);
        clockMs = 1000 + index * 20;
        radioRssi = -92.25f;
        radioSnr = 7.5f;
        operations.clear();
        LoRaRxTaskPoll();
        checkPacketOperationOrder();
        CHECK(rxActive);
        CHECK(startCalls == index + 2);
        CHECK(rxPacketCount == index + 1);
        CHECK(Serial.lines.size() == index + 1);
        const auto values = row(Serial.lines.back());
        CHECK(values.at("event") == "rx_packet");
        CHECK(values.at("packet_type") == "FAST");
        CHECK(values.at("seq") == std::to_string(index));
        CHECK(values.at("rpm") == std::to_string(7000 + index));
        CHECK(values.at("tx_ms") == std::to_string(index * 20));
        CHECK(values.at("rx_ms") == std::to_string(1000 + index * 20));
    }
    CHECK(delays.empty());
    ++groups;
}

} // namespace

int main()
{
    testSensors();
    testUnsupportedVersions();
    testIndependentPackets();
    testInvalidCrc();
    testBusyTimeoutAndRearm();
    testRadioErrors();
    testStartFailureBackoff();
    testTaskInitializationAndHeader();
    testRearmFailureRetainsCompletedPacket();
    testConsecutiveFastPackets();
    std::printf("LoRa task integration: %u checks in %u groups passed.\n", checks, groups);
    return 0;
}
