# Memory monitor policy

## Production behavior

Both production profiles are boot-only:

- `project_aura` for Aura AQ 4.3-inch;
- `project_aura_7` for Aura AQ 7-inch dual-I2C SCL6.

`MemoryMonitor::logNow("boot")` remains enabled after Board/LVGL startup. It
records the detailed heap snapshot once. `MemoryMonitor::poll()` receives a
zero interval in production, so it does not repeat the full heap walk every
15 minutes.

The build identity records `periodic_memory_monitor_enabled=false`, but this
runtime setting does not change the production firmware flavor or hardware OTA
target. Build generation fails closed if the setting is changed to `true`;
future periodic experiments must first define an explicit diagnostic firmware
lane instead of silently producing a production image.

## Reason

The former periodic snapshot performs overlapping largest-free-block queries
for the internal heap, all 8-bit heaps, and PSRAM. These queries walk heap
structures while holding heap synchronization. The display boot diagnostics
already avoid the same query because it can delay RGB bounce-buffer refill and
shift scanlines.

A controlled 4.3-inch A/B run disabled only the periodic MemoryMonitor while
retaining its boot snapshot. It crossed 38 former 15-minute boundaries with no
positive RGB recovery transition. The unchanged 7-inch control continued to
accumulate recoveries on the same cadence. This supports removing the periodic
operation from production; it does not prove that every possible visible
one-frame artifact has been eliminated.

The dated internal evidence summary is
`D:\21cncstudio\project_aura\logs\memory_monitor_isolation_5587372_20260901T214456Z\hardware_flash_COM11_20260901T214950Z\MORNING_AUDIT_20260902T0744Z.md`.
The live captures remain internal test evidence, not release artifacts.

The current diagnostic API still exposes free and minimum-free heap values for
live support checks. Reintroducing periodic telemetry, including a free/min-only
variant, requires a separate diagnostic build and hardware A/B test. Do not
re-enable the full largest-block walk by merely increasing its interval or
moving it to another task.

## Transition compatibility

The temporary `project_aura_4_3_memlog_off` environment and its diagnostic build
identity are preserved in Git history and hardware evidence, not in the active
PlatformIO environment list. OTA validation continues to recognize
`aura-aq-diag-v1` so a device running that service-installed candidate can
return to `aura-aq-v1` production firmware.
