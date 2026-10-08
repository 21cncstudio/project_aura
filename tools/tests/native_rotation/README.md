# Native 180-degree rendering differential test

2026-10-08. The fixture is host-only, but now links the same `src/display/NativeRotation.cpp` as the 7-inch firmware candidate. See [integration/build evidence](../../../docs/NATIVE_ROTATION_INTEGRATION_20261008.md).

Run from firmware-main with the existing Python and MinGW GCC/G++:

```powershell
python tools/probe_native_rotation.py
```

The runner uses the actual managed LVGL 9.6.0 sources and Aura generated
screens, fonts and images. The host configuration includes lv_conf_idf9.h via
the existing lvgl9_ui fixture; only the allocator is changed to the C library
and assertion failures abort instead of hanging the test process.
No dependencies are downloaded and no devices are accessed.

## Reason for a custom experiment

The online/source audit in docs/ROTATION_HARDWARE_AUDIT_20261008.md found that
Espressif's adapter still rotates completed pixels on ESP32-S3. It therefore
does not satisfy immediate oriented rendering. LVGL provides a supported blend
registration hook, and the Hub project has a related LVGL 9.5 / 90-degree proof.
This fixture uses that extension with Aura's exact LVGL 9.6 / 180-degree geometry;
it does not patch or replace managed library sources.

The callback maps only the two selected RGB565 display buffers. Other layer
formats are handled by their normal LVGL renderer. Flat fills use stock span kernels; source color and alpha
arithmetic for masks/general images uses stock single-pixel kernels. Opaque
RGB565 source images use direct stores. Hardware performance remains unmeasured.
Logical coordinates remain unchanged. A DIRECT-mode sync callback maps damage
rectangles into physical coordinates before ordinary row copies. There is no
separate candidate rotation buffer or whole-frame rotation pass. The oracle
and capture code have their own full-frame arrays; they are test equipment,
not a proposed embedded memory layout.

## Results

All four candidate runs matched the unmodified reference frame sequence:

| Candidate | Frame comparisons | Rotation calls in candidate | Result |
| --- | ---: | ---: | --- |
| 0 degrees, DIRECT | 102 | 0 | exact pixel equality |
| 180 degrees, FULL | 102 | 0 | exact pixel equality |
| 180 degrees, DIRECT | 102 | 0 | exact pixel equality |
| 180 degrees, DIRECT, 32 bytes row padding | 102 | 0 | exact pixel equality |

Total: 408 comparisons / 156,672,000 RGB565 pixel comparisons, zero differences.
The reference oracle uses the actual LVGL 9.6 lv_draw_rotate() function.
Rotation calls, draw-unit buffer acquisition and sync completion are observed with linker
wrappers. Assertions check that the current displayed buffer is not handed to
a drawing task and is not written by the candidate blender. Buffer and row
padding guards stayed intact. The DIRECT candidates executed 235 sync calls;
1,030 real intermediate-layer buffer acquisitions were observed. Intermediate
RGB565 destinations are not separately covered by this scene set.

Coverage: all 15 generated screens; repeated main-screen values and visibility;
warmup labels; a mixed scene with scrolling, rounded cards, shadows, gradients,
parent opacity and rotation, moving labels, an arc and a slider; screen return
and a no-dirty-update frame. Synthetic scene changes are deterministic. The
fixture does not run the complete UiController, real sensors or physical touch.

A negative control deliberately omits the physical mapping in DIRECT sync.
It reaches the first main-screen incremental update, then fails at frame 16
with 20,816 differing pixels. This is an expected detected defect, not a passing
image result. Its output/captures are retained separately. A second negative
control attempts a write into active scanout; the production ownership guard
rejects it. Foreign/duplicate acknowledgements and invalid initialization are
also rejected in the normal runs. A second negative
control attempts a write into active scanout; the production ownership guard
rejects it. Foreign/duplicate acknowledgements and invalid initialization are
also rejected in the normal runs.

## Separate existing LVGL cleanup defect

An added shutdown check caught `invalid magic: 0x10` from image cache cleanup.
The backtrace leads through canvas destruction. The local unmodified
lv_canvas_destructor passes `&canvas->draw_buf` while that field is a pointer.
The pointer address is interpreted as an image descriptor. The QR destructor
also destroys its buffer before its canvas ancestor is destructed.

canvas_cleanup.cpp reproduces the invalid descriptor in unmodified LVGL,
without installing the native hook or rendering any Aura screen. It places a
valid draw-buffer descriptor at an address with low byte 0x10 to make the
pointer-to-pointer error deterministic. Both image and header cache cleanup
report the error. The reproduction passes only when those two expected errors
occur and no other error occurs. It is evidence of an existing software issue,
not a device-crash claim or a firmware fix.

The rendering fixture is explicitly process-lifetime: it closes its evidence
files and exits without deleting the display/deinitializing LVGL. The OS reclaims
that process. No rendering warnings/errors are silenced; only expected existing
EEZ deprecated-API warnings are allowed. Rendering result.json explicitly says
that teardown was NOT validated. Keep this issue separate from the rotation
candidate; resolve library lifecycle support before claiming production readiness.

## Evidence and boundaries

Default output: `build-idf-native-rotation-integration-20261008/host/`.
result.json records compiler versions, source/configuration hashes, executable
and runner hashes, results and counters. Original prototype evidence and the
shutdown backtrace remain under `build-idf-native-rotation-20261008/`.

The integrated renderer has now passed both IDF builds. No devices were accessed.
Actual DMA/VSYNC ownership, cache coherency, scheduling, button/touch/restart
behavior and ESP32-S3 performance remain unvalidated. The tests cover the fixed
800x480 one-worker software configuration, not arbitrary LVGL. The full
UiController is not executed on host. See the integration report for exact BINs,
the restart policy tests and the hardware acceptance steps.
