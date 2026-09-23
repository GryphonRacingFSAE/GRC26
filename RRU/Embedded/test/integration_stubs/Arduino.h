#pragma once

#include <stdint.h>
#include <string>
#include <vector>

void testSerialPrinted();

struct TestSerial {
    std::vector<std::string> lines;
    void println(const char* text)
    {
        testSerialPrinted();
        lines.emplace_back(text);
    }
};

extern TestSerial Serial;
uint32_t millis();
void vTaskDelay(uint32_t ticks);
#define pdMS_TO_TICKS(ms) (ms)
