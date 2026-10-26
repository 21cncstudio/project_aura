# Standalone main cleanup and 4.3-inch overnight test — 2026-10-26

## Completed history cleanup

At the owner's explicit request, 14 local Hub-preparation commits from 7d3e403a
through 70ba9505 were removed from main's ancestry. They remain recoverable under
codex/archive-main-hub-preparation-20261026 at 70ba9505d9d1d8e7ac1888f5a6f6628d8c2c0c3f.
This is history cleanup, not a revert commit hiding those commits in main.

Standalone source is exactly d7f7dbc47259823cc5ef0b7f5feb34f5788a6991. The general
SEN0466 log correction 4cc06321 and all earlier main fixes remain. Compared with
public checkpoint 26339000, the only application-source difference is that
SEN0466 correction. Removed preparation includes ChartSeries/ChartHistoryArchive,
their ChartsHistory generation changes and tests, UiScreenHost and its runtime
hooks, and archive-related test CMake additions. No Hub client, pairing/upload,
BLE or Outdoor service was found in main. Existing outdoor-air help text is unrelated.

firmware-hub was not edited. feature/aura-link remains at
6b69ae5d9b838b2ea32c92333be4c7bb56925ea0. No remote ref, tag or release was changed.
Working tree was checked clean after the history operation. Current commit adds
only this record, status and an explicit clarification of the existing boundary.

## Hardware test in progress

The user authorized flashing newly connected Aura 4.3 and observation until
27 October 2026, 13:00 Europe/London (13:00 UTC). Target: native USB COM5,
MAC/serial 94:A9:90:04:6F:48, ESP32-S3 rev0.2, 16 MiB flash / 8 MiB PSRAM.
Its original image descriptor confirms aura-aq-v1 and its partition table exactly
matches main. Active app0 is at 0x10000. Other serial devices are outside this test.

Local evidence: ../build-idf-soak-4_3-20261026/ (ignored; includes private backup).
history-cleanup.json and removed-main-commits.txt identify the source operation.
Original full flash: flash-before.bin, SHA256
8fb922ec856fd089924448f22f7ab174be98f895dadb8a31eb6797fc34e6747b.

An intermediate 70ba950-dirty image with only the missing UiScreenHost include
was built and written before the owner requested isolation. It was never booted
for the soak: the board stayed in its flash stub. candidate/, flash-write.log and
flash-after.bin preserve that attempt. This image is superseded and must not be
used for final qualification. Final standalone build/flash/monitor evidence will
be written under final-candidate/, final-flash-*, flash-verified.json and
monitor-status.json in the same evidence directory.

Full initial host suite and 114 tooling tests passed. Those ran before history
cleanup; affected ChartsHistory/UI/build paths require new validation. Both final
profile builds and the final hardware run are pending at this documentation commit.

## Measurement scope

Boot PSRAM test and the boot memory snapshot; ongoing internal free/minimum heap,
uptime, display/touch faults, sensor state, Wi-Fi and SD progress; one receive-only
serial session and bounded read-only HTTP load windows alternating with baseline.
The collector has an absolute stop at 13:00 UTC and requests system-awake only
while recording. A thread heartbeat will inspect progress and summarize completion.

Full heap walks remain boot-only per MEMORY_MONITOR_POLICY.md because previous
periodic walks disturbed RGB refresh. This production BIN does not expose live
PSRAM free/fragmentation or per-task stack watermarks. Overnight observation does
not prove exhaustive RAM-pattern coverage, physical touch/display appearance,
cold power cycles, OTA install/recovery, long-term uptime, or 7-inch hardware.
Firmware metadata remains 1.2.1-beta; no stable release is published by this test.

Recorder/verification references:
- https://docs.espressif.com/projects/esptool/en/latest/esp32s3/esptool/advanced-commands.html
- https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/system/heap_debug.html
