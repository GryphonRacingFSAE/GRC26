# Sender telemetry schema version 2

This document specifies the expanded MAXXECU sender schema. `RRU/Embedded` accepts
both version 1 and version 2 and prints packet measurements as CSV. The Sentio
parser and desktop app still require separate updates for the expanded schema.

The supplied `MAXXECU RAW DBC.dbc` defines MAXXECU layout, signs and scaling. The
intentional exception is `0x538` bytes 0–1, configured as brake pressure in kPa × 10.
The existing `0x600` IMU mapping remains signed X/Y/Z acceleration in milligravity.
No ECU configuration is changed and the acquisition task does not send CAN frames.

## Over-the-air bench test

In `include/Utils/Telemetry.h`, set `TELEMETRY_MOCK_DATA` to `1` before building
and flashing the RTU transmitter. Leave `LORA_ROLE_TX` set to `1`. The default
`TELEMETRY_MOCK_DATA 0` uses real CAN input; restore it to `0` and reflash before
testing with the car. The define can also be supplied as `-DTELEMETRY_MOCK_DATA=1`
in PlatformIO `build_flags` (a build flag overrides the header default).

```powershell
pio run -e esp32-s3-devboard -t upload
pio device monitor -b 115200
```

With mock mode enabled, CAN is not initialized or read and no ECU connection is
needed. Serial prints `[CAN][MOCK] Enabled: generated telemetry -> LoRa; CAN disabled`
at startup. The real LoRa task transmits generated data using the same V2 packets,
fixed-point units, sequence numbers, CRC, queue, radio settings, and packet rates
as real input. Use the normal V2-capable RRU receiver with matching radio settings;
the legacy V1 RX path in this RTU project cannot decode these packets.

All 14 supported ECU/IMU sources are generated through the normal decoder and
remain fresh (`received_mask` and `fresh_mask` are `0x3FFF`). Values change once
per second: RPM ramps from 1,000 to 6,900 in steps of 100, while vehicle speed
ramps from 0 to 59 km/h in steps of 1. Both repeat every 60 seconds of uptime.
Temperatures, pressures, throttle, gear, and acceleration also vary. Brake/cut
states toggle and the knock counter increments every 5 seconds; error and lost-sync
counters increment every 10 and 15 seconds. These exercise event packets as well
as fast (20 ms), slow (2 seconds), and powertrain (6 seconds) packets.

Mock frames deliberately have no special wire marker: the receiver handles them
exactly like real telemetry. The startup message identifies the selected mode.

## Packet envelope and compatibility

Every radio frame has the same framing and CRC algorithm as version 1:

| Radio byte offset | Length | Meaning |
| --- | --- | --- |
| 0 | 2 | ASCII `TM`, bytes `54 4D` hexadecimal |
| 2 | 1 | Schema version: `02` |
| 3 | 1 | Packet type: fast `1`, slow `2`, event `3`, powertrain `4` |
| 4 | 1 | Body length in bytes, excluding the envelope and CRC |
| 5 | body length | Body defined below |
| 5 + body length | 2 | CRC16, least significant byte first |

CRC16-CCITT uses polynomial `0x1021`, initial value `0xFFFF`, no reflection and no
final XOR. It covers all five header bytes and the complete body, excluding the
CRC itself. Total frame length must equal `body_length + 7`.

All multibyte integers are little-endian, with no padding. Signed fields use two's
complement. No native C++ booleans or C++ bitfields appear on the wire. Version 2
changes field order, types and packet membership; it is not an append-only version
1 extension. Parse by the `(version, type, body_length)` tuple. The original version
1 structs and receiver path are retained in the firmware source for the existing
receiver; their layouts do not describe version 2 sender packets.

## Cadence and transport budget

The core fast packet is generated every 20 ms (50 Hz). Slow packets remain at
2,000 ms and powertrain packets at 6,000 ms. Cut transitions and knock-count
increments also appear in event packets, so these events do not wait for a
powertrain snapshot. No requested field, including boost solenoid duty, is omitted.

| Version 2 packet | Type | Body bytes | Complete frame bytes | Nominal enqueue cadence | Estimated airtime/frame |
| --- | --- | --- | --- | --- | --- |
| Fast | 1 | 28 | 35 | 20 ms / 50 Hz | 11.680 ms |
| Slow | 2 | 32 | 39 | 2,000 ms / 0.5 Hz | 12.320 ms |
| Event | 3 | 46 | 53 | On qualifying transitions | 15.520 ms |
| Powertrain | 4 | 58 | 65 | 6,000 ms / 1/6 Hz | 18.080 ms |

Both RTU and RRU use 915 MHz, SF6, 500 kHz bandwidth, coding rate 4/5, 12 preamble
symbols, private sync word, explicit header, PHY CRC enabled and low-data-rate
optimization disabled. Transmit power remains 14 dBm. The 12-symbol preamble follows
the LR1121 recommendation for SF5/SF6. Estimates follow the installed RadioLib
airtime calculation:

```text
airtime_ms = 0.128 * (26.25 + 5 * ceil((8 * complete_frame_bytes + 12) / 24))
```

Periodic traffic consumes approximately 593.173 ms of RF airtime per second
(59.317%). A 3 ms gap after each completed transmission gives the polled receiver
time to rearm when packets are queued back to back. Together, airtime and these
gaps consume about 74.517% of a second, excluding polling, setup, USB output and
events. Periodic wire traffic is approximately 1,780.333 bytes/s. Successful TX
diagnostics are limited to one report per second; errors are still reported.

The previous SF9 / 125 kHz / 4/7 / eight-symbol profile took 312.320 ms per FAST
packet and supported the earlier 2 Hz split, not 50 Hz. **Both boards must run the
new radio settings together**; schema compatibility alone does not make old radio
settings interoperable. Packet layouts and the 75-column receiver CSV are unchanged.

The fixed application radio buffer remains 96 bytes, leaving 89 bytes for a body.
The largest version 2 frame is 65 bytes. The shared queue still contains 24 entries,
is nonblocking on enqueue, preserves FIFO order and drops the newest packet when
full. A queue item is now 59 bytes, so item storage rises from 1,128 to 1,416 bytes
(288 additional bytes, excluding FreeRTOS queue bookkeeping). No retransmission,
priority queue or packet coalescing is added.

Scheduling starts after the first successfully decoded CAN frame. Bounded CAN
receive waits let periodic deadlines run even without another valid frame. Deadlines
retain their phase; if execution is late, one current snapshot is sent and missed
slots are skipped rather than replayed. After CAN stops, snapshots continue with
cached measurements and aging freshness masks. A 50 Hz snapshot rate does not
prove every ECU signal updates at 50 Hz; source CAN rates still determine that.

The receiver emits one row per received packet, nominally 50 FAST rows/s plus
0.5 SLOW rows/s, 1/6 POWERTRAIN rows/s and events. It polls every RTOS tick (1 ms
in this build), captures packet metadata, then rearms before formatting and printing
CSV. Coincident packets, events, RTOS scheduling, serial backpressure and losses
cause arrival jitter; rows are not guaranteed exactly 20 ms apart. Sustained event
bursts can fill the bounded FIFO and cause drop-newest behavior. There is no
receiver timer duplicating old rows to manufacture a 50 Hz output rate.

The design target is 1 km. Clear line-of-sight operation is plausible, but no range
or sustained-rate measurement has been made on these boards. Higher bandwidth and
lower spreading factor trade sensitivity for throughput; antenna placement, vehicle
shielding, obstructions and fading determine the actual margin. See the
[Semtech LR1121 user manual, section 8.2.2.1](https://storage.googleapis.com/cloud-storage-web/public/t_form3/rzsyE6ulSlqjnJrpxSmZqD5It5wRyeCyyxP6I2Y3.pdf)
for preamble settings and [Semtech's link-budget explanation](https://blog.semtech.com/long-range-with-lora)
for the sensitivity tradeoff. Validate at the actual course with both boards:
count FAST arrivals over sustained windows, check sequence gaps and `tx_ms`
spacing, inspect RSSI/SNR across the full 1 km, and include expected event traffic.
These are calculated budgets, not a guarantee against packet loss.

## Common body header and validity

Every version 2 body begins with this ten-byte header. All offsets in the remaining
tables are relative to the beginning of the body; add five for radio frame offsets.

| Body offset | Type | Field | Meaning |
| --- | --- | --- | --- |
| 0 | u32 | `ms` | Sender uptime in milliseconds when the snapshot is built |
| 4 | u16 | `seq` | Shared packet sequence number, wrapping at 65,536 |
| 6 | u16 | `received_mask` | Sources with a successfully decoded frame since boot |
| 8 | u16 | `fresh_mask` | Received sources whose most recent frame is at most 2,000 ms old |

| Bit | Source CAN ID | Fields governed by this source |
| --- | --- | --- |
| 0 | `0x520` | RPM, TPS, MAP, lambda average |
| 1 | `0x521` | Lambda A/B, ignition timing and ignition cut |
| 2 | `0x522` | Injector pulse width/duty, fuel cut, vehicle speed |
| 3 | `0x523` | Driven/non-driven wheel speeds, measured/target traction slip |
| 4 | `0x524` | Traction cut request, lambda corrections A/B |
| 5 | `0x526` | Raw status and its named state flags |
| 6 | `0x527` | Lambda target |
| 7 | `0x528` | Knock peak, correction, count and last cylinder |
| 8 | `0x530` | Battery voltage, coolant temperature, intake air temperature |
| 9 | `0x534` | EGT delta, ECU temperature, ECU error count, lost-sync count |
| 10 | `0x536` | Gear, boost solenoid duty, oil pressure, oil temperature |
| 11 | `0x537` | Fuel pressure, coolant pressure |
| 12 | `0x538` | Intentionally configured brake pressure |
| 13 | `0x600` | IMU acceleration X/Y/Z |

Bits 14–15 are reserved. A missing source is not a valid zero: the receiver must
require its `received_mask` bit before treating a numeric field as a measurement.
A received but stale field retains its last numeric value and has its `fresh_mask`
bit cleared. A valid physical zero has both source bits set. Neither mask proves
that a sensor is configured within the ECU; it proves CAN source receipt only.
Only standard, non-remote CAN data frames with DLC exactly 8 are accepted. Rejected
frames leave values, receipt masks and source timestamps unchanged.

The derived `lambda_error_x1000` requires both source bits 0 and 6 to be received
and fresh. It equals the current lambda average minus the current lambda target,
without saturation. The signed 32-bit representation covers the complete DBC
input range. Do not infer validity from the error's numeric value alone.

Freshness describes the snapshot at `ms`. The receiver must additionally monitor
packet arrival age, including radio loss or a sender that has stopped. CAN silence
now produces cached snapshots whose freshness bits clear after 2,000 ms. It must
not indefinitely display the last packet's `fresh_mask` as proof that data remains
live. A 6-second powertrain
period also means a fresh-at-snapshot value can age before the next snapshot.
Queueing and transmission can add delay before arrival; do not label the receive
time as the acquisition time. The protocol carries sender uptime, not a
synchronized wall-clock timestamp or an exact per-signal sample timestamp.

Retain snapshot time and validity separately for each stored packet/value. Every
packet's masks describe all CAN sources at that packet's snapshot time, but only
the measurements included in that packet are updated. A newer fast packet's fresh
`0x521` or `0x523` bit does not refresh lambda A/B or wheel-speed values retained
from an older powertrain packet. Those values retain the older snapshot time and
validity until a packet containing them arrives. Likewise, an event updates only
the measurements actually present in its event body.

## Fast packet, type 1, 28-byte body

| Body offset | Type | Field | Engineering conversion | Source |
| --- | --- | --- | --- | --- |
| 10 | u16 | `rpm` | raw RPM | `0x520` |
| 12 | u16 | `tps_x10` | raw / 10 percent | `0x520` |
| 14 | u16 | `map_kpa_x10` | raw / 10 kPa | `0x520` |
| 16 | u16 | `lambda_avg_x1000` | raw / 1,000 lambda | `0x520` |
| 18 | u16 | `oil_pressure_kpa_x10` | raw / 10 kPa | `0x536` |
| 20 | u16 | `battery_v_x100` | raw / 100 V | `0x530` |
| 22 | u16 | `vehicle_speed_kph_x10` | raw / 10 km/h | `0x522` |
| 24 | u16 | `brake_pressure_kpa_x10` | raw / 10 kPa | configured `0x538` |
| 26 | u16 | `status_bits` | Raw status word; bit meanings below | `0x526` |

## Powertrain packet, type 4, 58-byte body

| Body offset | Type | Field | Engineering conversion | Source |
| --- | --- | --- | --- | --- |
| 10 | u16 | `lambda_a_x1000` | raw / 1,000 lambda | `0x521` |
| 12 | u16 | `lambda_b_x1000` | raw / 1,000 lambda | `0x521` |
| 14 | i16 | `lambda_target_x1000` | raw / 1,000 lambda | `0x527` |
| 16 | i32 | `lambda_error_x1000` | raw / 1,000 lambda | `0x520` and `0x527` |
| 20 | u16 | `fuel_inj_pulse_width_ms_x100` | raw / 100 ms | `0x522` |
| 22 | u16 | `fuel_inj_duty_x10` | raw / 10 percent; may exceed 100% | `0x522` |
| 24 | u16 | `fuel_cut_percent` | raw percent | `0x522` |
| 26 | u16 | `ignition_timing_deg_x10` | raw / 10 degrees | `0x521` |
| 28 | u16 | `ignition_cut_percent` | raw percent | `0x521` |
| 30 | u16 | `driven_wheel_speed_kph_x10` | raw / 10 km/h | `0x523` |
| 32 | u16 | `non_driven_wheel_speed_kph_x10` | raw / 10 km/h | `0x523` |
| 34 | u16 | `traction_slip_measured_x10` | raw / 10 percent | `0x523` |
| 36 | u16 | `traction_slip_target_x10` | raw / 10 percent | `0x523` |
| 38 | u16 | `traction_cut_request_x10` | raw / 10 percent | `0x524` |
| 40 | u16 | `lambda_corr_a_x10` | raw / 10 percent | `0x524` |
| 42 | u16 | `lambda_corr_b_x10` | raw / 10 percent | `0x524` |
| 44 | u16 | `gear` | Raw gear position; no invented sentinel meanings | `0x536` |
| 46 | u16 | `boost_solenoid_duty_x10` | raw / 10 percent | `0x536` |
| 48 | u16 | `knock_level_peak` | Raw value; DBC specifies no physical unit | `0x528` |
| 50 | u16 | `knock_correction_deg_x10` | raw / 10 degrees | `0x528` |
| 52 | i16 | `acceleration_x_mg` | raw / 1,000 g | existing `0x600` |
| 54 | i16 | `acceleration_y_mg` | raw / 1,000 g | existing `0x600` |
| 56 | i16 | `acceleration_z_mg` | raw / 1,000 g | existing `0x600` |

## Slow packet, type 2, 32-byte body

| Body offset | Type | Field | Engineering conversion | Source |
| --- | --- | --- | --- | --- |
| 10 | i16 | `oil_temp_c_x10` | raw / 10 °C | `0x536` |
| 12 | u16 | `coolant_temp_c_x10` | raw / 10 °C | `0x530` |
| 14 | u16 | `intake_air_temp_c_x10` | raw / 10 °C | `0x530` |
| 16 | u16 | `ecu_temp_c` | raw °C | `0x534` |
| 18 | u16 | `egt_delta_c` | raw °C difference | `0x534` |
| 20 | u16 | `fuel_pressure_kpa_x10` | raw / 10 kPa | `0x537` |
| 22 | u16 | `coolant_pressure_kpa_x10` | raw / 10 kPa | `0x537` |
| 24 | u16 | `ecu_error_count` | Number of currently active ECU errors | `0x534` |
| 26 | u16 | `ecu_lost_sync_count` | Lost-sync count | `0x534` |
| 28 | u16 | `knock_count` | Knock count | `0x528` |
| 30 | u16 | `last_knock_cylinder` | Raw cylinder identifier | `0x528` |

## Event packet, type 3, 46-byte body

An event contains a current snapshot, not a replay of earlier CAN frames. It has
the same ten-byte header and validity rules as periodic packets.

| Body offset | Type | Field | Engineering conversion / source |
| --- | --- | --- | --- |
| 10 | u16 | `alert_flags` | Version 2 event bits below |
| 12 | u16 | `status_bits` | Raw `0x526` status |
| 14 | u16 | `rpm` | RPM, `0x520` |
| 16 | u16 | `oil_pressure_kpa_x10` | raw / 10 kPa, `0x536` |
| 18 | u16 | `fuel_pressure_kpa_x10` | raw / 10 kPa, `0x537` |
| 20 | u16 | `coolant_temp_c_x10` | raw / 10 °C, `0x530` |
| 22 | u16 | `battery_v_x100` | raw / 100 V, `0x530` |
| 24 | i32 | `lambda_error_x1000` | raw / 1,000 lambda, `0x520` and `0x527` |
| 28 | u16 | `ecu_error_count` | Active error count, `0x534` |
| 30 | u16 | `ecu_lost_sync_count` | Lost-sync count, `0x534` |
| 32 | u16 | `knock_count` | Knock count, `0x528` |
| 34 | u16 | `last_knock_cylinder` | Raw cylinder identifier, `0x528` |
| 36 | u16 | `fuel_cut_percent` | raw percent, `0x522` |
| 38 | u16 | `ignition_cut_percent` | raw percent, `0x521` |
| 40 | u16 | `traction_cut_request_x10` | raw / 10 percent, `0x524` |
| 42 | u16 | `knock_level_peak` | Raw unitless value, `0x528` |
| 44 | u16 | `knock_correction_deg_x10` | raw / 10 degrees, `0x528` |

| Event bit | Mask | Name | Trigger |
| --- | --- | --- | --- |
| 0 | `0x0001` | `STATUS_CHANGED` | Raw status word changes |
| 1 | `0x0002` | `KNOCK_COUNT_INCREMENTED` | Knock count increases after its initial observation, or changes from 65,535 to 0 |
| 2 | `0x0004` | `ECU_ERROR_CHANGED` | Active ECU error count changes |
| 3 | `0x0008` | `LOST_SYNC_CHANGED` | Lost-sync count changes |
| 9 | `0x0200` | `FUEL_CUT_CHANGED` | Fuel cut changes between zero and nonzero |
| 10 | `0x0400` | `IGNITION_CUT_CHANGED` | Ignition cut changes between zero and nonzero |
| 11 | `0x0800` | `TRACTION_CUT_CHANGED` | Traction cut request changes between zero and nonzero |

Bits 4–8 and 12–15 are not emitted. In particular, counters do not encode severity
or trigger invented vehicle-specific thresholds. Version 2 bit 1 means a knock
count increment; do not apply the legacy `ALERT_KNOCK_DETECTED` label based only on
its bit number. Raw status bit 7 remains independently observable through the raw
status word and ordinary status-change events.

The first received nonzero status/error/lost-sync value generates the existing
change-from-zero event; a first zero does not. The first knock count establishes a
baseline and is not itself reported as an increment. A first nonzero cut request
generates a cut event; a first zero does not. Later changes between two nonzero cut
amounts update telemetry values but do not generate additional cut events.

After the initial knock baseline, a strictly greater new count generates an event,
as does the adjacent 65,535-to-0 rollover. All other decreases establish a new
baseline without an increment event. The DBC supplies no reset marker, so a reset
from 65,535 to 0 is indistinguishable from that rollover; skipped counts spanning a
rollover are not inferred. Counter values are still logged in slow/event snapshots.

After handling each observation, event baselines advance even if the queue rejects
the packet. This preserves drop-newest semantics: failed events are not retried or
replayed. Unchanged subsequent frames do not emit synthetic zero-flag clearing
events. A missing source cannot independently create a counter or cut event.

## Status bits and receiver booleans

Preserve the entire raw 16-bit `status_bits` word. The sender decodes these named
states, and a receiver should derive its displayed booleans using the same bits:

| Bit | Mask | State |
| --- | --- | --- |
| 0 | `0x0001` | Shift cut active |
| 1 | `0x0002` | Rev limit active |
| 2 | `0x0004` | Anti-lag active |
| 3 | `0x0008` | Launch control active |
| 4 | `0x0010` | Traction-control power limiter active |
| 5 | `0x0020` | Throttle blip active |
| 7 | `0x0080` | Knock detected; DBC describes a 250 ms indication after detected knock |
| 8 | `0x0100` | Brake pedal active |
| 9 | `0x0200` | Clutch pedal active |
| 10 | `0x0400` | Speed limiter active |
| 11 | `0x0800` | GP limiter active |
| 12 | `0x1000` | User cut active |
| 13 | `0x2000` | ECU logging |

Boolean validity depends on source bit 5 (`0x526`) in the masks. Do not render an
unreceived zero word as confirmed inactive states. Bits 6 and 14–15 are preserved
in the raw word without adding configured interpretations here. In particular,
AC, nitrous and spare fields must not receive invented meanings.

## Required receiver update

1. Accept version 2 explicitly, retaining version 1 decoding separately if needed.
   Validate magic, total length and CRC before reading any body fields.
2. Dispatch exact body lengths: type 1 = 28, type 2 = 32, type 3 = 46, type 4 = 58.
   Reject unsupported type/length combinations. The existing 96-byte radio buffer
   remains sufficient; type 4 handling is new.
3. Decode the layouts above as packed little-endian values. Lambda target, oil
   temperature and IMU acceleration are signed 16-bit; lambda error is signed
   32-bit. Coolant/IAT, gear, ignition timing, lambda corrections, traction slip and
   knock correction are unsigned exactly as declared by the DBC.
4. Use both validity masks, the per-field source mapping and receive-age tracking.
   Preserve missing/stale status separately from the numeric value and do not
   interpret source masks as ECU sensor-configuration flags.
5. Move thermal/counter/fuel-pressure displays to slow updates. Add the 6-second
   powertrain snapshot, and use event snapshots for prompt cut and knock updates.
   Keep the full measurement units and the configured brake-pressure mapping.
6. Decode version 2 event bits and raw status separately. Log counter values and
   changes; do not derive severity from their raw magnitude. Handle sender uptime
   and sequence wrap, reboot, queue-drop gaps and radio loss.
7. Add receiver fixtures for each version/type, little-endian signed extremes,
   incorrect lengths/CRC, absent/stale sources and counter/cut transitions before
   deploying an updated receiver.

Legacy version 1 placeholder slots such as fuel trim, EGT highest, boost target
and wastegate pressure were not populated by the original sender. They are not
silently repurposed in version 2; only documented version 2 fields should be read.
