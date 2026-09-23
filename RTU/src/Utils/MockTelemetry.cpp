#include "MockTelemetry.h"

#if TELEMETRY_MOCK_DATA
namespace
{
void decodeMockFrame(EcuTelemetryState& state, uint32_t nowMs, uint32_t id,
                     uint16_t a, uint16_t b = 0, uint16_t c = 0, uint16_t d = 0)
{
    const uint16_t words[] = {a, b, c, d};
    uint8_t data[8] = {};
    for (size_t i = 0; i < 4; ++i) {
        data[2 * i] = static_cast<uint8_t>(words[i]);
        data[2 * i + 1] = static_cast<uint8_t>(words[i] >> 8);
    }
    // Bytes pass through the real decoder locally, never onto the vehicle CAN bus.
    decodeEcuCanFrame(state, id, data, sizeof(data), nowMs);
}
} // namespace

void updateMockTelemetry(EcuTelemetryState& state, uint32_t nowMs)
{
    const uint32_t seconds = nowMs / 1000u;
    const uint16_t step = static_cast<uint16_t>(seconds % 60u);
    const bool braking = (seconds / 5u) % 2u != 0;
    const uint16_t status = braking ? ECU_STATUS_BRAKE_PEDAL_ACTIVE : 0;
    const uint16_t sequence = static_cast<uint8_t>(nowMs / TELEMETRY_FAST_PERIOD_MS);

    decodeMockFrame(state, nowMs, 0x520, 1000 + 100 * step, 10 * step, 0, 950 + step);
    decodeMockFrame(state, nowMs, 0x521, 940 + step, 960 + step);
    decodeMockFrame(state, nowMs, 0x522, 200 + step, 100 + 10 * step, 0, 10 * step);
    decodeMockFrame(state, nowMs, 0x526, status, 0, 12000);
    decodeMockFrame(state, nowMs, 0x527, 0, 0, 0, 1000);
    decodeMockFrame(state, nowMs, 0x530, 1300 + step, 0, 250 + step, 800 + step);
    decodeMockFrame(state, nowMs, 0x536, 1 + step / 10);
    decodeMockFrame(state, nowMs, 0x538, 100 + step);
    decodeMockFrame(state, nowMs, 0x600, static_cast<uint16_t>(-100 - step), 200 + step);
    decodeMockFrame(state, nowMs, 0x601, 2500 + step, 10132);
    decodeMockFrame(state, nowMs, 0x602, 0x0301, 0, sequence);
    decodeMockFrame(state, nowMs, 0x610, static_cast<uint16_t>(static_cast<int32_t>(step) * 10 - 300),
                    static_cast<uint16_t>(300 - static_cast<int32_t>(step) * 10), 1000 + step);
    decodeMockFrame(state, nowMs, 0x611, static_cast<uint16_t>(-500 + step), 100 + step, 200 + step);
    decodeMockFrame(state, nowMs, 0x612, 0x0301, 0, sequence);
    const uint32_t latitude = static_cast<uint32_t>(430000000 + step * 100);
    const uint32_t longitude = static_cast<uint32_t>(-790000000 + static_cast<int32_t>(step) * 100);
    decodeMockFrame(state, nowMs, 0x620, static_cast<uint16_t>(latitude), static_cast<uint16_t>(latitude >> 16),
                    static_cast<uint16_t>(longitude), static_cast<uint16_t>(longitude >> 16));
    decodeMockFrame(state, nowMs, 0x621, 100 * step, 9000 + step);
}
#endif
