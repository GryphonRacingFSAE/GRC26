#ifndef LORA_API_H
#define LORA_API_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <RadioLib.h>

#define LORA_API_BUSY 1
#define LORA_API_ERR_INVALID_ARG -10000

int16_t LoRaApiInit(bool txRole);
bool LoRaApiIsBusy();

int16_t LoRaApiStartTransmit(const uint8_t* payload, size_t len);
int16_t LoRaApiStartTransmit(const char* payload);
int16_t LoRaApiPollTransmit();

int16_t LoRaApiStartReceive(uint32_t timeoutMs);
int16_t LoRaApiPollReceive(uint8_t* buffer, size_t bufferSize, size_t* receivedLen);
int16_t LoRaApiPollReceive(String& received);

float LoRaApiGetRSSI();
float LoRaApiGetSNR();

#endif // LORA_API_H
