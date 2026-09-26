#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr int16_t LORA_API_BUSY = 1;
int16_t LoRaApiInit(bool txRole);
int16_t LoRaApiStartReceive(uint32_t timeoutMs);
int16_t LoRaApiPollReceive(uint8_t* buffer, size_t bufferSize, size_t* receivedLen);
float LoRaApiGetRSSI();
float LoRaApiGetSNR();
