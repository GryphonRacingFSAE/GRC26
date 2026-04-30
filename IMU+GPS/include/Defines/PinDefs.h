#ifndef PIN_DEFS_H
#define PIN_DEFS_H

#include <stdint.h>

static constexpr int8_t UART0_TX = 43;
static constexpr int8_t UART0_RX = 44;

static constexpr int8_t LED_ONBOARD = 4;

static constexpr int8_t SPI_MOSI = 11;
static constexpr int8_t SPI_SCK = 12;
static constexpr int8_t SPI_MISO = 13;
static constexpr uint8_t IMU_CS = 15;

static constexpr int8_t CAN_RX = 47;
static constexpr int8_t CAN_TX = 48;

#endif // PIN_DEFS_H
