# IDF 6.1 USB logging backpressure regression, 2026-09-11

Status: fix prepared; hardware qualification is pending. The preceding `0eadb70`
trial passed installation integrity and initial API checks, but failed the user's
physical test. Initial IRQ registration, zero error counters and healthy API
samples were insufficient to qualify real touch interaction.

## Observed failure

The user reported `---` sensor values on 4.3-inch and nonresponsive touch on both
boards, then clarified that touch responded with a large delay and readings
alternated between valid and `---` on both boards. Fresh read-only snapshots
confirmed missing SEN66/HCHO values, stale display flushes and increasing LVGL
lock timeouts, while Wi-Fi and display refresh callbacks remained active.

Evidence: `D:\21cncstudio\project_aura\logs\idf61_regression_20260911T1826Z`.
No reset or write was performed while preserving these failures.

- Before opening a serial reader, 4.3-inch uptime 1169 seconds had null
  temperature/CO2/HCHO and a display flush age of 61986 ms.
- With a receive-only COM11 session open, the same boot at uptime 1272 seconds
  had temperature/CO2/HCHO restored and flush age 29 ms. The 7-inch board still
  had stale sensors while its serial port remained closed.
- After a receive-only COM10 session opened, 7-inch recovered without a restart:
  uptime 867 seconds, temperature/CO2 restored, flush age 5 ms. The user confirmed
  that 7-inch now worked normally. Its serial log includes Settings/Back presses.
- Serial evidence is under the original trial root in `4_3-regression-live` and
  `7_dual_i2c-regression-live`; both captures transmit zero bytes and do not reset.

## Mechanism and fix

The pinned [Arduino 3.3.11 HWCDC implementation](https://github.com/espressif/arduino-esp32/blob/3.3.11/cores/esp32/HWCDC.cpp)
defaults to a 100 ms transmit wait and allows twenty consecutive timeouts per
write when USB remains plugged in but the host does not drain data. Aura's Logger
emits nine write fragments for a tagged line, so one line can take approximately
18 seconds. UI callbacks log while holding the LVGL lock; sensor polling and
other logging tasks can also wait on the output path. Two warning timestamps in
the failing 7-inch snapshot are 36 seconds apart, consistent with two such lines.
The source mechanism and the read-only open-port recovery support USB logging
backpressure as the cause; the forthcoming test must validate the correction.

Immediately after `Serial.begin`, native USB Serial/JTAG now sets
`Serial.setTxTimeoutMs(0)`. Diagnostic writes therefore use nonblocking queue
operations and may drop bytes under backpressure. In-memory/API logs remain
available. The UART console and upstream managed sources are unchanged.

## Required validation

- Build both profiles and preserve exact commit-linked candidates and checksums.
- Install sequentially into the already identified active app slots, preserving
  all other partitions and verifying readback hashes.
- Close serial capture before testing touch. Check actual Settings/Back actions,
  repeated sensor freshness, increasing display flushes and absence of new LVGL
  lock failures with no host application reading COM10/COM11.
- Include the user in the final physical display/touch check. Do not transfer a
  healthy serial-connected test to unattended/closed-port operation.
