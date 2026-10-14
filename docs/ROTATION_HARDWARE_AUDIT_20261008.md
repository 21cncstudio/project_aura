# 7-inch orientation without software pixel rotation

> Follow-up correction, 2026-10-08: the owner excludes board changes. See
> "Follow-up: direct rendering without board changes" below. The earlier
> hardware-only next step is superseded by a concrete LVGL blend-hook candidate;
> direct rendering on Aura is not yet implemented or validated.

Date: 2026-10-08. Active checkout: firmware-main, main based on 2dddfa2.
Research only; no display-source change, new build, flash, reset, commit or push.

## Accepted requirement and result

The owner clarified that the objective is **no software pixel rotation**, and
a restart when pressing 180 degrees is acceptable. The earlier proposal to
integrate adapter triple-partial CPU rotation does not meet that objective.

For the stock Waveshare ESP32-S3-Touch-LCD-7 wiring, no supported firmware-only
hardware orientation command was found. Restarting does not change the wiring.
There is a concrete hardware route on the published **V1.2** schematic:
the LCD exposes L/R and U/D, strapped with resistors. The actual device's PCB
revision, population, and panel response have not been physically verified.

## Official evidence

- [Waveshare board resources](https://docs.waveshare.com/ESP32-S3-Touch-LCD-7/Resources-And-Documents)
  links the schematic and ST7262 datasheet used here.
- [ST7262 datasheet](https://files.waveshare.com/wiki/common/ST7262.pdf):
  page 45 maps HDIR/VDIR hardware pins to register 19h bits 5/6, with software
  register priority. Page 60 describes horizontal and vertical scan direction.
  Thus the IC itself does support hardware reversal. It is incorrect to say
  that ST7262 has no configurable registers.
- [Waveshare ESP32-S3-Touch-LCD-7 V1.2 schematic](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-7/ESP32-S3-Touch-LCD-7-Sch.pdf):
  page 1, J1 pins 39 (L/R) and 40 (U/D). L/R is strapped to LCD_VDD via R7=0;
  R6 is the unpopulated ground alternative. U/D is strapped to LCD_VDD via
  R8=0R; R9 is the unpopulated ground alternative. Neither net is connected
  to an MCU/CH422G output on this schematic.
  J1 exposes no LCD I2C/SPI control interface; the separate 6-pin touch
  connection's SDA/SCL are for the touch controller.
- [Espressif RGB LCD guide](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/lcd/rgb_lcd.html)
  distinguishes LCD command-based mirroring from software RGB mirroring.
  The plain RGB example marks mirror and swap_xy as software operations.

The schematic and full relevant datasheet pages were visually reviewed from
locally rendered PDFs, including a high-resolution J1 crop. Download URLs and
SHA256 are saved in ../build-idf-rotation-hardware-20261008/evidence.json.

## Why the apparent firmware alternatives do not meet the requirement

Current local IDF 6.1 source:
../tools/idf61-sdk/esp-idf/components/esp_lcd/rgb/esp_lcd_panel_rgb.c
- rgb_panel_mirror / rgb_panel_swap_xy set rotate_mask.
- rgb_panel_draw_bitmap applies that mask in the CPU-copy branch.
- Passing an existing panel framebuffer takes the framebuffer-selection path;
  it does not reverse the DMA scan of the pixels.
- The S3 LCD HAL byte-order operation changes byte order inside a pixel; it
  does not reverse the sequence of pixels/lines. No supported reverse-scan
  setting is exposed by the reviewed RGB driver/HAL.

Current Display Panel LCD_ST7262 delegates to esp_lcd_new_rgb_panel; Aura's
ESP_PANEL_BOARD_LCD_RGB_USE_CONTROL_PANEL=0. A successful mirror API return
therefore does not establish hardware-only rotation on this board.

A community driver was checked as a candidate:
https://github.com/chipguyhere/ESP32-S3-Touch-LCD-7_display
Its README calls 180-degree rotation inexpensive, but the actual
chipguy_ESP32S3_Touch_LCD_7_display.cpp reads PSRAM forward and reverses writes
into the SRAM bounce buffer. The code under the 180-degree rotation branch
still copies/reorders pixels with the CPU during scanout. This is not the
requested hardware-only solution, regardless of the performance claim.
It was downloaded for inspection only, not executed or integrated.

EEZ exports widgets and assets. Changing layout coordinates alone does not
rotate text and bitmap contents. A whole-screen LVGL transform or adapter
rotation still needs image processing; it is not a hardware scan-direction
setting. No applicable maintained direct inverted-render solution was found.

## Hardware route and 180-degree button design

| Hardware | Output with no CPU pixel rotation | Meaning of the 180-degree button |
| --- | --- | --- |
| Unmodified board | Native panel orientation only | Cannot offer both orientations without software rotation |
| Fixed L/R + U/D strap reversal | Candidate for a permanently reversed native orientation | Opposite orientation would need software rotation or another hardware change |
| L/R + U/D connected to suitable controlled outputs | Candidate for both hardware orientations | Save orientation, restart, apply hardware direction before showing the UI; map touch accordingly |

The V1.2 drawing suggests moving the two direction straps from their high
options to their low options would reverse both axes. This is a schematic-based
candidate, **not an instruction to modify the current device without checking
its revision, actual component population and LCD behavior**. Do not populate
both the high and low 0-ohm options simultaneously.

For the controlled-output variant, first select/verify electrical connections
and startup levels; no particular spare pin has been assumed available.
Then:
1. Maintain a logical user orientation separate from the hardware base orientation.
2. On button press, save the requested setting durably. If saving fails, keep
   the existing setting and do not restart.
3. Use the existing orderly restart path. On boot, set both hardware direction
   signals while the display is hidden and at the verified point in LCD startup.
4. Start native-orientation LVGL output without a rotation copy; transform touch
   coordinates to the selected physical orientation.
5. Verify both orientations and startup/failure paths on the actual 7-inch board.

This design is conditional on hardware support and has not been implemented.
A restart alone on the unmodified board offers no benefit toward the goal.

## Preserved workspace

The just-started adapter overlay scaffold was removed from active source and
dependency configuration and retained, unbuilt, at:
../build-idf-adapter-audit-20261008/paused-runtime-integration/

The prior official esp_lcd_touch / GT911 1.2.1 migration is retained.
All 25 files in its saved source manifest still match their SHA256.
src/lvgl_v9_port.cpp has no diff from HEAD. Existing user UI edits remain intact.
No new firmware test/build is claimed: executable firmware source is unchanged
relative to the previously tested touch candidate.

Next step: owner decision on a hardware modification. Before such work, identify
the actual board revision and direction-strap population. Until then keep the
existing display implementation and do not resume software-rotation integration
as if it satisfied the clarified requirement.


## Follow-up: direct rendering without board changes

2026-10-08, follow-up audit after the owner explicitly excluded soldering and
board modifications. This section supersedes the earlier "Next step: owner
decision on a hardware modification." Hardware scan-direction control and
direct software rendering into the desired orientation are different routes.
The earlier hardware-only conclusion did not rule out the latter and must not
be used to claim that the user's requested result requires changing the board.

### Requirement

For the existing 7-inch ESP32-S3 / RGB / ST7262 setup, produce the 180-degree
image during rendering, without first producing an ordinary complete frame and
then reversing it into a separate full-frame rotation destination.
Ordinary display buffers, RGB bounce buffers, and bounded intermediate widget
layers are distinct from that extra rotation pass. This does not promise zero
CPU work, zero copying everywhere, or a bufferless RGB display.
A restart when switching the saved 0/180 setting is acceptable.

### Reverified current implementation and previous work

- Main is at 2dddfa2f92a7a7d092d6d5aad696957d9b217c3a with local changes.
  src/lvgl_v9_port.cpp still has no diff from HEAD.
- lvgl_port_copy_frame_180() reverses all 384,000 RGB565 pixels. Its call in
  flush_callback() runs after the final dirty area of each UI refresh when
  screen_flip_180 is enabled, not on every independent LCD scan.
- This copies 768,000 bytes and entails 1,536,000 bytes of logical source reads
  plus destination writes per refresh, excluding rendering, cache and scanout.
  The flipped path assigns one full framebuffer to LVGL and two to alternating
  scanout. Buffers must not be reduced without redesigning that ownership.
- The default for 7-inch is already flipped before UI creation; setting the
  default earlier does not change how pixels are produced.
- main/idf_component.yml pins official esp_lcd_touch and GT911 to 1.2.1.
  The old bundled C implementations are excluded through the Display Panel
  overlay, including the adaptation preserving single-consumption semantics.
  This was preparation for the official adapter's new touch API, not a
  rotation optimization and not a prerequisite for direct native rendering.
- Rehashed all 25 inputs in build-idf-touch-api-20261008/source-manifest.json:
  zero mismatches. Both saved candidate BIN hashes match the touch migration
  report. Existing test logs were inspected; no tests or builds were rerun here.
  Adapter 0.7.2 remains an inspected/probed dependency, not active firmware.
  The previous report records no flashing; this audit accessed no devices.

### A concrete software route exists

The actual Aura LVGL 9.6 source exposes lv_draw_sw_register_blend_handler().
src/draw/sw/blend/lv_draw_sw_blend.c calls that handler before calculating the
ordinary destination address. An RGB565 handler can blend each final pixel
directly at the target orientation's address, while retaining logical widget
layout, font layout and clipping.

For a full 800 x 480 framebuffer without padding, the target pixel address is:

    framebuffer[(479 - y) * 800 + (799 - x)]

This maps the destination during drawing; it does not require reading back
the completed unrotated frame. A real implementation must account for stride,
buffer/layer origins and which destination is currently safe to write.

An existing related implementation was found and read in the Hub project:
D:/21cncstudio/aura-aq-hub/main/hub_native_blend.c and .h.
Its documented host feasibility experiment is in
D:/21cncstudio/aura-aq-hub/docs/development/scroll-demo-review-2026-09-27.md,
with retained evidence at build/scroll-demo-review-20260927/native-probe-result.json.
That saved result reports LVGL 9.5, RGB565, 90-degree partial output, six scroll
states with zero pixel differences, 96 reference rotation calls and zero
candidate rotation calls. It is prior host-only evidence, not a new run,
7-inch Aura validation, or measured hardware speedup.
The Hub implementation is explicitly version-guarded for LVGL 9.5 and cannot
be copied unchanged into Aura 9.6.

Important integration limits:
- Only the selected root display destination is mapped; intermediate layers
  and unrelated RGB565 buffers retain their ordinary layout.
- Audit operations bypassing the generic blender, including buffer clear/copy,
  transforms, blur/vector paths if enabled, and snapshots.
- In LVGL DIRECT mode, lv_refr.c synchronizes dirty rectangles between display
  buffers using ordinary coordinates. That synchronization must use the same
  physical mapping (the inspected version has a sync callback extension);
  changing only the blender would leave a correctness hole.
- Retain acknowledged frame ownership, bounded completion waits and cache
  handling. Never render into the active scanout buffer.
- Preserve the logical touch mapping and prevent a held touch from becoming
  a click on another control after orientation changes.

### Options against this requirement

| Option | Meets immediate oriented rendering without a separate rotation pass? | Assessment |
| --- | --- | --- |
| Custom destination mapping through LVGL's existing blend hook | Candidate: yes | Most relevant next experiment; validate against actual Aura 9.6 UI |
| Adapter CPU rotation / small partial buffers | No | Can reduce rotation work to changed regions, but still renders then rotates pixels; optional fallback only |
| Rotate a root LVGL object / redesign EEZ coordinates | Not by itself | Ordinary style transforms can create an intermediate layer; coordinates alone do not rotate glyphs and images |
| RGB mirror API / startup flag / restart alone | No in the inspected current path | RGB mirror sets a software copy mask; existing framebuffer scanout does not become reversed |
| Rotate inside the RGB bounce-buffer callback | No | Moves pixel reversal into continuous scanout; not direct oriented rendering |
| Reverse a completed frame in place | No | Saves a separate rotation destination in some designs but retains pixel reversal and complicates incremental redraw and ownership |

The ordinary lv_display_set_rotation() changes display geometry/state; it does
not implement immediate software rasterization at the selected orientation.
The current build has matrix transforms disabled; no applicable turnkey matrix
renderer was established by this audit. ESP32-P4 PPA acceleration from Hub is
not an available PPA peripheral on this ESP32-S3 board.

### Recommended next bounded work

Prepare an isolated host prototype using Aura's exact LVGL 9.6 and real UI
assets, compare normal render followed by reference 180-degree reversal against
direct mapped rendering, and require pixel equality without a candidate
rotation call. Cover both orientations, text/icons, masks/alpha, scrolling,
small incremental updates, intermediate layers and buffer guards.

A full-refresh two-buffer prototype is a simpler correctness starting point,
not a presumed performance winner. Before retaining DIRECT incremental mode,
validate mapped dirty-region synchronization. Only after correctness is proven
should the candidate be integrated and timed on an explicitly authorized
7-inch board. Memory savings and speed gains remain unmeasured.

For the 180-degree button: save the logical orientation, use the orderly restart
path, select the renderer mapping before the first visible UI, and apply the
matching touch mapping. This needs no board modification. The restart merely
simplifies switching ownership; the new renderer is what removes the extra pass.

No executable source was edited, no new candidate was built, and no device,
commit, push or publication action was performed by this follow-up.

### Primary sources checked in this follow-up

- [LVGL 9.6 blend dispatch](https://raw.githubusercontent.com/lvgl/lvgl/v9.6.0/src/draw/sw/blend/lv_draw_sw_blend.c)
- [LVGL 9.6 rotation documentation](https://github.com/lvgl/open-docs/blob/main/9.6/src/main-modules/display/rotation.mdx)
- [Official adapter 0.7.2](https://components.espressif.com/components/espressif/esp_lvgl_adapter/versions/0.7.2/readme)
- [Espressif RGB LCD guide](https://docs.espressif.com/projects/esp-iot-solution/en/latest/display/lcd/rgb_lcd.html)
- Local exact IDF 6.1 esp_lcd_panel_rgb.c, Aura managed LVGL 9.6 source, current
  framebuffer code, and Hub native-blend source/evidence listed above.

## Host prototype result, 2026-10-08

The owner authorized trying direct 180-degree rendering. The isolated experiment
is implemented in tools/probe_native_rotation.py and tools/tests/native_rotation/;
it is excluded from the firmware target (which collects sources under src/).

Compiled 510 translation units including unmodified managed LVGL 9.6, real Aura
assets/screens and the test controls. All four candidate configurations passed
102 frames each: 0-degree DIRECT, 180-degree FULL, 180-degree DIRECT, and
180-degree DIRECT with padded rows. Total 408 full-frame pixel comparisons,
zero differences, zero candidate calls to lv_draw_rotate, intact framebuffer
and padding guards. DIRECT uses a physical-coordinate sync callback and two
screen buffers, with 235 observed synchronization operations. Ordinary damage
copies still exist; the separate raster-rotation pass does not.

A deliberately incorrect sync callback failed at frame 16 with 20,816 differing
pixels, demonstrating that the incremental comparison detects this integration
error. These are software feasibility results, not an ESP32 speed/memory or
physical display qualification. The simple callback uses stock one-pixel color
kernels and requires optimization and production integration.

An additional shutdown check exposed a separate canvas image-cache error in the
unmodified LVGL source. A deterministic minimal reproduction without the native
renderer confirms it. Render comparisons use isolated process-lifetime fixtures
and do not claim teardown validation. No library source was patched to obtain
the pixel results. See the test README for the exact limitation and evidence.

[Reproduction, coverage, controls and limitations](../tools/tests/native_rotation/README.md).
Evidence: build-idf-native-rotation-20261008/result.json and adjacent logs/captures.
Main raw framebuffer captures were visually inspected. All 25 inputs of the prior
touch candidate still match its source manifest. Working firmware source, vendor
sources, dependencies, EEZ, devices and remote refs remain unchanged.

Next: production integration of the direct write path and physical-coordinate
buffer synchronization, with both profile builds and then a separately authorized
7-inch hardware test. Keep the cleanup finding explicit; this prototype alone
is not a firmware candidate or proof that the display becomes faster.


## Firmware integration follow-up, 2026-10-08

The native 7-inch renderer and saved-orientation restart button are now in the
local test firmware; both profiles build and the exact module passes the host
pixel comparison. See [integration report and hashes](NATIVE_ROTATION_INTEGRATION_20261008.md).
No board modifications or device actions were performed; hardware speed and
touch/button acceptance are still open.
