#ifndef RECEIVER_TEST_FIXTURES_H
#define RECEIVER_TEST_FIXTURES_H

#include "TestSupport.h"
#include "TelemetryReceiver.h"

#include <sstream>
#include <vector>

// Independent complete sender vectors, including little-endian CRC trailers.
// CRCs were checked with Python binascii.crc_hqx(frame_without_crc, 0xFFFF).
// V2 bytes are also asserted independently by the RTU sender native tests.
static const char* const V2_GOLDENS[] = {
    "54 4D 02 01 1C 78 56 34 12 CD AB FF 3F 55 15 "
    "01 11 02 11 03 11 FF FF 03 99 01 77 04 33 01 BB 5A A5 77 EF",
    "54 4D 02 02 20 78 56 34 12 CD AB FF 3F 55 15 "
    "85 FF 04 F7 03 F7 02 88 01 88 01 AA 03 AA 03 88 04 88 03 66 04 66 AC 2F",
    "54 4D 02 03 2E 78 56 34 12 CD AB FF 3F 55 15 "
    "0F 0E 5A A5 01 11 03 99 01 AA 04 F7 01 77 FF 7F 01 00 03 88 04 88 "
    "03 66 04 66 0C 00 0D 00 01 55 01 66 02 66 8F 34",
    "54 4D 02 04 3A 78 56 34 12 CD AB FF 3F 55 15 "
    "01 22 02 22 00 80 FF 7F 01 00 01 33 02 33 0C 00 03 22 0D 00 02 44 "
    "01 44 03 44 04 44 01 55 02 55 03 55 01 99 02 99 01 66 02 66 BF FE "
    "00 00 FF 7F 85 C0",
};

static const char* const V1_GOLDENS[] = {
    "54 4D 01 01 1E 78 56 34 12 CD AB 01 11 02 11 03 11 FF FF 85 FF "
    "03 99 01 AA 85 FF 01 77 04 33 85 FF 5A A5 31 0E",
    "54 4D 01 02 2E 78 56 34 12 CD AB 85 FF 86 FF 01 01 02 02 03 03 04 04 "
    "05 05 06 06 07 07 08 08 09 09 0A 0A 0B 0B 0C 0C 0D 0D 0E 0E 0F 0F "
    "10 10 11 11 12 12 C4 16",
    "54 4D 01 03 1C 78 56 34 12 CD AB 0F 0E 5A A5 01 11 03 99 01 AA "
    "85 FF 01 77 85 FF 03 88 04 88 03 66 13 CF",
};

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

inline TelemetryReceivedPacket decodeGolden(uint8_t version, uint8_t type)
{
    const std::vector<uint8_t> wire = hexBytes(version == 1 ? V1_GOLDENS[type - 1] : V2_GOLDENS[type - 1]);
    TelemetryReceivedPacket result = {};
    CHECK(decodeTelemetryRadioPayload(wire.data(), wire.size(), result) == TelemetryDecodeResult::Ok);
    return result;
}

#endif
