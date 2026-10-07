# Issue #157: touch input stops after long uptime

Checked 2026-10-07. The code defect is reproduced and fixed locally for 1.2.1 beta.
No remote device access, reboot, flash, push, publication, issue comment, or closure occurred.
Physical recovery on the reporter's unit and a new month-long soak are not yet verified.

## Evidence from the issue

- [Issue #157](https://github.com/21cncstudio/project_aura/issues/157) and
  [the reporter's clarification and two diagnostic reports](https://github.com/21cncstudio/project_aura/issues/157#issuecomment-6009947831).
- Both reports identify firmware `1.2.0 - 4.3`, build `02b012b`, target `aura-aq-v1`.
- The reporter confirms that screen readings continue updating and only touch fails.
- Samples are at 2,410,346,585 and 2,410,413,202 ms, 66,617 ms apart (about 27.90 days uptime).
- LVGL timer handlers advance by 1,516, framebuffer flushes/handoffs by 32,
  and display refresh callbacks by 2,599. No display-sync fault, pause, wait timeout,
  or runtime lock failure is reported.
- All exported touch polling counters remain identical: status reads 6,210,381,
  full reads 3, skipped callbacks 25,243,635, fallback probes 5,890,461.
  Touch read errors remain zero and touch is not marked offline.
- The `sda_stuck_low` value belongs to a `before_board_init` snapshot with `live=false`.
  It is not a measurement of the bus during the reported failure.
- DailyHistory queue-full and earlier MQTT errors are separate observations.
  These reports do not establish their root cause or make them the cause of the touch stop.

## Reproduced cause

The touch callback uses a short startup guard, `lvgl_touch_boot_quiet_until_ms`.
Its signed 32-bit deadline comparison was reevaluated forever, without retiring
that startup deadline after it expired. Once the deadline is more than 2^31 ms
in the past (24.855 days plus the startup offset), the comparison becomes true
again and the callback returns RELEASED before any touch polling or error tracking.
This directly explains working rendering, frozen polling counters, and zero read errors.

The same stale guard was confirmed in the installed build's source `02b012b`,
`v1.1.5`, and both current LVGL port files. The reported time is when the user
noticed/captured the failure; it is not a measurement of the precise first failure time.
The reports do not export the startup deadline, so their causal match is strong
code-and-counter evidence, not a runtime trace of that branch on the remote unit.

## Local correction

A shared `TouchWakePolicy::bootQuietWindowActive` function clears the deadline
on its first expiry. Both LVGL 8 and LVGL 9 touch callbacks call it. The cold/warm
startup durations stay at 5/10 seconds, and the touch IRQ, I2C addresses, recovery,
wake policy, and rendering behavior are otherwise unchanged.

Five regressions cover cold/warm expiry, the signed half-range boundary, both
issue-report uptimes, full uint32 wrap with no reactivation, and an unarmed guard.
With the original guard extracted unchanged into the shared function, three of
these tests fail while 23 cases pass. After correction, all 83 focused cases pass
across touch-wake, screen-on-touch, release/framebuffer, and backlight policies.

Native test records:
- Expected pre-fix failure: `.pio/native-tests/reports/20261007T110805Z-09d41126`.
- Passing 83 cases: `.pio/native-tests/reports/20261007T110838Z-35417afe`.

The native IDF builds compile the active LVGL 9 path. The retained LVGL 8 call site
uses the same tested helper but was not independently built on the old toolchain.

## Local evidence

`build-idf-issue157-20261007/` contains the two downloaded JSON files, their SHA256
and selected counter comparison in `analysis.json`, original source and dirty-file
snapshots, before/after test logs, build logs, and verified artifact copies.
The previous four tested app images remain intact under
`build-idf-sensor-audit-20261007/after-artifacts/` and their hashes were rechecked.

## Build verification

All four native ESP-IDF builds succeeded, including image identity, I2C linkage,
retained RTC ABI, and reset-order checks. Each of five artifacts per build was
checked against the post-build stamp and copied into the evidence directory.
The build IDs include `dirty` because the user's pre-existing UI/EEZ edits were
preserved and included. Those files were not committed with this fix.

Source commit on main: `3133ef0fc09bc0bf4b69b76439206ab3e47cc9d1`.
Hub merge: `1e2180303a81aafdd4e9948bcd2f5fc56410bc39`.
Subsequent audit-document commits do not change firmware sources.

| Line/profile | Build ID | App bytes | App SHA256 |
| --- | --- | ---: | --- |
| main/4_3 | `3133ef0-dirty` | 4646352 | `90a235fbff0b30ae29b5a2cef2ef2d2a5ffa01c6fec7687f4183e77bf06eeaed` |
| main/7_dual_i2c | `3133ef0-7-dual-i2c-dirty` | 4646720 | `79f1435253216ce2c122467fd31f68adaa649e8aefd38cb0de60948e28406d8a` |
| hub/4_3 | `1e21803-dirty` | 4720640 | `99e1c84ea2000e71452aee14708e7257da13bce16f78307517906374c1f2955a` |
| hub/7_dual_i2c | `1e21803-7-dual-i2c-dirty` | 4720976 | `c3460b51774d9b77ef0a4572731f76a25b9ef90129bb279f3c91f5fcc9564b48` |

## Next validation

Before releasing, use a separately authorized matching-profile device test to
verify normal startup, touch interaction and dark-screen wake. Host tests already
exercise the long-uptime arithmetic without waiting a month; they do not replace
physical confirmation from the reporter or a new long-running device test.
The issue should remain open until that follow-up evidence is available.
