# SEN69C VOC factory-reset correction

Date: 2026-10-12. Local source change; no device writes or publication.

## Defect and scope

The 1.2.1 beta review found that `StorageManager::clearAll()` omitted
`/sen69c_voc_v1.bin`. The file could survive a factory reset and restore the
same sensor's previous VOC algorithm state. The host implementation used a
blanket map clear, hiding the omission from tests.

This does not explain the reported 7-inch unit becoming unresponsive after
days of operation. This correction changes factory-reset persistence, not
power, GPIO, display, USB, clock or startup configuration.

Main commit: `d81d1627`; Aura-to-Hub merge: `52eb4b6b`.
The published beta snapshot and its retained release assets were not edited.

## Implementation

- The driver and storage manager share `kSen69cVocStatePath`.
- Factory reset calls the existing `removeBlob`, removing the main file and
  its `.tmp` and `.bak` companions.
- Host tests execute the same reset file list as firmware. The host backend's
  per-path removal also removes its separate typed configuration records.
- Hub retains its additional removal of `kAuraHubConfigPath`.

Existing solutions checked: the official
[Espressif LittleFS removal example](https://github.com/espressif/arduino-esp32/blob/master/libraries/LittleFS/examples/LITTLEFS_test/LITTLEFS_test.ino),
[Sensirion SEN6x datasheet](https://sensirion.com/resource/datasheet/SEN6x),
and the active Aura-to-Hub storage implementation. Aura's existing atomic-file
cleanup already fits the fix; no filesystem or sensor library was replaced.

## Regression evidence

The new SEN69C tests initially failed with the production reset list unchanged:
one restored the primary record after `clearAll`, the other recovered a lone
backup after `SafeFactoryReset`. Both pass after adding the missing deletion.
The failing Unity run also records a runner SIGINT error after the assertions;
the two recorded assertion failures are the defect reproduction.

Three added tests cover reset/reload, orphan-backup recovery, and removal of
current/last-good configuration and legacy VOC while preserving an unrelated
file. They do not silently reset test persistence before checking the result.

| Checkout | Suites | Passed |
| --- | --- | --- |
| Main | SEN69C, storage, charts, sensor manager, last-good health | 146 |
| Aura-to-Hub | SEN69C, storage, charts, Hub client/pairing | 90 |

Native runs used `scripts/run_tests.ps1`; the launcher verified Unity source
identity for all selected suites. Original reports are under each checkout's
`.pio/native-tests/reports/`:

- Main failing control: `20261012T164729Z-b534d2d7`.
- Main passing: `20261012T164755Z-0ab53405`.
- Hub passing: `20261012T165431Z-8bb3a34a`.

Copies, build logs, and before/after hashes are retained in
`C:/Users/user/.codex/visualizations/2026/10/10/01a12552-790f-7a31-b1e6-1086e5bca86d/aura-voc-reset-fix-20261012/`.
`before.json` records both original refs, dirty-file hashes and preserved
BIN/ELF/build identity artifacts. Pre-existing dirty files were hash-verified
after the source change and merge.

## Build and device boundary

All four ESP-IDF builds completed with the standard organization wrapper.
The post-build artifact stamps match all five installer inputs in each build;
`after.json` records source refs, build IDs, hashes and preserved candidate paths.
EEZ postprocess checks and `git diff --check` passed in both checkouts.

| Checkout / profile | Build ID | App BIN SHA256 |
| --- | --- | --- |
| Main / 4.3 | `d81d162-dirty` | `146529931e6c76508b0ed805e11eb0111a79302797ecbac1c4ff65332c1ac423` |
| Main / 7 | `d81d162-7-dual-i2c-dirty` | `a4d81f768d9d06ab1a60f7c054cc32c5efe910f8495e86bf3976b5bb406932e3` |
| Aura-to-Hub / 4.3 | `52eb4b6-dirty` | `bc755223e1ef4bed47a01c70240c030ef7c8d6108e1642d4a04037c3e4f1c9d3` |
| Aura-to-Hub / 7 | `52eb4b6-7-dual-i2c-dirty` | `3ae520ce1069c6fcf2a12ed935ede7816cc9494d08704b578be5fdd81a48a332` |

These are local checkout builds, including the pre-existing UI changes denoted
by `dirty`; they are not replacement published beta packages. All original
dirty files were hash-verified again after builds, before adding the dated
CURRENT_STATE entries. The original CURRENT_STATE contents remain below them.

No device reset, flash, signing, push or release change is part of this fix.
The reported unresponsive board remains a separate, unresolved diagnosis.
Next: include this fix in a separately approved release; physical reset behavior
has not been retested on a device during this task.
