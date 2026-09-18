# Sender native tests

From the repository root, run:

```powershell
./test/run_native_tests.ps1
```

The runner requires a C++11 host compiler (`g++` by default). To select another
compiler, use `-Compiler 'path/to/g++.exe'`. Optional `-Sanitize` adds AddressSanitizer
and UndefinedBehaviorSanitizer when that compiler provides the runtimes. The tested
Windows MinGW 6.3 installation uses operating-system guard pages without requiring
sanitizers. Build artifacts go only in `.pio/native-tests/`.

The runner builds and executes five independent programs with warnings treated as
errors:

- `native/`: all 14 supported CAN IDs, independent DBC byte expectations and
  engineering scales, signed oil temperature/lambda target, unsigned values above 32767,
  exact signed 32-bit lambda error, every raw status bit and its named booleans,
  vehicle-speed regression, per-source receipt/freshness, and timer rollover.
- `test_can_task.cpp`: the actual sender CAN task with small Arduino/TWAI/queue
  substitutes. Checks the 20/2000/6000 ms periods, telemetry during CAN silence,
  receive timeouts following the next telemetry deadline, timer rollover, sequence numbers,
  nonblocking drop-newest behavior, dropped-event baselines, driver setup/failure,
  and decoded vehicle speed reaching the queued fast packet. No CAN-transmit API
  is supplied by the substitutes.
- `test_mock_can_task.cpp`: runs the actual task with `TELEMETRY_MOCK_DATA=1` for
  60 simulated seconds. Checks that no CAN driver operation occurs, all fourteen
  sources stay fresh, fast/slow/powertrain packets keep their 20/2000/6000 ms
  cadence, RPM and speed change and repeat after a minute, status/counter changes
  generate events, and queued packets have sequential numbers and valid V2
  envelopes/CRCs. The task feeds a host queue; this test does not emit radio traffic.

The runner explicitly selects mock mode for that test and real CAN mode for all
other tests, regardless of the default in `include/Utils/Telemetry.h`.

Decoder cases place each payload immediately before an inaccessible memory page.
Every DLC from zero through seven must be rejected without reading past its actual
length; valid eight-byte frames must not read a ninth byte. Invalid DLC nine,
null, extended, remote, and unknown frames must leave the entire state untouched.

Four complete radio golden vectors independently specify every populated field,
header byte, packet length, signed integer encoding, and CRC trailer. Their CRCs
were cross-checked using Python's `binascii.crc_hqx(bytes, 0xFFFF)`. Every undersized
output capacity is checked against an inaccessible page, with no partial writes
allowed. Counter tests cover initial baselines, increments, decreases, adjacent
65535-to-zero rollover, and raw values independent of severity. Cut event tests
cover both activation and deactivation without repeated events for positive
magnitude changes.

These host tests validate sender logic and byte layouts. The separate PlatformIO
firmware build verifies the ESP32 target; physical RF delivery and vehicle-bus
timing still require hardware.

No pre-existing sender tests were found. The Windows run used guard pages; the
optional sanitizer mode was not run.

The runner also compiles `test_lora_tx_task.cpp` twice, selecting TX and legacy RX
roles. This includes the actual LoRa task and production serializer. Its dedicated
`tx_integration_stubs/` substitutes only the radio, serial, queue, and clock
boundaries. Checks cover all four V2 frame envelopes, one send per queue entry,
busy polling, the 3 ms completion guard (including failed transmissions and
back-to-back queue entries), minimum one tick at coarse RTOS tick rates, immediate
error reporting, success-log throttling across millisecond rollover, task startup,
initialization failure, and a legacy V1 receive smoke test. These checks model
scheduling logic; they do not measure actual SPI, serial, or RF timing.
