# Official touch API migration â€” 2026-10-08

Scope: local general firmware `main`, both `4_3` and `7_dual_i2c`. Base commit
`2dddfa2f92a7a7d092d6d5aad696957d9b217c3a`, with existing unrelated UI/document changes
preserved. No device access, flashing, reset, commit, push or publication.

## Official sources and choice

- [esp_lcd_touch 1.2.1](https://components.espressif.com/components/espressif/esp_lcd_touch/versions/1.2.1/readme):
  new `esp_lcd_touch_point_data_t` and `esp_lcd_touch_get_data()`.
- [esp_lcd_touch_gt911 1.2.1](https://components.espressif.com/components/espressif/esp_lcd_touch_gt911/versions/1.2.1/readme):
  track IDs and the corresponding persistent-sample driver.
- [Display Panel](https://github.com/esp-arduino-libs/ESP32_Display_Panel):
  current 1.0.5 still bundles touch core 1.1.2 and GT911 1.1.1.
- [LVGL Adapter 0.7.2](https://components.espressif.com/components/espressif/esp_lvgl_adapter/versions/0.7.2/readme)
  needs the new touch API. It is an integration candidate, not part of this firmware yet.

Both official touch components are exact manifest/lockfile pins. Their managed C
sources and headers were byte-compared with the official release archives.
Archive SHA256:

- core: `e06bea26320feedec31a099dc1b29205860f80775346739c841ffdf9815034e3`
- GT911: `22ef0133de11cc68a5daa3164a7bc1d2a937ec4b05265cb1ddb569e0fde8dbbd`

## Implementation

The build-generated Display Panel overlay forwards its two bundled touch headers to
the official managed headers. CMake excludes the bundled core/GT911 C sources and
links the two official components. There is no duplicate driver or mixed struct ABI.
Vendor and managed driver sources are unchanged.

The generated `Touch::readRawDataPoints()` uses one array of official point records
and propagates API errors. The public Display Panel `TouchPoint` interface remains
compatible with the existing Aura input path.

Two explicit compatibility adaptations remain in the overlay:

1. Display Panel needs an address-selecting I2C initializer absent from the standalone
   GT911 header. Generate it from the official initializer, changing only the address.
   Preserve 4.3-inch `0x14`, 7-inch `0x5D`, 100 kHz and the command/control format.
2. GT911 1.1.1's `get_xy()` consumed cached points; 1.2.1 retains them for repeated
   `get_data()` calls. After copying points/track IDs, the Display Panel wrapper clears
   the exposed driver point count under its critical-section lock. This retains the
   wrapper's old single-consumption contract, including non-ready/key-frame races.
   Direct use of the official API retains its upstream behavior. Only one runtime
   reader may own this handle; future LVGL Adapter integration must replace that
   ownership rather than poll concurrently with Display Panel.

The cached-point regression was reproduced with a failing test before this adaptation;
the same test now passes. Core and panel capacities both remain five points / five
button slots, with compile-time checks that they agree.

Aura's GT911 startup/address selection, shared I2C gate, retry, interrupt/wake,
release-gate, long-uptime guard, current display rotation and framebuffer logic
are unchanged. The rejected custom partial-rotation experiment was removed.

## Validation

- 41 scenarios execute both unmodified official C drivers and the actual generated
  C++ point-reader method on the host. They cover the two profile addresses, I2C
  initializer, press/release, retained direct-API samples, wrapper consumption,
  non-ready frames, eight coordinate transform combinations, five-point reads,
  bounds/default point count, malformed frames, I2C failures/retry, track-ID failure,
  invalid arguments and key state decoding.
- Seven Python tooling tests pass, including the above executable, complete vendor
  provenance, existing generated I2C adaptations and fault-injecting native transport.
- 96 existing native cases pass across nine GT911/LVGL/touch suites, including the
  24.9-day startup-block regression.
- 12 additional cases pass with the actual 7-inch profile's I2C/probe/flip definitions.
- LVGL Adapter 0.7.2: all seven probed translation units compile against the new
  S3 compile databases for both 4.3-inch and 7-inch profiles. Its upstream rotation/wait host probe passes 1305
  rotation cases and two healthy-wait cases. Missing frame completion still exposes
  an unbounded upstream wait; that remains a separate adapter integration concern.
  These checks do not establish adapter linking, framebuffer ownership or hardware timing.

Both final IDF builds passed the existing binary gates: current build ID,
OTA target/image integrity, retained RTC ABI, native I2C linkage and restart order.
For each ELF, verified exactly one official core and GT911 compilation unit,
one linked `get_data()` and GT911 constructor, Display Panel's reference to the
new API, and absence of deprecated `get_coordinates()` from the linked image.

| Profile | Build ID | BIN bytes | SHA256 |
|---|---|---:|---|
| 4_3 | 2dddfa2-dirty | 4643744 | `5861d6228cdf0e712c68081257783104731942db881805424657364ccbc20425` |
| 7_dual_i2c | 2dddfa2-7-dual-i2c-dirty | 4644096 | `28bee7b78d371a6953c67a15b47e33481dff0e551e32847e7c454623d8313759` |

Saved candidates: `build-idf-touch-api-20261008/candidates/<profile>/firmware.bin`,
with identity, original build artifact stamp and `verification.json`.
Both are local 1.2.1-beta **dirty** candidates, not a new published release.

Exact local changed-source/profile snapshot:
`build-idf-touch-api-20261008/source-candidate.zip`, SHA256
`87263d334b551469150b3936d14dba8d405c8fe823c18481ef89a744734015a9`.
`source-manifest.json` records all 25 input hashes, including existing user UI
changes. Original vendored sources remain byte-identical.
EEZ postprocess check and `git diff --check` pass.


## Evidence and remaining work

Reproduction: resolve the exact managed components via the normal IDF build first,
then run the official-driver test with
`python -m unittest tools.tests.test_native_touch_api -v`. It compiles the managed C
sources and extracts the generated production point-reader method; GCC/G++ are required.
Both complete firmware builds use `../tools/build.ps1 -Line main -Profile <profile>`.
`tools/probe_lvgl_adapter.py` takes the downloaded adapter 0.7.2 source, a profile's
`compile_commands.json`, and a separate output directory. It records upstream hashes
and returns failure when any probed translation unit fails to compile.

Local evidence is under `build-idf-touch-api-20261008/`, including the failing
`wrapper-consumption-before-fix.log`, passing `tool-tests.log`,
`native-tests.log`, `native-7-profile.log`, build logs and adapter probe report.

Host GPIO/IRQ/scheduler calls are mocked. These checks establish software compatibility,
not touch electrical behavior, physical wake, cold boot or visual performance.
Next hardware step requires a separately authorized, sequential test of the two exact
candidates, preserving NVS/partitions and verifying identity, screen/touch, wake and
rotation. Further LVGL Adapter work still needs bounded-wait and buffer-ownership
integration; the rejected custom partial-rotation fallback is not restored.
