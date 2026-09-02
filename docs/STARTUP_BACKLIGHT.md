# Startup backlight candidate

> Current status, 2026-09-02: the startup-backlight behavior remains in both
> production profiles. The separate GT911 startup diagnostic environment and
> its additional reads were retired. Candidate tables below are retained as
> historical artifact provenance.

This documents the local candidate in `main` based on `5383b77`, prepared on
2026-08-31. The intended behavior is to keep the backlight off until the complete
boot logo has been rendered and its framebuffer hand-off acknowledged. This is
a source design, not an optical or hardware acceptance result.

## Initial output and driver

`include/Ch422gBoardPolicy.h` changes the 4.3-inch initial CH422G output image
from `0xDF` to `0xDB`: only EXIO2, the backlight output, changes to OFF. Native USB
selection and the other outputs retain their previous levels. The 7-inch image
remains `0xD1`, with EXIO2 already OFF.

The board configuration selects the vendor custom backlight driver instead of
the switch-expander driver. The latter writes ON during `begin()`, before its
idle state is applied. The custom driver avoids that begin-time write; its
callback retains the same binary EXIO2 switch behavior, and idle-OFF is enabled.
The expander initialization still owns direction setup. Attaching the backlight
reads the driver's software brightness rather than assuming it is ON.

This cannot guarantee darkness before the first successful CH422G output write.
Power-on defaults, retained output levels and physical behavior during reset
still require measurement. A successful driver operation is not an optical
measurement of the backlight.

## Logo-to-backlight sequence

1. Create and load the boot-logo screen while the backlight remains OFF.
2. Wait for the logo to be the active, fully opaque screen with no running
   animation. The compiled `ui_runtime.c` currently uses immediate `lv_scr_load`;
   generated `ui.c` contains a fade but is not part of this build. While holding
   the LVGL mutex, invalidate the full logo screen and capture the current frame
   generation.
3. Release the mutex and wait for a newer framebuffer hand-off acknowledgement.
   Rendering remains on the LVGL task. The generation advances only after the
   framebuffer switch and subsequent VSYNC acknowledgement, not at flush entry
   or after an arbitrary delay.
4. Arm `StartupWake`. The backlight owner queues the existing guarded wake path:
   bus quiet period, switch, settle and guarded render completion. Startup is
   recorded as its own source, not as a touch or web request.

The candidate allows up to 1000 ms for logo completion and another 1000 ms for
the fresh frame acknowledgement. Missing screens, unsupported acknowledgement
paths, a paused or failed display, and expired waits do not authorize startup
ON. UI initialization reports failure to the existing startup recovery/headless
policy; this change does not replace that policy or add a new recovery loop.
Startup darkness is not treated as ordinary touch-to-wake sleep, and regular
backlight requests cannot bypass the pending startup gate.

The framebuffer/VSYNC acknowledgement establishes completion of the software
driver path. It does not prove the physical panel displays the expected pixels.

The five-second logo dwell starts when the UI observes the driver's confirmed
ON state, so time spent waiting in darkness does not consume the visible dwell.
The 30-second schedule boot grace is restarted from the successful startup ON
operation. Existing controls and schedule behavior resume through the ordinary
backlight state machine.

## Retained diagnostics and scope

`BacklightWakeBreadcrumbs::Event::StartupWake` is appended as value `6`, with
text `startup_wake`. Values `0` through `5` are unchanged. Current version 3
records accept the new value; the retained record stays 60 bytes and its
two-slot storage stays 120 bytes. Legacy version 2 remains limited to
`AlarmWake` (`3`). Older firmware is not required to decode event `6` and may
reject such a retained record; preserving the layout is not a backward-decoding
guarantee.

The backlight part of the change does not alter GT911 addresses, bus frequencies,
wiring or the power supply. `project_aura` remains at `0x14`, while production
`project_aura_7` selects `0x5D`. The retired diagnostic environment and its
additional startup reads are not part of current builds.

## Validation and hardware acceptance

Local validation on 2026-08-31:

- 141 native Unity cases passed with source identity verified by the canonical
  launcher: backlight state, wake breadcrumbs, wake power guard, LVGL wait
  policy, and CH422G/reset/probe/I2C routing for both profiles.
- Both normal PlatformIO builds passed, including OTA image identity/checksum,
  retained RTC layout and restart-linker checks. EEZ postprocess `--check` and
  `git diff --check` passed. Generated EEZ UI files were not edited.
- Independent source review checked startup request gating, complete logo state,
  framebuffer acknowledgement, guarded wake progression and failure paths.
  Native tests do not execute the complete UiController/FreeRTOS/display path.

Separate candidate BINs are saved under
`D:\21cncstudio\project_aura\logs\startup_backlight_20260831T174744Z\build`:

| Environment | BIN bytes | SHA256 |
| --- | ---: | --- |
| `project_aura` | 4307888 | `9f4d9002c637ae3cc556d3990bfa43c545630574eb04cf8721dcd0f6f27d4a24` |
| `project_aura_7` | 4308352 | `a72926782e4709eb99d30c221eba0c479bfab8cbc192f30e3aed03c55cde6ec7` |

The evidence directory also contains source snapshots, logs, native JSON reports
and `ARTIFACTS.json`. Dirty build IDs can repeat across local candidates; use
these BIN hashes and snapshots for exact identity. These are internal unsigned
build artifacts, not release packages. Hardware and optical acceptance remain
**pending**. Do not transfer results from a different source snapshot or BIN.

After separate authorization and exact BIN identification, record each profile
and restart category separately:

- Physical cold start: observe the full interval from power application, any
  initial flash or blank frame, complete logo visibility and its visible dwell.
- Software restart: record the previous display state and the entire transition
  to the logo and normal UI; do not count this as a cold start.
- Hardware reset: keep its result separate from software restart and physical
  power cycling, including output behavior before firmware gains control.
- Normal UI: confirm the full screen, continuing redraw and responsive touch
  after the logo, not merely network availability or a logged acknowledgement.
- Backlight controls: verify ordinary OFF/ON and supported touch, web and MQTT
  wake paths without changing the test configuration to enable absent services.
- Schedule: verify boot grace and the subsequent configured ON/OFF behavior;
  record any discrepancy with exact timing and retained wake diagnostics.

No hardware commands, serial opening, reset or flash are authorized by this
checklist. COM8 remains excluded. Preserve failure evidence before recovery.

## Combined final candidate, 2026-09-01

The later integrated candidate retains this startup gate and adds the corrected
framebuffer ownership path. Three isolated builds succeeded with OTA target,
ESP checksum/hash, retained RTC layout and restart-linker verification:

| Environment | GT911 | BIN bytes | SHA256 |
| --- | --- | ---: | --- |
| `project_aura` | `0x14` | 4316912 | `0d48aadcea89f02524fcbd8879bd6e613e5a0d46f0f2a16f451e7fcfb50d5bd0` |
| `project_aura_7` | `0x5D` | 4317408 | `9470262a974a3ee85d2587cf35fda962956f5b5620fb74fb634f012c12c7ed52` |
| `project_aura_7_gt911_5d` | `0x5D`, diagnostic | 4318608 | `45c1fbcdca82f6abe275122e362a892d5ae853e90ddd7b7672467303cc799c7e` |

Evidence is under
`D:\21cncstudio\project_aura\logs\post_audit_release_candidate_20260831T234315Z`.
These exact BINs have not been flashed. No optical, cold-start or long-run PASS
is assigned.
