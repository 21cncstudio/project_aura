# Gradual ESP-IDF 6.1 migration

Branch: `codex/idf-6.1-migration`, based on committed main `78f4e9ba`.
The root archive worktree and the `dual-profile-release` checkpoint are preserved.
Main preparation was committed separately as `a75ae870` (sensor divider theme)
and `78f4e9ba` (dependency audit). Migration changes belong only to this branch.

## Scope and stages

1. Add native CMake/IDF build integration with Arduino 3.3.11 as a component.
2. Resolve source/driver incompatibilities and build both hardware profiles.
3. Verify the exact artifacts on hardware under a separately agreed test scope.
4. Migrate LVGL 8.4 to LVGL 9 as a later coordinated UI change.

The PlatformIO firmware build remains the old Arduino 3.1.1 / IDF 5.3.2 baseline.
Native tests continue to use `scripts/run_native_tests.py`. Do not describe a
passing PlatformIO build as evidence for the new IDF build.

## Native build

Install the official ESP-IDF v6.1 SDK with its recommended tools and Python
environment, then run from the migration worktree:

```powershell
./scripts/build_idf.ps1 -Profile 4_3 -IdfPath '<esp-idf-v6.1>' -ToolsPath '<idf-tools>'
./scripts/build_idf.ps1 -Profile 7_dual_i2c -IdfPath '<esp-idf-v6.1>' -ToolsPath '<idf-tools>'
```

`-Action reconfigure` performs configuration without compiling. The wrapper
exposes only build/reconfigure, and performs no flash, serial-open or reset.
Use `-Jobs 8` to override the default four parallel compilation jobs.

Local SDK prepared on 2026-09-11:

- `D:\21cncstudio\project_aura\tmp\idf61-sdk\esp-idf`, v6.1 commit
  `fff9895c82d744c7237be8847347bdd1b07c6643`.
- Tools and Python environment: `D:\21cncstudio\project_aura\tmp\idf61-sdk\tools`.
- ESP-IDF selects GCC 15.2.0 (`esp-15.2.0_20251204`). Existing PlatformIO tools
  were not replaced.
- Build/download logs are preserved in the `tmp\idf61-sdk` directory.

Each profile has its own `build-idf-<profile>` directory and generated
`sdkconfig.idf61.<profile>`. `sdkconfig.defaults.idf61` supplies the initial
configuration; changing defaults does not override values in an existing
generated sdkconfig. Review the resolved config before accepting a build.

The new configuration explicitly uses octal PSRAM, QIO/80 MHz flash, PSRAM XIP,
32 KB data cache, the existing 16 MB partition CSV, core 1 Arduino loop/events,
1 kHz FreeRTOS tick, and native USB Serial/JTAG. The IPC task stack is explicitly
4096 bytes, making the earlier requested setting effective in a source build.
These settings require hardware verification; compilation alone is insufficient.
The panel Kconfig switches explicitly allow the project board header and
`idf/include/esp_panel_drivers_conf.h`; otherwise the library defaults skip
Aura's board configuration. Only the ST7262/GT911/CH422G/custom-backlight
selection is enabled in the native build.

## Dependencies and preserved checks

`main/idf_component.yml` pins IDF, Arduino, LVGL, ArduinoJson, esp-lib-utils and
the existing Display Panel commit. `dependencies.lock` records the resolved
transitive versions and hashes. The local component override builds the same
Aura-patched IO Expander sources under `third_party`; it does not replace them
with registry defaults.

`scripts/idf_build_env.py` adapts only the operations used by existing Aura
build scripts. This reuses the canonical profile/target/GT911 identity policy,
EEZ postprocess, sensor-routing check and compressed web asset generators.
Preparation runs during configuration and before each firmware compilation so
Git identity and source changes are not hidden behind an old generated header.

`scripts/idf_verify.py` applies the existing RTC layout and OTA image checks.
The IDF 5.3.2 restart backport is excluded only from the native-IDF build; the
old PlatformIO baseline still includes it. The restart disassembly validator
can check the upstream `esp_restart_noos` symbol as well as the old wrapper.
The native check rejects accidental linkage of the old wrapper.

The binary checks are fail-closed: an ELF/BIN existing on disk does not qualify
the migration if its RTC layout, restart path or OTA descriptor checks fail.

## Current evidence

- Both profile preparation runs pass EEZ and sensor-routing checks and generate
  their own target/address metadata and web resources.
- CMake configuration succeeds for both `4_3` and `7_dual_i2c`, using IDF 6.1
  and GCC 15.2.0. The 7-inch generated identity is `aura-aq-7-v1`, GT911 `0x5d`;
  the 4.3-inch identity is `aura-aq-v1`, GT911 `0x14`.
- `python -m unittest discover -s tools/tests`: 103 tests passed.
- `scripts/run_native_tests.py -e native_test_ch422g_4_3_profile
  -e native_test_ch422g_7_profile`: 48 tests passed, 24 per profile. Report:
  `.pio/native-tests/reports/20260911T154558Z-b526364e/launcher.json`.
- EEZ postprocess check and `git diff --check` pass.
- Full 4.3-inch firmware compilation is blocked in the pinned Display Panel
  library. No completed firmware ELF/BIN exists. The final RTC, restart and
  OTA binary gates have therefore not run against an IDF 6.1 artifact.
- The 7-inch profile has configuration evidence only, not a full firmware build.
- No hardware test, flash, serial-open, reset, release package or publication
  was performed.

## Next stage: Display Panel and shared I2C compatibility

The real compiler errors are recorded in
`D:\21cncstudio\project_aura\tmp\idf61-sdk\build-4_3-08.log`:

- `BusI2C` assigns integer pins to IDF 6's typed `gpio_num_t` fields.
- `esp_lcd_i2c_bus_handle_t` and `esp_lcd_new_panel_io_i2c_v1` are absent in
  IDF 6.1. The new LCD factory takes a new-driver I2C master-bus handle, while
  Aura's sensors, bus recovery and IO expander still use the legacy driver.
- The RGB header expects SoC width macros removed in IDF 6; its fallback emits
  a warning that IDF promotes to a build error.
- An unconditional Display Panel public header includes `driver/ledc.h`.
  The native CMake integration now explicitly adds the `esp_driver_ledc`
  dependency rather than relying on the old umbrella `driver` component.

The follow-up `build-4_3-09.log` confirms configuration and tool discovery pass
after the LEDC fix; compilation still stops at the RGB width diagnostic.
Earlier I2C errors remain unresolved in the unchanged library sources.

The upstream managed component has not been edited in place. The next stage
must provide a reproducible library adaptation and coordinate I2C ownership
across panel, touch, sensors, IO expander and recovery. Simply passing an old
numeric I2C port to the new pointer-based factory is invalid. Retain both
profile policies and the artifact gates while making that change.

This is an experimental source-build checkpoint, not a buildable firmware
release. Passing configuration or host tests must not be treated as evidence
that IDF 6.1 firmware boots or operates on either board.
