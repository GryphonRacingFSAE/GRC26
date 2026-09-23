#ifndef RECEIVER_TEST_FIXTURES_H
#define RECEIVER_TEST_FIXTURES_H

#include "TestSupport.h"
#include "TelemetryReceiver.h"

#include <sstream>
#include <vector>

// Independent vectors authored from the protocol offsets using Python struct.pack;
// binascii.crc_hqx supplies independent CRCs. Type 4 is deliberately unsupported.
static const char* const GOLDENS[] = {
    "54 4D 03 01 1C 78 56 34 12 CD AB FF FF 55 A5 "
    "01 11 02 11 FF FF 04 33 80 03 28 23 FF FF 01 BB 01 77 8C 7B",
    "54 4D 03 02 18 78 56 34 12 CD AB FF FF 55 A5 01 22 02 22 00 80 01 33 02 33 03 F7 04 F7 06 34",
    "54 4D 03 03 1A 78 56 34 12 CD AB FF FF 55 A5 "
    "07 00 80 03 01 11 03 A5 01 80 FF 02 5A FF FF 80 81 99",
    nullptr,
    "54 4D 03 05 34 78 56 34 12 CD AB FF FF 55 A5 "
    "00 80 FF 7F 2E FB FF FF 03 A5 01 80 FF 00 80 00 00 FF 7F 00 80 "
    "FF FF FF 7F 02 5A FF FF 80 00 00 00 80 FF FF FF 7F FF FF 9F 8C 36 AB",
};

inline const char* golden(uint8_t type)
{
    CHECK(type >= 1 && type <= 5);
    const char* result = GOLDENS[type - 1];
    CHECK(result != nullptr);
    return result;
}

inline std::vector<uint8_t> hexBytes(const char* hex)
{
    std::istringstream input(hex);
    std::vector<uint8_t> result;
    unsigned byte = 0;
    while (input >> std::hex >> byte) {
        CHECK(byte <= 255);
        result.push_back(static_cast<uint8_t>(byte));
    }
    return result;
}

inline TelemetryReceivedPacket decodeGolden(uint8_t type)
{
    const std::vector<uint8_t> wire = hexBytes(golden(type));
    TelemetryReceivedPacket result = {};
    CHECK(decodeTelemetryRadioPayload(wire.data(), wire.size(), result) == TelemetryDecodeResult::Ok);
    return result;
}

#endif
