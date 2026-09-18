# RRU embedded telemetry receiver

This ESP32-S3 firmware receives RTU LoRa telemetry and emits one CSV row per
packet over serial. It accepts both legacy schema version 1 and the expanded
schema version 2. The version 2 wire layouts match
[the RTU protocol specification](../../RTU/docs/telemetry-v2.md).

## Packet compatibility

The receiver checks the `TM` magic, supported version and type, exact frame/body
length, and CRC16-CCITT before decoding any measurements. All multibyte wire
integers are little-endian. Invalid frames produce an `rx_error` row instead of
measurement data.

| Packet | Type | Version 1 body bytes | Version 2 body bytes | Version 2 nominal cadence |
| --- | --- | --- | --- | --- |
| FAST | 1 | 30 | 28 | Every 20 ms (50 Hz) |
| SLOW | 2 | 46 | 32 | Every 2,000 ms |
| EVENT | 3 | 28 | 46 | On qualifying transitions |
| POWERTRAIN | 4 | Unsupported | 58 | Every 6,000 ms |

Every radio frame adds seven bytes for its header and CRC. Version 2 is a new
layout, not an appended version 1 body. Its fast packet contains the core driving
signals, its slow packet contains temperatures, pressures and counters, and its
powertrain packet contains the additional engine, traction and acceleration
signals. Events include current cut and knock measurements for timely updates.
The table describes sender generation intervals; radio airtime and queueing can
delay actual packet arrival. With no losses, periodic output averages 50.667 rows/s
plus events. The receiver prints each packet once; it does not duplicate cached
rows on a 50 Hz timer. Sender snapshots continue after CAN silence, with freshness
bits clearing as sources age beyond 2,000 ms; 50 Hz transport does not imply that
every source CAN signal updates at 50 Hz.

Version 2 speed comes from CAN `0x522`; brake pressure retains the configured
`0x538` mapping. CSV measurements use engineering units indicated by their column
names. Scaling follows the RTU specification: most pressure, temperature, speed,
percent and angle fields divide by 10; battery voltage and injector pulse width
divide by 100; lambda and acceleration in g divide by 1,000. Raw counts, RPM, gear,
knock peak, ECU temperature and EGT delta remain unscaled. Lambda target, oil
temperature and acceleration are signed 16-bit wire fields; lambda error is
signed 32-bit. Coolant temperature, intake temperature, gear, ignition timing,
lambda corrections and traction slip remain unsigned as defined by the sender.

Version 1 packet meanings and scales remain separate. Its placeholder fields are
not repurposed as version 2 signals. Version 2 event bit 1 means
`KNOCK_COUNT_INCREMENTED`, while version 1 bit 1 retains its legacy meaning.
Version 2 adds fuel-cut, ignition-cut and traction-cut transition flags at bits
9, 10 and 11. Raw status and event flags are different words. Status bits 6, 14
and 15 are preserved without inventing named interpretations.

## CSV and validity

The serial stream begins with its CSV header. Each `rx_packet` row contains only
the measurements carried in that packet; other measurement columns are empty.
The first 44 legacy columns retain their names and ordering; 31 appended columns
bring the total to 75. Use the emitted header to map columns instead of assuming
a fixed column count. The appended columns, in order, are:

```text
schema_version,received_mask_hex,fresh_mask_hex,brake_pressure_kpa,
lambda_a,lambda_b,lambda_target,fuel_inj_pulse_width_ms,
driven_wheel_speed_kph,non_driven_wheel_speed_kph,
traction_slip_measured_pct,traction_slip_target_pct,traction_cut_request_pct,
knock_level_peak,last_knock_cylinder,
acceleration_x_g,acceleration_y_g,acceleration_z_g,
shift_cut_active,rev_limit_active,anti_lag_active,launch_control_active,
tc_power_limiter_active,throttle_blip_active,knock_detected,
brake_pedal_active,clutch_pedal_active,speed_limiter_active,
gp_limiter_active,user_cut_active,ecu_logging
```

Version 1 rows set `schema_version` to 1 and leave the other appended columns
blank. Version 2 rows set it to 2. The existing `boost_duty_pct` column carries
version 2 boost solenoid duty; old placeholder columns remain blank for version
2. Status booleans are 0/1 and appear only in fast/event rows with a received
`0x526` source. Error rows leave appended columns blank.

Every version 2 packet carries separate `received_mask` and `fresh_mask` values,
printed in `received_mask_hex` and `fresh_mask_hex`. Their bits refer to
successfully decoded CAN sources at that packet's sender snapshot time:

| Bit | CAN source |
| --- | --- |
| 0 | `0x520` |
| 1 | `0x521` |
| 2 | `0x522` |
| 3 | `0x523` |
| 4 | `0x524` |
| 5 | `0x526` |
| 6 | `0x527` |
| 7 | `0x528` |
| 8 | `0x530` |
| 9 | `0x534` |
| 10 | `0x536` |
| 11 | `0x537` |
| 12 | `0x538` |
| 13 | `0x600` |

An unreceived source produces blank measurement/status columns, rather than a
false zero or false inactive state. A received but stale source retains its
numeric value with the corresponding freshness bit cleared. A valid physical
zero is printed as zero. Lambda error requires both `0x520` and `0x527` to have
been received; its freshness depends on both source bits. Version 1 has no source
validity masks and cannot provide these guarantees. Masks establish CAN frame
receipt, not ECU sensor configuration. Reserved source bits 14-15 and freshness
bits without corresponding receipt bits are rejected as invalid masks.

The receiver does not cache a combined vehicle state. Each row retains its own
sender uptime `tx_ms`, receiver uptime `rx_ms`, sequence number and validity
masks. A newer fast row must not refresh wheel speed, lambda A/B or other
measurements held from an older powertrain row. Source freshness means no older
than 2,000 ms **at the sender snapshot**, not at a later display time. Host
consumers must track packet arrival age independently, including when CAN or RF
traffic stops. Receiver time is not acquisition time, and the two board uptimes
are not synchronized clocks. Consumers should handle sequence/uptime wrap, sender
reboots and missing packets without inventing interpolated readings or event
severity.

`RRU/Software` and Sentio are separate consumers and are not updated by this
embedded change. Their parser/topic definitions must be updated to accept
POWERTRAIN, retain the new columns, interpret schema version and event bits, and
apply validity and receive-age rules. The current desktop parser learns the
header but its topic/field lists omit POWERTRAIN and the newly added signals.

## Runtime and verification

Both boards now use 915 MHz, 500 kHz bandwidth, spreading factor 6, coding rate
4/5, private sync word and a 12-symbol preamble. Transmit power remains 14 dBm.
The preamble length follows the LR1121 SF5/SF6 recommendation. Both boards must
use this profile together; old SF9/125 kHz firmware will not receive it even
though the CSV and packet schemas have not changed. FAST airtime is 11.680 ms,
and periodic packets consume about 59.317% RF airtime. Including the sender's
3 ms gap between transmissions raises modeled occupancy to 74.517%, before
polling/setup overhead and events. See the RTU protocol specification for the
calculation and the 1 km field-validation target. Range and sustained throughput
have not been measured on the hardware.

The 255-byte receive buffer fits every supported frame. The receiver polls each
RTOS tick (1 ms in this build), captures the completed packet's timestamp/RSSI/SNR,
and rearms before CSV formatting and serial printing. It runs a single LoRa task,
prints directly to serial and has no packet queue. A host that stops draining USB
can still cause backpressure and packet loss; rearming does not provide an
unbounded radio buffer. Legacy `Outputs`/`LoRaOutputs` files are not used by this path. A bounded,
reused 2,048-byte CSV buffer avoids growing the LoRa task's stack with the
expanded schema.

Build the receiver from this directory:

```powershell
pio run
```

Run the host-side decoder and CSV tests without a connected board:

```powershell
powershell -ExecutionPolicy Bypass -File test/run_native_tests.ps1
```

The runner accepts `-Compiler <path>`, `-Sanitize`, and `-SenderRoot <RTU path>`.
It builds under `.pio/native-tests` and covers version 1 compatibility, all four
version 2 packet types, RTU wire fixtures, signed values, malformed frames,
missing/stale sources, status/event meanings, CSV scales and bounded output.
Task integration tests compile the actual LoRa task with radio/serial fakes and
exercise packet output, errors, busy polling, quiet timeouts, metadata capture
and receive rearming before serial output, start-failure backoff and header/init
behavior.
Firmware compilation and native tests do not replace CAN/RF testing on both
physical boards.
