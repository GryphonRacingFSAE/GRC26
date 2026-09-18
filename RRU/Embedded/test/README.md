# Receiver native tests

From `RRU/Embedded`, run:

```powershell
./test/run_native_tests.ps1
```

The runner needs a C++11 host compiler (`g++` by default). Use
`-Compiler 'path/to/g++.exe'` to select one. `-Sanitize` enables AddressSanitizer
and UndefinedBehaviorSanitizer when the compiler provides those runtimes. Build
artifacts stay in `.pio/native-tests/`. Compiler warnings are treated as errors.

When the sibling RTU checkout is present, the runner also checks that the receiver
`TelemetryV2.h` is byte-identical to the sender's schema. A standalone or staged
checkout can use `-SenderRoot 'path/to/RTU'`; without either location the runner
reports that the optional schema comparison was skipped.

The native decoder/CSV program verifies:

- Four complete independent RTU V2 radio vectors and three V1 compatibility
  vectors, including CRC trailers independently checked with Python
  `binascii.crc_hqx(frame_without_crc, 0xFFFF)`.
- Every populated V2 wire field, including signed 32-bit lambda error, signed
  lambda target/oil temperature/acceleration, and unsigned values above 32767.
- Truncated prefixes, surplus bytes, null and oversized input, all single-bit
  corruptions, invalid framing/version/type/body length with valid CRC, reserved
  source bits, invalid freshness masks, and cleared output after rejection.
- The original 44 CSV columns, 31 appended columns, exact engineering scales,
  all 14 sources' receipt masks, retained stale values, missing-source blanks,
  legitimate physical zero, raw status bits and all 13 named booleans, raw event
  flags/counts, V1 output, signed conversion limits and escaped error messages.
- A fixed 75-column header and every data/error row, rejection of malformed
  packets as normal measurement rows, and every undersized CSV output capacity.

Input and output buffers end immediately before inaccessible memory pages on
Windows and POSIX. Buffer overreads/overwrites therefore fail even without a
sanitizer runtime. The tested Windows MinGW installation used these guard pages;
the optional sanitizer mode was not run.

The second program includes the real receiver LoRa task with small host substitutes
for Arduino, RadioLib and task initialization. It exercises RF polling, V1 and V2
CSV dispatch, malformed packets, CRC/read failures, timeout rearming, start-receive
retry delay, and one-time startup/header behavior. No transmit API is available
in its radio substitute.

The validated run passed **201,193 native checks** and **139 integration checks
in seven groups**. A PlatformIO receiver firmware build is also required to check
ESP32 compilation. These tests do not establish hardware RF delivery or vehicle
timing performance.
