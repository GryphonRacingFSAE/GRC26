#ifndef PIN_DEFS_H
#define PIN_DEFS_H

#include <stdint.h>
static constexpr int8_t LED_PIN = 4;

static constexpr int8_t LORA_CS   = 16;
static constexpr int8_t LORA_BUSY = 35;
static constexpr int8_t LORA_RST  = 36;

static constexpr int8_t SPI_MOSI = 11;
static constexpr int8_t SPI_CLK  = 12;
static constexpr int8_t SPI_MISO = 13;

#endif // PIN_DEFS_H