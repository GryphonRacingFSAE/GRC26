#ifndef TEST_ARDUINO_H
#define TEST_ARDUINO_H

#include <stdint.h>

uint32_t millis();

struct MockSerial {
    template <typename T> void print(const T&) {}
    template <typename T> void println(const T&) {}
};

extern MockSerial Serial;

#endif
