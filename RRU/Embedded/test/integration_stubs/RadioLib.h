#pragma once

#include <stdint.h>

constexpr int16_t RADIOLIB_ERR_NONE = 0;
constexpr int16_t RADIOLIB_ERR_RX_TIMEOUT = -6;
constexpr int16_t RADIOLIB_ERR_CRC_MISMATCH = -7;
constexpr unsigned RADIOLIB_LR11X0_MAX_PACKET_LENGTH = 255;
