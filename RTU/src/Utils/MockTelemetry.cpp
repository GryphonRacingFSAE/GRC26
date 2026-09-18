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
    // These bytes are decoded locally, never transmitted onto the vehicle CAN bus.
    decodeEcuCanFrame(state, id, data, sizeof(data), nowMs);
}

} // namespace

void updateMockTelemetry(EcuTelemetryState& state, uint32_t nowMs)
{
    const uint32_t seconds = nowMs / 1000u;
    const uint16_t step = static_cast<uint16_t>(seconds % 60u);
    const bool braking = (seconds / 5u) % 2u != 0;
    const uint16_t status = ECU_STATUS_ECU_IS_LOGGING | (braking ? ECU_STATUS_BRAKE_PEDAL_ACTIVE : 0);

    // Use the real DBC word positions and fixed-point scales. The 60-second ramp
    // makes changes easy to recognize on the receiver while keeping values bounded.
    decodeMockFrame(state, nowMs, 0x520, 1000 + 100 * step, 10 * step, 1000 + 10 * step, 950 + step);
    decodeMockFrame(state, nowMs, 0x521, 940 + step, 960 + step, 100 + step, braking ? 10 : 0);
    decodeMockFrame(state, nowMs, 0x522, 200 + step, 100 + 10 * step, braking ? 5 : 0, 10 * step);
    decodeMockFrame(state, nowMs, 0x523, 10 * step, 11 * step, step, 50 + step);
    decodeMockFrame(state, nowMs, 0x524, braking ? 100 : 0, 1000 + step, 1000 - step);
    decodeMockFrame(state, nowMs, 0x526, status);
    decodeMockFrame(state, nowMs, 0x527, 0, 0, 0, 1000);
    decodeMockFrame(state, nowMs, 0x528, 100 + step, step, static_cast<uint16_t>(seconds / 5u), 1 + step % 4);
    decodeMockFrame(state, nowMs, 0x530, 1300 + step, 1000 + step, 250 + step, 800 + step);
    decodeMockFrame(state, nowMs, 0x534, 20 + step, 40 + step, static_cast<uint16_t>(seconds / 10u),
                    static_cast<uint16_t>(seconds / 15u));
    decodeMockFrame(state, nowMs, 0x536, 1 + step / 10, 10 * step, 3000 + 10 * step, 850 + step);
    decodeMockFrame(state, nowMs, 0x537, 4000 + 10 * step, 0, 1000 + step);
    decodeMockFrame(state, nowMs, 0x538, braking ? 1000 + 10 * step : 0);
    // Signed acceleration sweeps through zero; the cast preserves CAN two's-complement words.
    decodeMockFrame(state, nowMs, 0x600, static_cast<uint16_t>(static_cast<int32_t>(step) * 10 - 300),
                    static_cast<uint16_t>(300 - static_cast<int32_t>(step) * 10), 1000 + step);
}
#endif
