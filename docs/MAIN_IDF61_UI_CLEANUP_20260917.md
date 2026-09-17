# Main consolidation and firmware badge removal, 2026-09-17

The user authorized moving local main to the existing ESP-IDF 6.1 / LVGL 9.5
implementation so future work has one primary source. Main remains in
`D:/21cncstudio/project_aura/tmp/worktrees/aura-post-115-clean`.

## Preserved source and merge

- Main before integration: `53c6a111`, preserved as
  `codex/aura-main-before-idf61-20260917`.
- Imported migration checkpoint: `ae8fe02c` on `idf-6.1-migration`.
- Both main history commits (`79e936a9`, `53c6a111`) are retained. Conflicting
  history implementation keeps main's bounded five-minute summaries and export
  behavior, with the migration's `std::isfinite` / `<cmath>` compatibility.
  Sensor tests retain both freshness and SEN69C/recovery cases.
- The archive checkout and `dual-profile-release` are unchanged. The unrelated
  untracked `docs/SFA40_AUDIT_20260916.md` remains outside this change, with a
  byte-for-byte backup. No push or device operation is part of this work.

## UI scope

Removed `chip_firmware_trust` and its child label from the canonical EEZ project,
regenerated screens and object declarations, and removed the obsolete trust-label
postprocessor override and its fixture. Existing firmware signing and release
package verification are preserved. No replacement unofficial-firmware badge
or installed-image authenticity claim was added.

The user's saved editor file at `release-assets/EEZ/aura-lvgl.eez-project` has
the same two objects removed and its output redirected to main. Its other saved
layout values and font paths are retained; seven pre-existing layout/flag
differences from the canonical repository copy were not overwritten. Continue
from `ui/aura-lvgl.eez-project` in main for the reviewed source. The old saved
editor file is backed up under `logs/main-idf61-ui-cleanup-20260917`.

## Build and validation

Use `scripts/build_idf.ps1` for `4_3` and `7_dual_i2c`. PlatformIO remains the
host-test/metadata tool. The imported lockfile and pinned SDK/component versions
are retained; this consolidation does not select newer dependencies.

Integration commits:

- `3f20fedc`: merge migration into main, remove the UI badge, update development
  entry points and record the separate Link proposal.
- `b081572e`: adapt history acquisition to the SEN6x facade. Preserve raw CO2
  for history and track SEN69C particle-number acquisition separately from its
  earlier measurement-values response. This prevents a cached PM0.5 value from
  being counted as a new sample while the numbers response is still pending.
- `9351bcba`: use portable `PRIX32` formatting for the history-export device ID
  with the IDF 6.1 compiler. The resulting identifier is unchanged.

Validation completed against firmware source `9351bcba0665dc2a74520549bf4e4ce9cedff9f5`:

- Full native test launcher: **1098 cases passed**, 123 suite/environment runs
  across all 10 environments; Unity source identity checked by the launcher.
- Targeted sensor-manager/SEN69C check: 89 cases passed, including separate
  values/particle-count acquisition and repeated cached-data coverage.
- Python tool tests: **113 passed**.
- Installer signing/package tests: **18 passed**; release-layout tests: **4 passed**.
  These use test data/keys and do not sign or publish a production release.
- Native ESP-IDF 6.1 builds: **4_3 and 7_dual_i2c passed**. Both retain the
  compiled identity, RTC ABI, OTA target/image integrity, new I2C linkage and
  upstream restart-order validation gates.
- EEZ Studio export completed, followed by the postprocessor and its `--check`.
  JSON comparison confirms the canonical project differs from the migration
  checkpoint only by removal of the badge and child label. The saved external
  editor differs from its backup only by that removal and the output directory.
- Host LVGL 9 check rendered all 15 generated screens plus CO2 warmup, and
  verified all 65536 RGB565 persisted-color round trips. The Settings render
  was visually inspected; the font-coverage check passed. Its temporary build directory was removed after
  retaining the executable, render files and logs outside the source worktree.
- `git diff --check` passed. Dependency pins/lockfile and all release scripts
  match the imported migration checkpoint. Both pre-merge branch histories
  remain ancestors of main.

The first Python run lacked materialized managed components; it passed after
the first IDF configuration prepared them. Initial native/firmware compilation
exposed the old SEN66 history references, and the next IDF pass exposed the
integer-format mismatch. Both were corrected above; failed and passing logs
are retained rather than treating the initial merge as validated.

The local build IDs are `9351bcb-dirty` (4.3-inch) and
`9351bcb-7-dual-i2c-dirty` (7-inch): the build identity correctly includes
the preserved, untracked user audit document. That document was not staged or
modified (SHA-256 `801708a15c2c797b78d5f6fa4bc1f80c51e972980a2260c7a847d542b151c1f3`).
These are validation builds, not published release packages.

| Profile | Application bytes | Application SHA-256 |
| --- | ---: | --- |
| `4_3` | 4576256 | `936feca6eb50808e94733af1ecffcd1c91e9f49a5735789f9ad8ee4fdb9e5670` |
| `7_dual_i2c` | 4576576 | `ef8fa36464418034dc2abe1170c4e31490a682d3b844d9bd83c9aa202d453da2` |

Full local evidence is retained under
`D:/21cncstudio/project_aura/logs/main-idf61-ui-cleanup-20260917`, including
per-profile identity/artifact manifests, all test/build logs, saved-editor
backup, generated render files and `source-verification.json`.

No Aura or Hub was flashed, reset or opened over serial during this cleanup.
No remote branch, installed firmware or Link production state was changed.
Successful builds and host renders do not establish physical display, touch,
sensor, Wi-Fi or long-term stability for these exact merged images. Hardware
validation is the next separate step before installing/publishing them.

The separate [Link marking proposal](FIRMWARE_ORIGIN_LINK_PROPOSAL.md) is design
work only; it does not change backend admission, device access or production UI.
