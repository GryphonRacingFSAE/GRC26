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
    return clockMs;
}

void vTaskDelay(uint32_t ticks)
{
    delays.push_back(ticks);
    if (stopAfterLoopDelay && ticks == 5) {
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
    ++startCalls;
    lastTimeoutMs = timeoutMs;
    return startResult;
}

int16_t LoRaApiPollReceive(uint8_t* buffer, size_t bufferSize, size_t* receivedLen)
{
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
    return -92.25f;
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
    rxActive = false;
    rxPacketCount = 0;
    csvHeaderPrinted = false;
    std::memset(csvBuffer, 0xA5, sizeof(csvBuffer));
    Serial.lines.clear();
    delays.clear();
    radioBytes.clear();
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

std::vector<uint8_t> powertrainFrame()
{
    std::vector<uint8_t> body(58, 0);
    put32(body, 0, 123456);
    put16(body, 4, 42);
    put16(body, 6, 0x3FFF);
    put16(body, 8, 0x3FFF);
    put16(body, 10, 1234);
    put16(body, 14, static_cast<uint16_t>(-1000));
    put32(body, 16, static_cast<uint32_t>(-50000));
    put16(body, 20, 1234);
    put16(body, 44, 65535);
    put16(body, 52, static_cast<uint16_t>(-1000));
    return frame(2, 4, body);
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
    CHECK(names.size() == 75);
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
    LoRaRxTaskPoll();
}

void testPowertrain()
{
    reset();
    radioBytes = powertrainFrame();
    receive();
    CHECK(!rxActive);
    CHECK(rxPacketCount == 1);
    CHECK(Serial.lines.size() == 1);
    const auto values = row(Serial.lines.front());
    CHECK(values.at("event") == "rx_packet");
    CHECK(values.at("packet_type") == "POWERTRAIN");
    CHECK(values.at("schema_version") == "2");
    CHECK(values.at("rx_count") == "1");
    CHECK(values.at("rx_ms") == "987654");
    CHECK(values.at("rssi_dbm") == "-92.25");
    CHECK(values.at("snr_db") == "7.50");
    CHECK(values.at("radio_len") == "65");
    CHECK(values.at("seq") == "42");
    CHECK(values.at("tx_ms") == "123456");
    CHECK(values.at("received_mask_hex") == "0x3FFF");
    CHECK(values.at("fresh_mask_hex") == "0x3FFF");
    CHECK(values.at("lambda_a") == "1.234");
    CHECK(values.at("lambda_target") == "-1.000");
    CHECK(values.at("lambda_error") == "-50.000");
    CHECK(values.at("fuel_inj_pulse_width_ms") == "12.34");
    CHECK(values.at("gear") == "65535");
    CHECK(values.at("acceleration_x_g") == "-1.000");
    CHECK(values.at("rpm").empty());
    ++groups;
}

void testLegacy()
{
    reset();
    std::vector<uint8_t> body(30, 0);
    put32(body, 0, 2000);
    put16(body, 4, 7);
    put16(body, 6, 7500);
    put16(body, 14, static_cast<uint16_t>(-1234));
    put16(body, 20, static_cast<uint16_t>(-500));
    put16(body, 26, static_cast<uint16_t>(-1));
    put16(body, 28, 0x0012);
    radioBytes = frame(1, 1, body);
    receive();
    CHECK(Serial.lines.size() == 1);
    const auto values = row(Serial.lines.front());
    CHECK(values.at("event") == "rx_packet");
    CHECK(values.at("packet_type") == "FAST");
    CHECK(values.at("schema_version") == "1");
    CHECK(values.at("rpm") == "7500");
    CHECK(values.at("lambda_error") == "-1.234");
    CHECK(values.at("coolant_temp_c") == "-50.0");
    CHECK(values.at("gear") == "-1");
    CHECK(values.at("status_bits_hex") == "0x0012");
    CHECK(values.at("received_mask_hex").empty());
    CHECK(values.at("rev_limit_active").empty());
    ++groups;
}

void testInvalidCrc()
{
    reset();
    radioBytes = powertrainFrame();
    radioBytes.back() ^= 0x40;
    receive();
    CHECK(!rxActive);
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
    LoRaRxTaskPoll();
    CHECK(rxActive);
    CHECK(rxPacketCount == 0);
    CHECK(startCalls == 2);
    CHECK(Serial.lines.empty());
    pollResult = RADIOLIB_ERR_RX_TIMEOUT;
    LoRaRxTaskPoll();
    CHECK(!rxActive);
    CHECK(rxPacketCount == 0);
    CHECK(Serial.lines.empty());
    LoRaRxTaskPoll();
    CHECK(rxActive);
    CHECK(startCalls == 3);
    CHECK(pollCalls == 2);
    CHECK(delays.empty());
    ++groups;
}

void testRadioErrors()
{
    for (int16_t state : {RADIOLIB_ERR_CRC_MISMATCH, static_cast<int16_t>(-99)}) {
        reset();
        pollResult = state;
        receive();
        CHECK(!rxActive);
        CHECK(rxPacketCount == 0);
        CHECK(Serial.lines.size() == 1);
        const auto values = row(Serial.lines.front());
        CHECK(values.at("event") == "rx_error");
        CHECK(values.at("error_code") == std::to_string(state));
        CHECK(values.at("error_text") ==
              (state == RADIOLIB_ERR_CRC_MISMATCH ? "radio_crc_or_header_mismatch" : "receive_failed"));
        CHECK(values.at("schema_version").empty());
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
    CHECK(delays.front() == 5);
    CHECK(Serial.lines.size() == 1);
    CHECK(Serial.lines.front() == telemetryCsvHeader());
    printCsvHeaderOnce();
    CHECK(Serial.lines.size() == 1);
    ++groups;
}

} // namespace

int main()
{
    testPowertrain();
    testLegacy();
    testInvalidCrc();
    testBusyTimeoutAndRearm();
    testRadioErrors();
    testStartFailureBackoff();
    testTaskInitializationAndHeader();
    std::printf("LoRa task integration: %u checks in %u groups passed.\n", checks, groups);
    return 0;
}
