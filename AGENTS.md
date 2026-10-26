# Agent Instructions

<!-- BEGIN:aura-current-state -->
## Current project state

Read `CURRENT_STATE.md` before broad exploration; follow only task-relevant links.
Update it after significant completed work: date, evidence, open issue, next step.
Keep history in linked reports and secrets out of these files.
Cross-project and MacBook/HA context: `D:/21cncstudio/AURA_WORKSPACE.md`.
<!-- END:aura-current-state -->

## Prefer existing solutions

- Before implementing a feature, fixing a problem or optimizing a subsystem,
  first search online for existing solutions. Prioritize official documentation,
  vendor examples, maintained libraries/components and upstream fixes or issues.
  Also inspect relevant implementations in the other Aura and Hub projects.
- Prefer an applicable existing solution. Check compatibility with the actual MCU,
  display/interface and dependency versions, licensing, resource requirements and
  integration constraints. Verify relevant behavior locally; upstream claims or
  benchmarks do not establish performance or correctness on our hardware.
- Write a custom implementation only when no applicable solution is found or the
  available options fail a concrete requirement. Briefly record the sources
  checked and why they do not fit before proceeding; a superficial search is not
  evidence that no solution exists.

## Workspace and branches

- Common firmware: `D:/21cncstudio/project_aura_aq/firmware-main`, branch `main`.
- Aura-to-Hub firmware: `D:/21cncstudio/project_aura_aq/firmware-hub`, local
  `feature/aura-link`, no upstream. This is not the separate Link backend repo;
  preserve their isolation and do not rename its branches as part of firmware work.
- Read [the branch map](docs/ACTIVE_BRANCHES.md) before choosing a checkout.
  Its older "Hub port not done" claim is superseded by the dated
  [Hub integration report](../firmware-hub/docs/HUB_INTEGRATION_STATUS.md). Recheck refs before integration;
  source/build evidence does not establish the installed BIN or device behavior.
- Put general fixes on main, then merge main forward into the Hub branch.
  Keep Hub-specific code out of main until separately reviewed and accepted.
- Clarification, 2026-10-26: unused standalone scaffolding for Hub UI/history is
  Hub-specific even when named a generic/shared module. Keep it out of main and
  its commit ancestry unless the owner explicitly approves that standalone change.
  Preserve Hub-only commits in the Hub line or a local archive; do not merge the
  standalone cleanup back into Hub blindly.
- Common Git repository and dirty diagnostic root:
  `D:/21cncstudio/project_aura_aq/archive/diagnostic-root`. Preserve its branch,
  dirty experiments and nested worktrees; do not switch it to main, clean or move it.
- `D:/21cncstudio/project_aura` and its old checkout/SDK paths are compatibility
  junctions, not duplicate trees. Do not recursively delete them or target new work there.
- SDK: `D:/21cncstudio/project_aura_aq/tools/idf61-sdk`.
  Organization and build wrapper: `D:/21cncstudio/project_aura_aq/README.md` and
  `tools/build.ps1` in that organization root.

## Required reading and boundaries

- Before firmware edits or branch integration, read
  `D:/21cncstudio/project_aura_aq/firmware-main/docs/LOCAL_MAIN_PREPARATION_20260830.md`.
- Before Aura 4.3/7-inch profiles, dual I2C, GP8403/GPIO44/GPIO6, native
  USB/COM10/COM11, OTA or beta work, read
  `D:/21cncstudio/project_aura/docs/DUAL_PROFILE_HANDOFF_20260830.md`.
  Find these files without asking the user. The handoff holds artifact, wiring,
  test, signing, defect and publication evidence; reverify branch/artifact/device
  identities and production status before acting on dated records.
- Read [workspace history](docs/WORKSPACE_HISTORY.md) for integration or archive
  recovery. Preserve all dirty archives, release checkpoints and exact tested BINs,
  including `dual-profile-release`, `codex/aura-main-before-idf61-20260917`,
  `archive/idf-6.1-migration-20260918` and `archive/aura-link-pre-idf61-20260918`.
  Do not copy the dirty root into main or wholesale merge the archived diagnostic
  chain, migration, signing, mixed OTA or OEM experiments into active branches.
- Local preparation/source/build work authorizes no flashing, resets, device
  migrations, release signing, pushes, publication, deployment, production changes
  or remote-ref changes. Obtain explicit authorization for those actions. No force-push.

## Builds and EEZ

- ESP-IDF 6.1 / LVGL 9.6: build both `4_3` and `7_dual_i2c` with
  `scripts/build_idf.ps1`. PlatformIO is for host tests/profile metadata; its old
  firmware build is intentionally blocked. Do not downgrade dependencies to unblock it.
- Canonical EEZ source: `ui/aura-lvgl.eez-project`. Keep both profiles supported.
  EEZ overwrites `src/ui`; after every EEZ Build run
  `python tools/eez_ui_postprocess.py`. Native IDF builds also run it automatically.
- Before accepting generated UI changes, run
  `python tools/eez_ui_postprocess.py --check` and review `git diff -- src/ui`.
  Preserve user EEZ edits; do not commit `src/ui/.eez-project-build`.
- For an extension repeatedly removed by EEZ, add an idempotent postprocess
  invariant, a unit test and documentation in `docs/EEZ_UI_WORKFLOW.md`.
  Do not rely on manual repairs. General UI belongs on main; Hub UI on the Hub branch.
