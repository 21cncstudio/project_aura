# Native 180-degree firmware candidate — 2026-10-08

Local integration on `main`, base `2dddfa2f92a7a7d092d6d5aad696957d9b217c3a`.
Both profiles build. No flash, port access, device reset, commit, push or publication.
These are uncommitted test candidates, even though the existing build metadata
calls the standard firmware flavor `production`. The dirty build ID alone does
not identify this change; use the artifact hashes below.

## Behavior and implementation

- Native IDF **7-inch only**, RGB565 800x480 / LVGL 9.6.0, one software draw unit.
  `src/display/NativeRotation.cpp` registers the official blend hook. Root pixel
  writes go directly to physical 0/180-degree coordinates. Intermediate layers
  retain normal coordinates. Flat fills use stock span kernels; masked fills and
  general images use stock scalar kernels. Opaque RGB565 images have a direct
  store path. No additional rotated-frame or scratch-row buffer is allocated.
- Two ordinary RGB screen buffers replace three. One renders; the driver reads
  the other. This removes one 800x480x2 allocation (768,000 bytes / 750 KiB).
  This is a source/configuration reduction, not measured device free heap.
- DIRECT-mode damage synchronization maps rectangles and copies matching physical
  rows between buffers. Those normal coherence copies still exist. There is no
  separate full-frame reversal. The 7-inch ELF contains the native renderer and
  neither `lvgl_port_copy_frame_180` nor `lv_draw_rotate`.
- Initial scanout remains IDF framebuffer zero; LVGL starts in framebuffer one.
  The port publishes native ownership only after its existing bounded driver
  completion wait succeeds. Invalid draw/sync/flush ownership fails the display
  task closed. IDF 6.1 RGB bounce completion updates `bb_fb_index` to
  `cur_fb_index` before invoking `on_frame_buf_complete`; hardware timing still
  needs verification on this panel.
- Saved `screen_flip_180` is passed before the LVGL task/UI starts and also drives
  existing touch coordinate inversion. No coordinate/layout changes in EEZ.
- The **180-degree button remains**. It saves the new value synchronously, disables
  itself, and requests the existing orderly restart (250 ms feedback delay).
  Failed save restores the old value and does not request restart; repeated
  requests cannot toggle it again. OTA busy blocks the change. After reboot,
  the chosen orientation is applied from the first rendered frame. The live
  setter only accepts the already active orientation on this profile.
- 4.3-inch retains its three buffers, previous software-copy flip and immediate
  button behavior. Its ELF does not link the new renderer. The updated GT911
  1.2.1 dependencies remain intact; the official LVGL Adapter is not integrated.

The alternative/source audit and why the adapter does not meet this goal are
in [the rotation audit](ROTATION_HARDWARE_AUDIT_20261008.md#follow-up-direct-rendering-without-board-changes).
No board changes are required by this candidate.

## Verification

- `python tools/probe_native_rotation.py`: compiles the **same production renderer**
  with actual managed LVGL and all 15 generated Aura screens/fonts/images.
  408 frame comparisons (156,672,000 pixels), zero differences versus normal
  LVGL plus its reference rotation. Modes: 0 DIRECT, 180 FULL, 180 DIRECT,
  180 DIRECT with padded row stride. No candidate rotation calls; 235 syncs per
  DIRECT run. Guards and intermediate layer observations passed.
- Two deliberate negative controls: wrong DIRECT rectangle mapping fails at
  frame 16 with 20,816 different pixels; writing to active scanout is rejected
  by the actual production guard before UI rendering.
- Native tests: 19 display-wait/rotation-setting/diagnostic cases; 51 existing
  framebuffer/restart/storage cases; 5 rotation/default cases with the 7-inch
  profile define. Total **75 passing cases**. Failed saves, both directions,
  repeated presses and a subsequent boot are covered. This tests the policy,
  not a physical press or real flash-storage power failure. The launcher blocked
  an overlapping invocation; the 7-inch suite was then run separately and passed.
- Both final IDF builds passed and their release artifact stamps match the BINs.
  The first 7-inch build exposed an unused old-copy helper warning; that helper
  is now excluded by preprocessing and the final build passed after the fix.
- `python tools/eez_ui_postprocess.py --check` passed. All 25 files in the prior
  touch-candidate manifest and both archived touch BIN hashes remain unchanged,
  including the user's pre-existing generated UI/EEZ edits.

## Exact local artifacts

Root: `build-idf-native-rotation-integration-20261008/`.
Each `candidates/<profile>/` contains firmware.bin, matching ELF, build identity,
artifact stamp, compiled sdkconfig.h and selected symbols. `artifacts.json` and
`source-manifest.json` bind the hashes and local inputs. Original edited files
are retained under `before/`. Previous host-only evidence stays separately in
`build-idf-native-rotation-20261008/` and has not been overwritten.

| Profile | Bytes | firmware.bin SHA256 |
| --- | ---: | --- |
| 7_dual_i2c | 4646432 | `282d4caa57e629a7ddea64c979d4fae59f3ac45ee02b263e59851666e99093f1` |
| 4_3 | 4643872 | `9fd31b5da1cb40939adb4bab321e592efc7e6d69802042bfcfa5ee896fce0b46` |

## Remaining device check

Hardware smoothness/frame latency, watchdog headroom, cache/DMA timing, boot
display, real touch targets and the button/restart cycle have not been tested.
Scalar masked/image blending may affect CPU cost; host timing is not an ESP32
benchmark. Before any separately authorized flash, identify the exact 7-inch
board, USB/MAC, running profile/slot and candidate hash; preserve settings and
other partitions. Then check 180 -> 0 -> 180, corners/controls, dirty updates,
screen transitions, sleep/wake, free PSRAM, display task stack and `/diag`.

Expected 7-inch diagnostics: `native_rotation_renderer=true`,
`framebuffer_count=2`, requested `screen_flip_180`,
`rotation_pipeline_active=false` (the old copy pipeline),
`rotated_copy_switch_count=0`; ownership violations and wait timeouts must remain
zero while handoff counts advance. None of these device readings is claimed yet.

The separate unmodified LVGL canvas destructor defect remains reproduced by the
host control. It is not fixed here, and rendering-fixture teardown is not a pass.
The button uses the established reboot path; runtime RGB deinit/reinit remains
unsupported. Rare formats/effects and RGB565 intermediate destinations outside
the captured scenes are not claimed as exhaustively validated.
