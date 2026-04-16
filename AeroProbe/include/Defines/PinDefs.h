#ifndef PIN_DEFS_H
#define PIN_DEFS_H

#include <stdint.h>

static constexpr int8_t LED_ONBOARD = 4;
static constexpr int8_t SPI_MOSI = 11;
static constexpr int8_t SPI_SCK = 12;
static constexpr int8_t SPI_MISO = 13;

static constexpr int8_t DLVR1_CS = 15;
static constexpr int8_t DLVR2_CS = 16;
static constexpr int8_t BME680_CS = 18;

static constexpr int8_t CAN_RX = 47;
static constexpr int8_t CAN_TX = 48;

#endif // PIN_DEFS_H