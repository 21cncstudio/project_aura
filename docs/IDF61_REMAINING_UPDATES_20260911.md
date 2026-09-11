# Native I2C and LVGL 9 migration, 2026-09-11

This is local work on `idf-6.1-migration`, following the tested first-stage
ESP-IDF 6.1 checkpoint. It is not merged to main or published. The firmware
currently on the two boards is the earlier `c52e193` checkpoint; none of the
native-I2C/LVGL-9 candidates in this report has been flashed.

## Resolved versions

The upstream release pages and GitHub release API were checked on 2026-09-11.
The complete transitive resolution is committed in `dependencies.lock`.

| Component | Native build | Result |
| --- | --- | --- |
| ESP-IDF | 6.1 | Existing selected SDK retained |
| Arduino-ESP32 | 3.3.11 | Already the latest stable release |
| LVGL | 9.5.0 | Upgraded from 8.4.0 |
| ESP32_IO_Expander | 1.1.1 | Upgraded from 1.1.0, Aura reset policy reapplied |
| esp-lib-utils | 0.3.0 | Upgraded from 0.2.3 |
| ArduinoJson | 7.4.3 | Already the latest stable release |
| ESP-MQTT | 1.1.0 | Current resolved native component |
| ESP32_Display_Panel | 92b790ed6d24b0678e2f45b1fc85f0abd2d41b33 | Existing pin retained |

Sources: [LVGL](https://github.com/lvgl/lvgl/releases/tag/v9.5.0),
[IO Expander](https://github.com/esp-arduino-libs/ESP32_IO_Expander/releases/tag/v1.1.1),
[utils](https://github.com/esp-arduino-libs/esp-lib-utils/releases/tag/v0.3.0),
[Arduino](https://github.com/espressif/arduino-esp32/releases/tag/3.3.11),
[ArduinoJson](https://github.com/bblanchon/ArduinoJson/releases/tag/v7.4.3).

Display Panel's latest published tag is 1.0.4; Aura already pins a later
revision whose manifest says 1.0.5. The source is now vendored, with original
hashes and licenses, to make the reviewed utils 0.3.0 constraint reproducible.
The seven-file IDF adaptation remains generated from hash-checked source.
The unused utils plugin registry is disabled, keeping RTTI disabled.

## I2C

`components/aura_i2c` uses IDF's `i2c_master` API. Sensor and expander transfers
use cached per-port device handles. GT911 uses IDF 6.1's native LCD IO with a
borrowed bus handle and a finite 50 ms transaction timeout. The former
`aura_lcd_i2c_legacy` component is removed.

The port owner controls bus creation/deletion. Transfers do not reinstall or
recover a bus. Shutdown drains the existing runtime gates before releasing
LCD IO, cached devices and the bus. Failure exits preserve still-owned handles
for a possible explicit retry. Addresses, GPIO routing and bus rates remain
profile-specific. Command-plus-parameter writes stay one I2C transaction;
write/read operations keep repeated START framing. NACK and timeout remain
distinct to the existing sensor policies.

The host transport test compiles the actual native component against a
fault-injecting IDF fake. It covers both ports, device caching, framing,
NACK/timeout, lock/allocation/removal failures and stop/start invalidation.
The final ELF gate requires the new master/LCD IO symbols and rejects legacy
driver symbols. The SDK conflict check remains enabled.

The expander rebase initially restored upstream's `0xFF` output image. The
existing profile tests caught it before any hardware action. Corrected source
uses `Ch422gBoardPolicy.h`: `0xDB` for 4.3-inch and `0xD1` for 7-inch, with
output values written before enabling outputs. The intermediate I2C-only
candidates are explicitly marked `REJECTED_DO_NOT_FLASH` in their local
qualification record, and retained solely as failure evidence.

## LVGL and EEZ

LVGL 9 uses its public display, input, timer and chart APIs. RGB565 scanout,
the three physical framebuffers, existing VSYNC acknowledgement/fail-stop
policy, touch recovery and runtime 180-degree rotation are retained. LVGL 9
performs dirty-area synchronization between its two normal drawing buffers.
Rotated mode gives LVGL one logical drawing buffer and alternates two physical
output buffers. All LVGL callers still share Aura's recursive mutex; rendering
uses one draw unit. The allocator remains PSRAM-first with internal fallback.

Saved theme values remain RGB565 despite LVGL 9's RGB888 `lv_color_t`, so a
load/save cycle preserves old preferences. The web API continues to use hex
RGB. Preset matching compares the persisted RGB565 values.

Canonical EEZ source is `ui/aura-lvgl.eez-project`; its three source TTFs and
OFL licenses are under `ui/fonts`. It targets LVGL 9.5.0 and generates into
`../src/ui`. The original user file was also updated at:

`D:\21cncstudio\project_aura\release-assets\EEZ\aura-lvgl.eez-project`

Its output points to this migration worktree. The complete pre-edit backup is:

`D:\21cncstudio\project_aura\logs\eez_before_lvgl9_20260911T200734\EEZ`

All 129 backup files were SHA-256 verified again before changing the original.
The original EEZ file's pre-edit SHA-256 is
`57163796a2f1dfb445c207a7b958e49c73e0d9a348869335b25d2ef61ad1301f`.

Only ten JSON values changed: LVGL version, build destination, seven font
paths, and the embedded boot-logo encoding. The logo had JPEG bytes under a
PNG data URL; it was converted to actual PNG with identical decoded RGB
pixels. Page geometry, widget IDs, styles and text were preserved.

Installed EEZ Studio 0.27.1 supports LVGL 9.5.0. Its final isolated command-line
build reported no errors or warnings. Generated screens/styles/images were
updated, then `tools/eez_ui_postprocess.py` restored the existing Aura
invariants. Font C data already supports LVGL 9 and is retained, including the
extended Latin/Chinese/Japanese subsets. EEZ's headless build does not emit
these font C files; their compilation, rendering and coverage are checked
separately. Do not overwrite these subsets with a reduced editor subset.

## Validation and evidence boundary

- Both native profiles compile and pass compiled identity, RTC layout, OTA
  descriptor/hash, upstream restart-order and new-I2C linkage gates.
- Python suite: 109 tests pass, including vendor provenance and native I2C.
- The desktop LVGL 9 test compiles the actual generated screens, images and
  fonts, renders all 15 pages, and checks all 65,536 saved RGB565 values.
- Localized status glyph coverage and EEZ postprocess checks pass.
- Canonical native application/driver suite: 1044 cases passed across ten
  invocations, zero failures or skipped cases. Report:
  `.pio/native-tests/reports/20260911T195730Z-f4696b81/launcher.json`.

Local evidence directory:
`D:\21cncstudio\project_aura\tmp\idf61-sdk\remaining-updates`.
Relevant logs include `eez-final-build.log`, `lvgl9-host-run.log`,
`python-tests-final-2.log`, `native-tests-final.log` and the profile build logs.

The desktop renderer checks EEZ output, not the full device runtime. Some EEZ
pages intentionally contain overlapping alternative sensor labels; firmware
selects and hides them at runtime. Host rendering does not validate sensor
readiness, touch latency, USB reconnect, screen flip, sleep/wake or physical
cold boot. Those checks remain necessary on both boards before accepting this
checkpoint for main/release. No signing, release or publication was performed.

Firmware builds in this branch use `scripts/build_idf.ps1`. The old PlatformIO
firmware entry point is explicitly blocked after the LVGL 9 migration; its
configuration is retained for profile identity and host tests. The earlier
working baseline remains in Git and clean main.
