# Screen-on adaptive touch polling implementation and qualification note

Date: 2026-09-01. Status: **IMPLEMENTED LOCALLY, NOT HARDWARE QUALIFIED**.

The adaptive screen-on GT911 polling change was prepared locally on
`main` in `D:\21cncstudio\project_aura\tmp\worktrees\aura-post-115-clean`.
Its immediate baseline was `f7eff122a13d370686ee8d8850007b4837fabb5e`.
This implementation has
not been flashed, installed on either board or validated on hardware. It has
not been pushed, published or packaged as a release.

The change does not alter the selected GT911 addresses, I2C frequencies,
power control or wiring. The 4.3-inch production profile remains at `0x14` and
the 7-inch dual-I2C production profile remains at `0x5D`, as established by
the preceding `f7eff12` candidate. The tested BINs and release packages from
earlier checkpoints remain separate and must not be overwritten.

## Implemented behavior

The LVGL input callback remains at 40 ms. The policy only decides whether a
callback needs an I2C access; it does not slow LVGL input scheduling itself.
The screen-on sequence is:

- During a press, after an error or IRQ exit, and for 350 ms after an explicit
  release frame, the GT911 status is checked every 40 ms.
- After that 350 ms fast tail, status checks run every 80 ms.
- At 1000 ms from the explicit release, the code performs one final
  status-aware boundary read before it can arm IRQ-led idle. A press or error
  discovered at that boundary prevents arming. Only `NoData` or an explicit
  release can enter idle, and only when the controller's interrupt
  configuration has been read and verified as a supported edge mode. While
  idle, the callback skips I2C until an IRQ is latched or the 200 ms
  status-only fallback is due.
- If IRQ registration or arming fails, or a fallback finds a press without a
  corresponding IRQ, the boot switches to sticky `polling_only` fail-safe.
  That mode retains 40 ms status polling and is not re-enabled automatically
  later in the same boot.

The ISR remains a bounded latch. It performs no I2C, LVGL work, wait or vendor
driver call. An IRQ-selected read leaves idle mode even if the GT911 ready bit
has not appeared yet; the next 40 ms callback can then observe the frame. This
avoids turning an early edge into a false release. A fallback read that races
with a newly latched IRQ is reconciled as an IRQ exit rather than reported as
a missed interrupt.

The implementation reads GT911 register `0x804D` and uses bits 1:0 as follows:

| Mode | GT911 setting | Direct idle IRQ |
| --- | --- | --- |
| `0` | Rising edge | Supported after verification |
| `1` | Falling edge | Supported after verification |
| `2` | Low level | Disabled; 40 ms polling path |
| `3` | High level | Disabled; 40 ms polling path |

Level-triggered modes are deliberately not used by this first implementation.
They select `polling_only` without by themselves setting the diagnostic
`fail_safe` flag. An unreadable or invalid configuration selects the sticky
safe polling path.

Each adaptive read first checks coordinate status register `0x814E`:

- Ready bit clear means `NoData`. It is not accepted as release evidence, and
  the full vendor read/status-clear operation is skipped.
- Ready with point count zero means an explicit release. The vendor read runs
  to consume and clear the frame.
- Ready with one to five points means a press. The vendor read obtains the
  coordinates and clears the frame. If this raw pressed snapshot is followed
  by a vendor result with zero points, the vendor result is treated as the
  newer release frame rather than an error.
- A ready frame with more than five points is malformed. It is cleaned up but
  treated as an error, never as input.

This distinction prevents an ordinary no-frame poll from manufacturing a
release and prevents repeated no-data samples from qualifying the idle state.

## Blocking, wake and lifecycle behavior

Dark-screen touch wake remains a separate policy. If an IRQ arrives before the
GT911 ready bit and the first status read returns `NoData`, the policy schedules
one deferred retry on the next 40 ms LVGL callback. A wake caused by touch must
still observe an explicit release before input is admitted, so the waking
contact cannot also click a UI control.

Startup and non-touch blocks may finish through a bounded quiet fallback only
when the cached state was already released and no press was seen. With verified
IRQ polarity, the INT line must also be inactive. In `polling_only`, when IRQ
polarity is unknown, two `NoData` samples at least 40 ms apart are accepted as
the bounded quiet evidence for a non-touch block or startup. This exception
does not apply to `TouchWake`, which always requires an explicit release. An
active verified INT, press or error prevents the quiet shortcut. The touch
lifecycle masks the IRQ around startup suppression, backlight changes, OTA
blocking, recovery and disable/teardown transitions.

## Diagnostics

`/api/diag` now includes `display.touch_polling` with these fields:

- `mode`
- `irq_registered`
- `irq_armed`
- `irq_config_verified`
- `irq_config_mode` (`null` until known, otherwise `0` through `3`)
- `idle_enabled`
- `idle_active`
- `fail_safe`
- `status_reads`
- `full_reads`
- `skipped_callbacks`
- `idle_entries`
- `irq_exits`
- `fallback_probes`
- `missed_irq_presses`
- `irq_arm_failures`
- `irq_no_frame`

These are diagnostic counters and state, not proof of electrical IRQ quality,
touch responsiveness or reduced bus utilization.

## Validation completed so far

The relevant native bundle passed 80 test cases across the GT911 runtime
policy, adaptive screen-on policy, existing dark-screen wake policy, LVGL
framebuffer policies and diagnostic JSON serialization. The saved report is:

`D:\21cncstudio\project_aura\tmp\worktrees\aura-post-115-clean\.pio\native-tests\reports\20260901T113743Z-f20e8598\00-pio.json`

Local PlatformIO production builds also completed successfully after the
current source changes for:

- `project_aura` (4.3-inch, GT911 `0x14`): RAM 158956 bytes, flash
  4325018 bytes.
- `project_aura_7` (7-inch dual-I2C, GT911 `0x5D`): RAM 158956 bytes, flash
  4325494 bytes.

The same final source also passed the full native suite: 899 of 899 test cases
across 104 suites, with no errors or failures. The saved full-suite report is:

`D:\21cncstudio\project_aura\tmp\worktrees\aura-post-115-clean\.pio\native-tests\reports\20260901T114020Z-2ff7f8e3\00-pio.json`

The generated files under `.pio\build` are local build outputs, not signed or
qualified release artifacts. Native tests and successful compilation do not
establish physical touch behavior, cold-start reliability or long-run screen
stability.

## Hardware qualification still required

After a separate flashing authorization, qualify the exact new binaries on
both profiles without changing wiring, power or I2C settings. At minimum,
check short taps after a long idle, the first tap after an IRQ exit, rapid
taps, stationary holds, page buttons, release, sleep/wake, software restart
and cold start. Watch the diagnostic mode and counters while separately
recording what is physically seen on the display and touch panel. The 7-inch
candidate still requires the agreed long screen/touch run.

No hardware PASS from `f7eff12`, `019d87b` or an earlier diagnostic build
transfers to binaries containing this change. The rare one-second CO `--`
observation and the rare vertical display movement remain unconfirmed,
separate observations; this touch-polling work does not establish a cause or
fix for either one.

This note does not authorize flashing, reset, serial access, COM8 use, push,
publication or deployment.
