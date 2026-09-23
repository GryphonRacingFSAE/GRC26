#pragma once

#include <stdint.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using TickType_t = uint32_t;
using QueueHandle_t = void*;
using BaseType_t = int;
constexpr BaseType_t pdTRUE = 1;
constexpr TickType_t portMAX_DELAY = UINT32_MAX;
constexpr int HEX = 16;

class String : public std::string
{
  public:
    explicit String(int value) : std::string(std::to_string(value)) {}
};
inline std::string operator+(const char* prefix, const String& value)
{
    return std::string(prefix) + static_cast<const std::string&>(value);
}

struct TestSerial {
    std::vector<std::string> lines;
    std::string pending;
    void print(const char* value)
    {
        pending += value;
    }
    template <typename T> void print(T value)
    {
        std::ostringstream out;
        out << +value;
        pending += out.str();
    }
    template <typename T> void println(T value)
    {
        print(value);
        lines.push_back(pending);
        pending.clear();
    }
    template <typename T> void print(T value, int base)
    {
        std::ostringstream out;
        if (base == HEX) {
            out << std::hex;
        }
        out << +value;
        pending += out.str();
    }
    template <typename T> void println(T value, int base)
    {
        print(value, base);
        lines.push_back(pending);
        pending.clear();
    }
};

extern TestSerial Serial;
uint32_t millis();
TickType_t testMillisecondsToTicks(uint32_t milliseconds);
void vTaskDelay(TickType_t ticks);
BaseType_t xQueueReceive(QueueHandle_t queue, void* packet, TickType_t wait);
#define pdMS_TO_TICKS(ms) testMillisecondsToTicks(ms)
#define configASSERT(condition)                                                                                        \
    do {                                                                                                               \
        if (!(condition))                                                                                              \
            throw std::runtime_error(#condition);                                                                      \
    } while (false)
