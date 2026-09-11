# Aura dependency audit, 2026-09-11

Status: version and compatibility review only. No firmware dependency changes,
builds, tests, device access or publication were performed in this audit.

## Local baseline

- Worktree: `D:\21cncstudio\project_aura\tmp\worktrees\aura-post-115-clean`.
- Branch: `main`, HEAD `349796ed`; two commits ahead of the local `origin/main`
  tracking ref. The live remote ref was not queried.
- Existing uncommitted change: four added lines in
  `src/ui/UiControllerSensors.cpp`. Preserve this change.
- Both production environments inherit `env:project_aura` in `platformio.ini`.
- The root archive worktree and the tested release checkpoint were not changed.

## Versions checked against upstream

| Component | Current local configuration / installed version | Latest stable release found | Implication |
| --- | --- | --- | --- |
| ESP-IDF | 5.3.2 through Arduino 3.1.1 | [6.1](https://github.com/espressif/esp-idf/releases/tag/v6.1) | Current Arduino binary distribution instead supplies IDF 5.5.5. |
| Arduino-ESP32 | 3.1.1 | [3.3.11, based on IDF 5.5.5](https://github.com/espressif/arduino-esp32/releases/tag/3.3.11) | Upgrade core, framework libraries and toolchain as a matched set. |
| pioarduino platform | 53.03.11 | [55.03.311](https://github.com/pioarduino/platform-espressif32/releases/tag/55.03.311) | Pin the release archive; remove the obsolete 3.1.1 package overrides. |
| PlatformIO Core | 6.1.19, verified with `pio --version` | [6.1.19](https://github.com/platformio/platformio-core/releases/tag/v6.1.19) | Already current. |
| LVGL | 8.4.0 | [9.5.0](https://github.com/lvgl/lvgl/releases/tag/v9.5.0) | Major UI and display-port migration; not a version-only change. |
| ESP32_Display_Panel | Commit `92b790ed6d24b0678e2f45b1fc85f0abd2d41b33`, reports 1.0.5 | [1.0.4 release](https://github.com/esp-arduino-libs/ESP32_Display_Panel/releases/tag/v1.0.4) | The current commit contains changes beyond the latest published release. Do not silently downgrade it. |
| ESP32_IO_Expander | Vendored 1.1.0 with Aura CH422G changes | [1.1.1](https://github.com/esp-arduino-libs/ESP32_IO_Expander/releases/tag/v1.1.1) | Reapply and verify Aura reset image/order and USB/backlight policy when updating. |
| esp-lib-utils | 0.2.0 | [0.3.0](https://github.com/esp-arduino-libs/esp-lib-utils/releases/tag/v0.3.0) | [0.2.3](https://github.com/esp-arduino-libs/esp-lib-utils/releases/tag/v0.2.3) is the newest tagged 0.2 release and fits the current consumers' constraints. |
| ArduinoJson | `^7.0.0`, installed 7.4.3 in the 4.3-inch environment | [7.4.3](https://github.com/bblanchon/ArduinoJson/releases/tag/v7.4.3) | Already current in that cache; pin explicitly and verify all environments during the upgrade. |

The [55.03.311 platform manifest](https://raw.githubusercontent.com/pioarduino/platform-espressif32/55.03.311/platform.json)
selects the Arduino core and libraries from 3.3.11, Xtensa GCC
14.2.0+20260121 and its associated tools. These should follow the platform's
matched package set rather than independent upgrades to arbitrary tool versions.

## Compatibility findings

1. `framework = arduino` currently consumes prebuilt IDF libraries. Updating to
   stable pioarduino 55.03.311 provides IDF 5.5.5, not 6.1.
   The [Arduino 3.3.11 component manifest](https://raw.githubusercontent.com/espressif/arduino-esp32/3.3.11/idf_component.yml)
   declares IDF `>=5.3,<6.2`. IDF 6.1 with Arduino built as a component is
   therefore a possible migration path, but the manifest does not prove Aura
   compiles or works with it. This path needs a different build integration and
   review of project scripts, components, configuration and drivers.
2. `src/core/EspRestartNoosBackport.c` deliberately rejects every IDF version
   except 5.3.2. The migration must retire that implementation and the linker
   wrap, and adapt `scripts/check_esp_restart_noos.py` to validate the selected
   upstream restart implementation. Do not merely disable the version guard.
   Also review the old `NETWORK_EVENTS_MUTEX` workaround against the new core.
3. Both the installed Display Panel metadata and the
   [IO Expander 1.1.1 metadata](https://raw.githubusercontent.com/esp-arduino-libs/ESP32_IO_Expander/v1.1.1/library.properties)
   require esp-lib-utils below 0.3.0. Release 0.3.0 also changes minimum framework
   requirements and enables its plugin registry with RTTI. Use 0.2.3 for the
   initial compatible update unless the consumers are explicitly migrated.
4. Aura uses `src/lvgl_v8_port.cpp` and LVGL 8 APIs throughout the generated and
   maintained UI. The [LVGL 9 migration guide](https://lvgl.io/docs/open/9.0/CHANGELOG)
   changes display/input APIs, buffer units and color representation. Migrating
   to 9.5.0 needs coordinated EEZ configuration, generated assets, custom UI,
   display-port and postprocess changes plus visual verification.
5. Aura's sensor drivers use `driver/i2c.h`. The
   [IDF 6 migration guide](https://raw.githubusercontent.com/espressif/esp-idf/v6.1/docs/en/migration-guides/release-6.x/6.0/peripherals.rst)
   marks legacy I2C EOL, with removal scheduled for IDF 7.0. It is not already
   removed from IDF 6.1. However, the LCD I2C IO layer has dropped its legacy
   implementation, which requires explicit review for the display/touch stack.

## Proposed initial update

Use Arduino 3.3.11 / IDF 5.5.5 / pioarduino 55.03.311, esp-lib-utils 0.2.3,
and IO Expander 1.1.1 with the Aura patch preserved. Pin ArduinoJson 7.4.3.
Keep the current Display Panel commit and LVGL 8.4.0 during this first step.
IDF 6.1 build integration and LVGL 9.5 migration are distinct larger changes.

The user was asked to choose between this first step and immediate IDF 6.1
component-based migration. No implementation choice was applied in this audit.
After selection, run native tests and both firmware builds, including EEZ,
RTC ABI, restart and OTA identity checks. Hardware operation remains a separate
evidence level from successful local compilation and tests.

## Subsequent user decision

The user selected gradual migration to ESP-IDF 6.1 with Arduino as a component,
on a separate branch based on current main. Branch `codex/idf-6.1-migration`
was created in `D:\21cncstudio\project_aura\tmp\worktrees\aura-idf61`.
Before implementation, the user requested committing all remaining main
changes and advancing the migration branch to that baseline. This supersedes
the proposed 5.5.5 first step above. LVGL migration remains a later stage.
