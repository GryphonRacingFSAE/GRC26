#ifndef PIN_DEFS_H
#define PIN_DEFS_H

#include <stdint.h>

// User Configurable Pins
static constexpr int8_t LED_PIN = 4;
static constexpr int8_t GPIO_5  = 5;
static constexpr int8_t GPIO_6  = 6;
static constexpr int8_t GPIO_7  = 7;

/// @warning SPI line to be used for LoRa and SD card. Ensure that CS is active accordingly
// SPI Pins
static constexpr int8_t SPI_MOSI = 11;
static constexpr int8_t SPI_CLK  = 12;
static constexpr int8_t SPI_MISO = 13;

// SD Card Pins
static constexpr int8_t SD_CS = 14;

// LoRa Pins
static constexpr int8_t LORA_CS   = 16;
static constexpr int8_t LORA_RST  = 17;
static constexpr int8_t LORA_BUSY = 18;

// CAN Pins
static constexpr int8_t CAN_TX = 47;
static constexpr int8_t CAN_RX = 48;

#endif // PIN_DEFS_H