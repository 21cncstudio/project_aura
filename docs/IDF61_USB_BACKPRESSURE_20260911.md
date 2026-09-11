# IDF 6.1 USB logging backpressure regression, 2026-09-11

Status: `c52e193` is installed on both boards and passed the bounded closed-port
observations below. In the subsequent physical check, the user reported that
everything appears okay on both boards. This is a provisional positive result;
long-term stability, cold power cycles and OTA round trips remain untested.
The preceding `0eadb70`
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
backpressure as the cause. The subsequent closed-port observations and user
feedback below support the correction under the tested conditions.

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

## Candidate and installation records

Firmware commit: `c52e1934659c51bce62ad7fc4b5f9bbf207cdb04`.
Both builds passed identity, RTC layout, OTA integrity, I2C linkage and upstream
restart-order checks. `setup()` disassembly for each ELF confirms an argument
of zero to `HWCDC::setTxTimeoutMs` between `HWCDC::begin` and `Logger::begin`.
The disassembly files and immutable candidates are in the regression evidence root.

| Profile | Build ID | BIN bytes | SHA256 |
| --- | --- | --- | --- |
| 4.3-inch | `c52e193` | 4032672 | `6cdfeafe78cc45b21fb2c77e5f229dab04e6f6aba5caae474515b39ab1445d27` |
| 7-inch | `c52e193-7-dual-i2c` | 4033072 | `ac9ff4977e7653ee106cf1783aed084a1ed9ffdd7972864e938e9b310b6d2839` |

Exact previous `0eadb70` readbacks are reused as backups only after comparing
their digests with the current app in flash. The original full IDF 5.3 partition
backups remain preserved. Installers retain all existing USB/MAC/target/partition
guards, write only the already-selected active app and verify full readback SHA256
plus outside-app preservation before the normal return from USB download.

Each boot capture is limited to 30 seconds and closed before a separate GET-only
observer starts. The observer records every sample and flags sensor invalidation,
new lock errors, delayed LVGL handlers and resets. Static settings screens may
legitimately have no new flush, so a quiet flush counter alone is not classified
as a stall. Physical action/response evidence is tracked separately.

## Closed-port results

Both app-only installations passed exact readback SHA256 and outside-app
preservation. Both 30-second boot captures were closed before observation.
No serial port was opened during the following GET-only checks.

| Profile | Observation | Samples | Uptime range | Flagged issues | Largest sampled LVGL handler age | Slowest API request |
| --- | --- | --- | --- | --- | --- | --- |
| 4.3-inch | 10 minutes | 270 | 58-657 seconds | 0 | 320 ms | 250 ms |
| 7-inch | 6 minutes | 163 | 52-410 seconds | 0 | 356 ms | 328 ms |

Temperature, humidity, CO2, PM2.5 and pressure remained available in every sample.
VOC/NOx became available after the existing gas warmup on both boards; HCHO was
available after its warmup on 4.3-inch. No reset, new LVGL lock failure, touch-read
error, framebuffer wait timeout or ownership violation was observed. These are
bounded samples, not a guarantee against every short transient or a soak test.

The touch full-read counters remained at the three boot reads throughout both
observations. Thus these observations did not exercise fresh physical taps.
The user was asked to repeat Settings/Back actions with COM10/COM11 closed;
after the observation sessions ended, the user replied: "да вроде бы ок все"
(everything seems okay). Record this as provisional physical confirmation for
both `c52e193` images. Exact tap counts and measured response latency were not
provided; do not attach that later feedback to the earlier automated counters.
Earlier confirmation that 7-inch worked with a serial reader open belongs to
`0eadb70` and remains separate.

The dated follow-up is `USER_FEEDBACK_20260911T1854Z.json` in the regression root.
It supersedes the pending-feedback field in the original `SUMMARY.json` without
rewriting that earlier observation record.

Evidence index: `SUMMARY.json` in the regression root. Per-profile
`*-nonblocking-flash/RESULT.json` records writes/preservation and
`*-closed-port-observation/RESULT.json` plus `samples.jsonl` records runtime.
All flash, serial-capture and observation processes have completed. Main and
the release checkpoint remain unchanged; no publication was performed.
