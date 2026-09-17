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

Validation results are recorded below after completion. A successful local
build does not change installed Aura firmware or establish hardware stability.

The separate [Link marking proposal](FIRMWARE_ORIGIN_LINK_PROPOSAL.md) is design
work only; it does not change backend admission, device access or production UI.
