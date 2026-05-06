#pragma once

#include <Arduino.h>
#include <RadioLib.h>

/// @brief Initialize the LoRa API
/// @param txRole Boolean indicating whether this device is a transmitter (true) or receiver (false)
/// @return Error code
int16_t LoRaApiInit(bool txRole);

/// @brief Transmit a LoRa packet with the given payload
/// @param payload string to transmit
/// @return Error code
int16_t LoRaApiTransmit(const char* payload);

/// @brief Receive a LoRa packet, blocking until one is received or timeout occurs
/// @param received reference to a String that will be filled with the received payload
/// @param timeoutMs maximum time to wait for a packet, in milliseconds
/// @return Error code
///@warning This function blocks until a packet is received or timeout occurs. To be converted to non-blocking
int16_t LoRaApiReceive(String& received, uint32_t timeoutMs);

/// @brief get RSSI of the last received packet
/// @return RSSI in dBm
float LoRaApiGetRSSI();

/// @brief get SNR of the last received packet
/// @return SNR in dB
float LoRaApiGetSNR();