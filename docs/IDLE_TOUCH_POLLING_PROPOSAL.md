# Screen-on idle touch polling proposal

Date: 2026-08-31. Status: **PENDING DECISION**.

This document records a read-only review and a proposed next change. The user
has not yet selected idle touch polling for the current candidate. No source,
IRQ handling, polling interval, I2C frequency, address or wiring was changed
for this proposal. It does not authorize a build installation, reset, serial
session or device access. COM8 remains excluded.

Continue in `tmp/worktrees/aura-post-115-clean`, `main` at `5383b77` with the
existing local OTA, GT911, diagnostic-export and startup-backlight changes
preserved. The main preparation notes and dated hardware handoff remain the
entry points; this proposal does not replace their evidence or restrictions.

## Current behavior

- The LVGL input timer uses `LV_INDEV_DEF_READ_PERIOD = 40 ms`, approximately
  25 callbacks per second when scheduling and guards permit. The project does
  not override that timer after registering the touch input device.
- `LVGL_TOUCH_POLL_INTERVAL_MS = 12 ms` is a minimum interval inside the read
  callback, not an independent 83 Hz polling task. On a normal screen-on
  callback, `readPoints(..., 1, 0)` performs a read without waiting for the
  vendor interrupt semaphore. A failed read has the existing single retry
  after a requested 2 ms delay; startup quiet windows, recovery and runtime
  admission can suppress reads.
- The direct GT911 ISR is installed as a bounded latch but physically masked
  during ordinary screen-on operation. The configured GPIO4 interrupt is
  active-low; the vendor configures its falling edge. This software setting
  is not a measurement of the controller's electrical signal or proof that
  every physical contact will produce a captured interrupt.
- A normal no-data GT911 read attempts both a status-register read and a
  status-clear write. At approximately 25 successful polls per second, that
  suggests approximately 50 I2C transactions per second while idle. This is
  a code-derived estimate, **not measured bus utilization or power use**.
- Screen-off wake is already separate: a fresh IRQ requests a probe on the
  next input callback, with a 2500 ms fallback probe when no IRQ arrives.
  Its policy reports RELEASED to LVGL and requires a valid touch to request
  guarded wake. Wake blocking then avoids turning that contact into a click.
  Reusing this policy unchanged for screen-on input would consume the first
  intended user interaction.

Source locations reviewed:

- [LVGL input interval](D:/21cncstudio/project_aura/tmp/worktrees/aura-post-115-clean/include/lv_conf.h:92).
- [IRQ registration and masking](D:/21cncstudio/project_aura/tmp/worktrees/aura-post-115-clean/src/lvgl_v8_port.cpp:365).
- [Touch read callback](D:/21cncstudio/project_aura/tmp/worktrees/aura-post-115-clean/src/lvgl_v8_port.cpp:1356).
- [Dark-screen wake policy](D:/21cncstudio/project_aura/tmp/worktrees/aura-post-115-clean/src/core/TouchWakePolicy.h:17).
- [Backlight transition IRQ plan](D:/21cncstudio/project_aura/tmp/worktrees/aura-post-115-clean/src/core/BacklightStatePolicy.h:73).

Vendor behavior was checked in the cached, pinned ESP32_Display_Panel source,
particularly `esp_panel_touch.cpp::readRawData` and
`port/esp_lcd_touch_gt911.c::esp_lcd_touch_gt911_read_data`. No vendor/cache
source was edited. The existing driver does not establish that every short
press and release is queued until a later read.

## Proposed design for discussion

Keep the LVGL input timer and active touch polling unchanged. Add a separate
screen-on idle state after a period of successful RELEASED samples. An initial
proposal is **about 1 second of idle time and a 250 ms fallback**, but neither
value is agreed or validated. A fallback deadline is checked by the existing
40 ms callback, so actual servicing is quantized by that timer and scheduling.

While idle, arm the existing bounded interrupt latch and skip I2C reads until
an interrupt is pending or the fallback deadline expires. The ISR must only
signal the latch: no I2C, LVGL calls, blocking wait, extra task or vendor
semaphore wait in the ISR. Preserve the existing arm sequence that samples
the active pin level after enabling the interrupt to cover a held contact.

On an IRQ, return to the ordinary polling path and deliver the first valid
PRESS to LVGL. Continue at the existing rate through a stationary hold, drag
and release, then require the idle period again before reducing reads. Do not
throttle while the cached state is PRESSED, while release-after-wake is pending,
or during startup blocking, errors, recovery or a backlight transition. An
error is not a successful release and must not establish idle eligibility.

If IRQ registration or arming is unavailable but cleanup is safe, retain the
current 40 ms polling behavior. If physical IRQ cleanup cannot be confirmed,
preserve the existing failure handling rather than silently accepting it.
Keep screen-off wake unchanged. IRQ masking before both backlight transitions,
runtime admission, cooperative quiescence and teardown must remain effective.

This can reduce idle I2C traffic; it does not stop LVGL rendering or establish
a meaningful power saving or a reliability improvement. With a missed IRQ,
the first input can be delayed until fallback, and a short contact might be
missed. Those are acceptance questions, not guarantees supplied by a unit test.

## Controlled next step and acceptance

1. Decide whether this optimization belongs in the next candidate and agree
   the idle/fallback values. Until then, keep the implementation unchanged.
2. If selected, implement a small, independently testable screen-on policy,
   preserving all existing guard, screen-off and active-touch behavior. Add
   native tests for first press, stable hold, drag, release, idle re-entry,
   IRQ coalescing, stale/held/lost IRQ cases, fallback, errors, timer wraparound
   and lifecycle transitions. A failed IRQ setup must select normal polling.
3. Build the affected profiles locally and save a separate exact BIN with
   its source snapshot, environment, hardware profile, embedded identity and
   SHA256. Do not overwrite existing BINs or packages. Address, bus frequency,
   power supply and wiring must remain controlled and unchanged.
4. Only after separate installation/test authorization, compare against the
   preceding exact candidate. Verify short taps after long idle, first-touch
   response, rapid taps, stationary long press, dragging and release at screen
   edges. Check sleep/wake, schedule/alarm/web transitions, startup and soft
   restart. Exercise safe IRQ fallback without changing unrelated variables.
   Record physical screen/touch observations separately from API diagnostics.
5. Record idle read/IRQ counts if suitable bounded instrumentation is agreed;
   do not infer traffic reduction from absent error messages. Preserve any
   failure evidence before recovery. Long screen/touch operation must cover
   the new exact BIN before calling this optimization validated.

No PASS transfers from the earlier GT911 diagnostic cold-start or software-
restart series to a new BIN containing this change. That series has its own
capture and physical-observation limits. This proposal neither accepts 0x5D
for production nor establishes a GT911 fault cause. It also does not explain
the earlier isolated CO `--` observation with an unknown timestamp.
