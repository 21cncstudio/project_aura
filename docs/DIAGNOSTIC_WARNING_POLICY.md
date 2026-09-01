# GT911 and LVGL diagnostic messages

Local candidate prepared on 2026-08-31, based on main 5383b77 with the existing
OTA, dual-profile and startup-backlight work preserved. The user requested that
normal operation at GT911 0x5D should not appear as a warning. This changes
message classification and lock accounting, not the hardware address selection,
I2C transactions, bus timing, touch polling or startup rendering sequence.

## GT911

The diagnostic environment records these successful checks at INFO:

- The configured RESET/INT selection sequence succeeded.
- The configured address returned product ID 39,31,31 (GT911).
- Its configuration register was read successfully.
- The alternate address returned ESP_FAIL, but only after both configured
  address reads succeeded.

Failures at the configured address, invalid product IDs, configuration-read
failures, alternate-address timeouts/unknown failures, and an unexpected second
responder remain WARN. Existing RESET/INT path failures remain ERROR. An
alternate ESP_FAIL is reported with its raw return code; it is not relabelled
as an electrically measured NACK. No scan, fallback address, retry or additional
reset was introduced.

SystemEventPolicy explicitly retains GT911DIAG INFO in /api/events and existing
event mirroring. INFO does not enter the user-facing alert buffer or
/api/diag.last_errors. Actual warnings and errors still appear there. The
diagnostic export includes /api/events, subject to its finite history buffer.

This policy is based on the role and result of the probe, not on 0x5D being a
special fault. 4.3-inch remains at 0x14. The separate 7-inch diagnostic
environment remains at 0x5D; this logging change does not promote the production
environment before its outstanding qualification is complete.

## Subsequent 7-inch profile decision, 2026-09-01

The user subsequently selected 0x5D as the standard address for the split
7-inch firmware. `project_aura_7` therefore selects 0x5D without enabling the
extra startup identity/config/opposite-address reads. The separate
`project_aura_7_gt911_5d` environment selects the same 0x5D address and adds the
bounded structured startup snapshot. `project_aura` remains at 0x14. Address
selection and diagnostic-read enablement are separate build settings and are
recorded separately in `build-identity.json`.

This source decision does not transfer hardware PASS from an earlier BIN. Bus
frequencies, power, pins and wiring are unchanged.

## LVGL mutex accounting

Only the short mutex attempts in the boot-logo wait loop call
lvgl_port_lock_startup_logo(). They use the same recursive mutex and timeout
conversion as before, with the same overall logo deadline. Unsuccessful
attempts are retained in startup_lock_miss_count.

All other calls still use lvgl_port_lock(), including initial UI setup and any
runtime callers executing during startup. Their unsuccessful attempts remain
in lock_fail_count. Counters use atomic operations, are retained for the boot,
and are not cleared or rebased at logo readiness. There is no global startup
flag that could suppress another task's real lock failure.

After the logo wait, a compact UI INFO record reports both totals:

`Boot logo mutex: startup_miss=N, lock_fail=M`

The record is emitted before checking the overall logo-wait result, so an
exhausted wait still has diagnostics followed by its original ERROR. The
existing exact framebuffer-ACK message, heartbeat message format, runtime WARN
conditions, touch/VSYNC diagnostics and stall/recovery policy remain unchanged.
The separate INFO avoids truncating critical fields in the existing heartbeat.

This does not retrospectively establish the cause of earlier lock_fail=1
records. A later hardware test must use the exact new BIN and inspect both
counts. A runtime lock failure still produces its warning; a successful frame
ACK and quiet logs cannot prove physical screen/touch behavior.

## Validation evidence

The source snapshot, native reports, build logs and exact candidate manifest are
under `D:\21cncstudio\project_aura\logs\diagnostic_warning_policy_20260831T204001Z`.
See RESULT.md there for completed checks and hardware limits. Native policy
tests do not exercise task scheduling or the full FreeRTOS/display path.
No firmware upload or hardware test is authorized by this document.
