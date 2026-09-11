# Gradual ESP-IDF 6.1 migration

Branch: `codex/idf-6.1-migration`, based on committed main `78f4e9ba`.
The root archive worktree and the `dual-profile-release` checkpoint are preserved.
Main preparation was committed separately as `a75ae870` (sensor divider theme)
and `78f4e9ba` (dependency audit). Migration changes belong only to this branch.

## Scope and stages

1. Add native CMake/IDF build integration with Arduino 3.3.11 as a component.
2. Resolve source/driver incompatibilities and build both hardware profiles.
3. Verify the exact artifacts on hardware under a separately agreed test scope.
4. Replace the temporary LCD/legacy-I2C adapter with a coordinated migration
   of all shared-bus users to the new I2C master API.
5. Migrate LVGL 8.4 to LVGL 9 as a later coordinated UI change.

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
LVGL also explicitly uses `include/lv_conf.h`, including its PSRAM allocator,
fonts and 40 ms input/display periods. The application keeps C++17; the SDK
builds its components with its own language defaults.

The old Arduino SDK enabled the FSM ULP component and reserved 512 bytes of
RTC slow memory. The native defaults restore that reservation. No ULP program
is loaded or started by this change. Omitting the reservation shifts all Aura
RTC_NOINIT symbols by 512 bytes and fails the existing absolute-address gate.

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
It also verifies the compiled build ID and rejects linking the new I2C driver
alongside the legacy driver. The SDK's own startup conflict check stays enabled.
The PowerShell wrapper refreshes generated identity before Ninja evaluates its
dependency graph, including immediately after a Git commit.

The binary checks are fail-closed: an ELF/BIN existing on disk does not qualify
the migration if its RTC layout, restart path or OTA descriptor checks fail.

## Driver and source adaptation

`scripts/idf_panel_overlay.py` verifies four upstream source hashes and creates
build-directory copies of Display Panel. CMake compiles those copies and exports
their headers. Managed components are never patched in place; an upstream
change to a patched file fails configuration and requires review.

The native adaptation is limited to Aura's ESP32-S3 / 16-line RGB565 profiles.
It uses IDF 6's explicit RGB input/output formats, typed GPIO fields and updated
LCD configuration ordering. Original panel geometry and timing values are kept.
The application objects are available to the linker even when the display
library is the only caller of a board callback; function-section GC remains on.

`components/aura_lcd_i2c_legacy` contains the unmodified Espressif v5.5.5 LCD IO
adapter, compiled against IDF 6.1 with an Aura-specific exported name. Its README
records source, license, hash and lifetime. It preserves the existing numeric
port, synchronous transfers, repeated START behavior, timeout and bus ownership
used by GT911, CH422G, sensors and recovery. It is explicitly a temporary adapter,
not a conversion of those users to the new I2C master API.

Other source fixes use standard C++ math names, typed printf formats, bounded
date formatting and explicit enum conversions. Wi-Fi inactivity still maps to
reason code 4, and no retry timing or sensor threshold is changed. CMake declares
the LEDC and Wi-Fi provisioning header dependencies explicitly.

## Validation

- Python checks: 107 passed (`tmp\idf61-sdk\python-stage2.log`).
- Full canonical native test launcher: 1044 tests passed across 10 invocations,
  with zero failures/errors/skipped cases. Report:
  `.pio/native-tests/reports/20260911T162253Z-fcfe8f08/launcher.json`.
- After date-log formatting changed, the existing TimeManager/NTP suites were
  rerun: 51 passed. Report:
  `.pio/native-tests/reports/20260911T163926Z-0acba4ee/launcher.json`.
- EEZ postprocess and whitespace checks pass.
- Both native-IDF builds pass: `4_3` / `aura-aq-v1` and
  `7_dual_i2c` / `aura-aq-7-v1`. Each image passes compiled build identity,
  the unchanged RTC layout gate, OTA descriptor/checksum/hash, upstream restart
  order and legacy-only I2C linkage checks. Both leave 38% of the app partition
  free. Logs before the source commit: `tmp\idf61-sdk\build-4_3-18.log` and
  `tmp\idf61-sdk\build-7_dual_i2c-01.log`.
- The RTC gate initially caught an omitted 512-byte reserve. Restoring the old
  SDK reservation fixed the layout; the gate and its expected addresses were
  not relaxed.
- Commit-linked rebuild logs and exact artifact hashes are recorded locally in
  `D:\21cncstudio\project_aura\tmp\idf61-sdk\final-build-manifest.json`.

Configuration and host tests do not prove that firmware boots or operates on
either board. Hardware checks, flash/serial/reset, signing, release packaging and
publication are outside this local migration step and have not been performed.
