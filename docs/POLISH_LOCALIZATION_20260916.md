# Polish localization and button centering, 2026-09-16

Local work on `idf-6.1-migration` in `tmp/worktrees/aura-idf61`, based on
`5a6da15fe69c7018d0213e0af417b5575b250faa`.

## Changes

- Added `POLSKI` after Dutch in the language selector. Persisted language IDs
  0 through 8 stay unchanged; Polish uses 9.
- Added 369 Polish strings: the existing 340-entry dictionary plus 29 formerly
  hardcoded labels/messages for settings, firmware update, fan control and About.
  The previous nine dictionaries retain their original 340 entries; the new
  entries preserve the English text those screens used before this change.
- Preserved format placeholders, addresses and numeric reference bands.
- Added 16 missing Polish letters to both JetBrains body-font subsets. `Óó`
  were already present, so both fonts now cover all 18 Polish letters without
  removing any previous glyph. Updated canonical and user EEZ sources as well
  as `ui_font_jet_reg_14.c` and `ui_font_jet_reg_18.c`.
- Set all 186 EEZ button containers to Flex with main/cross/track centering.
  Retained button sizes, positions, colors, flags and handlers. Kept the language
  title before its value and preserved mutually exclusive confirmation labels.
- Adjusted six label widths within their buttons. This allows full Polish
  Settings/Units labels and fixes Dutch VOC relearn/NTP wrapping. Boot button
  labels now fit within their borders.
- Fan connection help is refreshed after a language switch even if the network
  connection has not changed.

## Backups and preservation

Before editing EEZ, copied the complete canonical `ui` and user EEZ directories
to `D:\21cncstudio\project_aura\logs\polish_localization_20260916T192018Z`.
`SHA256.json` records and verifies all 135 copied files.

Compared both edited EEZ files with their respective backups. All 867 semantic
field changes are limited to glyph sets, button Flex/text alignment and the six
label widths. Generated resources contain matching Flex settings for every
button. Other generated screens/images/style content was not changed.

## Validation

- EEZ Studio headless generation completed successfully; postprocessor and
  `tools/eez_ui_postprocess.py --check` passed.
- `tools/check_ui_font_coverage.py` passed all six font checks. The 18 px Latin
  check covers all strings, including Polish.
- All ten dictionaries contain 369 nonempty entries. Checked Polish format
  placeholders, URLs and numeric reference bands against English.
- PlatformIO: `test_init_config` and `test_status_messages`, 19/19 passed.
  These include stored-language compatibility and an actual CO warning in
  Polish followed by switching back to English without changing its severity.
- Host rendering uses the actual LVGL 9.5 generated screens and font C files.
  Applied 303 text bindings from runtime sources with each locale's body fonts.
  Checked all 186 button containers across ten locales, including fan tabs and
  all three confirmation-label variants. No measured button overflow or
  centering errors remained. Rendered 20 screen/variant images per locale.
- `git diff --check` passed.

The host harness applies relevant text/visibility states; it does not execute
the whole firmware controller or exercise touch. Raw generated screens also
contain alternate panels whose visibility is normally controlled by firmware.
Physical-device behavior has not been tested for this change.

Local preview and validation files:

- `.pio/polish-preview/settings-pl.png`
- `.pio/polish-preview/settings-all-languages.png`
- `.pio/polish-preview/polish-final.png`
- `D:\21cncstudio\project_aura\tmp\polish-tools\prepare_preview.py`
- `D:\21cncstudio\project_aura\tmp\polish-tools\lang-0.log` through `lang-9.log`
- `D:\21cncstudio\project_aura\tmp\polish-tools\native-tests.log`

## Firmware artifacts

Built locally with `scripts/build_idf.ps1`, ESP-IDF 6.1 and the installed native
toolchain. Artifact identity, ESP checksum/hash, native I2C linkage and upstream
reset ordering are checked by the build script.

| Profile | File | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| 4.3 inch | `build-idf-4_3/aura_aq.bin` | 4582608 | `d4b7f629fc4fbbf626bde2e8dfff87d2ae61e0b8cd2c575ee3200d80584fca0f` |
| 7 inch, dual I2C | `build-idf-7_dual_i2c/aura_aq.bin` | 4582912 | `3e56b35a90341e8727d7bf4c885cd4f12f72577a23b9c819329254f7d9f78579` |

Both builds completed successfully and passed the four artifact checks above.
Logs are `build-4_3-final.log` and `build-7_dual_i2c-final.log` in
`D:\21cncstudio\project_aura\tmp\polish-tools`.
Existing SDK Kconfig notes and unused network-variable/function warnings remain;
the builds are successful, not warning-free.

No device was flashed or reset. No remote branch, release or publication was
changed.
