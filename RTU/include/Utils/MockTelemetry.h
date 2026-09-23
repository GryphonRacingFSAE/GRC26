#ifndef MOCK_TELEMETRY_H
#define MOCK_TELEMETRY_H

#include "EcuTelemetry.h"
#include "Telemetry.h"

#if TELEMETRY_MOCK_DATA
// Generate all 16 ECU, AeroProbe, IMU, and GPS sources through the normal CAN decoder.
// Measurements change once per second; receipt timestamps refresh on every call.
void updateMockTelemetry(EcuTelemetryState& state, uint32_t nowMs);
#endif

#endif
