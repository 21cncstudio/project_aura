# SD recovery candidate, 2026-09-28

Local implementation for both ESP-IDF 6.1 profiles. The working tree also contains
the previously prepared SEN0321 changes. Preserve the separate SEN0321 tester BINs.
No device flash, reset, card format, message, commit, push or publication is part of this change.

## Problem and evidence boundary

A runtime SD fault could leave the filesystem mounted but unusable. The old daily
history retry reopened files, without reinitializing SDSPI/card/FatFs. A power-cycle
report suggests recovery is useful; it does not identify a defective slot, bad
power, SPI frequency or a firmware root cause. errno ENODEV combines several FatFs
results. SDK mount errors are no longer suppressed; serial logs remain necessary
for the underlying FatFs/SD command result when the VFS helper collapses errors.

## Behavior

- Two consecutive recoverable I/O failures schedule recovery. Successful I/O resets
  the trigger, except that reads cannot erase an active write failure.
- Initial mount transport errors and failed recovery mounts retry after 5, 30,
  60 seconds, then 5 minutes. Generic mount/FS errors need manual retry. Busy,
  missing file, access-denied and full-card errors do not trigger an unmount.
- A low-priority task performs teardown and reinitialization. Pending/active
  recovery closes admission to new file operations. An existing file lease, such
  as a CSV download, is allowed to finish; recovery defers by one second without
  counting a hardware attempt. Readiness is checked again after taking the lease.
- Card/bus cleanup failures stop automatic recovery. An unknown already-initialized
  SPI bus is not adopted. Card pointers are never used by the status endpoint;
  card geometry is copied under the status mutex.
- OTA pauses dispatch of new attempts. An already-running bounded attempt may
  finish. Restart waits for it before shutdown. The panel I2C fault-domain drain
  includes this new CH422G user. Recovery does not reset the board or the I2C bus.
- CH422G output writes share a per-Base mutex, protecting the output register's
  read/modify/write across backlight and SD chip-select changes.
- Daily history is not reinitialized: current samples, A/B generation, pending
  days, and cleanup intent remain in RAM. Remount schedules a save promptly.
  Mounted/awaiting-save is distinct from a successful state/CSV write.
- A different card is not automatically merged into the current RAM history.
  If boot had no card, a late inserted card with an existing Aura history directory
  is likewise held for normal boot-time restore. Export current day before restart.
  Putting the original card back and pressing Reconnect is supported. The same
  guard also covers a failed initial snapshot read: matching CID alone does not
  prove RAM contains the old history. Successful restore or a committed history
  write explicitly acknowledges the current media session.
- A blank card inserted after boot is supported. No automatic formatting is enabled.
  RAM-only measurements still depend on power, and the pending-day queue is bounded.

## Web/API

`GET /api/history/daily/status` remains a status refresh. The new
`POST /api/history/sd/reconnect` returns 202 for an accepted request, 409 when
unavailable, and the existing OTA 503 response while busy. No destructive action
is attached to GET. Reconnect does not require an erase confirmation.

Status adds readiness, pending/active/paused/blocked recovery, next retry,
attempt/remount counts, first incident error (domain, code, operation, stage,
uptime), and last successful write age. The dashboard disables SD actions during
recovery and leaves current-day RAM export available.

## References and deferred experiment

- [Espressif SDSPI example](https://github.com/espressif/esp-idf/blob/master/examples/storage/sd_card/sdspi/main/sd_card_example_main.c)
- [SDSPI concurrency](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/sdspi_host.html)
- [FatFs mount invalidates open objects](https://elm-chan.org/fsw/ff/doc/mount.html)
- [Waveshare SD example](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-7/blob/main/examples/Arduino/examples/03_SD_Test/03_SD_Test.ino)

SPI stays at 20 MHz. A 4 MHz comparison on the problematic unit is a separate
controlled experiment, not a confirmed fix or a change in this candidate.

## Validation

Host tests cover real manager file leases with a fake hardware boundary, deferred
CSV recovery, cleanup failure, pause/restart behavior, error classification,
backoff including clock wrap, card-history admission, and RAM/daily CSV preservation.
Dashboard tests exercise the actual embedded functions, action failure, duplicate
submission, OTA and recovery status. These do not replace physical removal/contact,
power-interruption, FAT corruption, long CSV, and soak tests on both display profiles.

Initial local validation on 2026-09-28 (candidate superseded by the review fixes below):

- 143/143 native cases passed across 11 selected suites; Unity source identity
  was verified for each suite. Report: `.pio/native-tests/reports/20260928T171433Z-87e8e02d/00-pio.json`.
- All 44 JavaScript tests passed, including seven SD dashboard tests.
- Both `tools/build.ps1 -Line main -Profile 4_3 -Jobs 4` and profile `7_dual_i2c`
  passed. Embedded target/flavor, ESP checksum/hash, new I2C linkage and restart
  ordering checks passed. Existing unused NetworkManager helper warnings remain.
- Browser review used a local SD panel fixture with production markup/functions:
  desktop and a 375 px content viewport, long error wrapping, gated SD controls,
  and available RAM export. No horizontal overflow. This is a fixture check,
  not a live-device HTTP or hardware test.
- `git diff --check` passed. The previous two SEN0321 BIN SHA-256 values are unchanged.

Separate local OTA test bundle: `release-assets/SD-recovery-test-20260928/`.
It contains application-only BINs, per-profile checksum/target verification,
English test instructions, and local evidence. The existing displayed version
remains `1.2.1-beta`; use the full BIN hash to distinguish this unpublished test.
The firmware also includes the previous SEN0321 and other existing working-tree
changes. The exact tracked diff and untracked source are retained in the bundle.

No physical device was flashed or tested. Recovery, touchscreen/backlight behavior
during remount, long CSV transfer, and extended logging need both-profile hardware
validation before any beta publication.


## Review fixes, candidate R2

Use `release-assets/SD-recovery-test-20260928-r2/` for the next local test.
The original SD bundle and SEN0321 bundle are preserved as exact earlier artifacts.

Three review findings are addressed:

1. SD recovery now holds a `WakePowerGuard::Activity` lease through unmount,
   CH422G operations and mount. Dispatch pauses during all guarded wake phases;
   the worker also checks admission after dispatch. A later wake waits for an
   already-running SD operation to drain. The output-register mutex remains.
2. A blank card accepted after an offline boot is explicitly bound to the current
   RAM session before its first successful save. A directory or sidecar left by
   its own failed first save no longer blocks the next reconnect. This binding is
   tied to the accepted CID and does not mark the write as successful. Another
   card, late existing history and a failed boot restore remain blocked.
3. Reconnect SD is available during the automatic backoff, including the five-minute
   interval. Active requests, active reconnect and paused recovery still disable
   it. The backend retains the existing immediate retry and duplicate coalescing.

Regression tests first reproduced five failing native cases and three failing JS
cases before the fixes. Final host validation passed 165/165 cases in 12 suites
and all 46 JavaScript tests. The test hardware boundary now feeds card identity
and history presence into the same media-admission implementation as firmware.
Wake tests exercise every guarded phase, a wake arriving after dispatch, and a
wake arriving during successful or failed recovery. Negative media tests cover
foreign cards, unrestored existing history, and reinsertion of the accepted card.

Native report: `.pio/native-tests/reports/20260928T174329Z-65211961/00-pio.json`.
Per-profile build, image identity and checksum results are retained in the R2
bundle manifest and evidence. No physical-device behavior is claimed from these
host tests. SPI remains 20 MHz; automatic formatting remains disabled.
