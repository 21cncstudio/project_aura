# Firmware workspace history

Archived from firmware-main/AGENTS.md on 2026-10-01. This is a historical
record, not the active instruction entry point. Use the root AGENTS.md and
CURRENT_STATE.md for current paths and rules. Read this document for branch
integration or archive recovery; verify refs and evidence before acting.

The old main snapshot below predates the selective Hub port. The dated report
in firmware-hub/docs/HUB_INTEGRATION_STATUS.md records its implementation and
validation; neither document establishes which BIN is running today.

## Workspace organization (2026-09-23)

- The user authorized the local directory reorganization recorded in
  `D:/21cncstudio/project_aura_aq/README.md`.
- General work: `D:/21cncstudio/project_aura_aq/firmware-main`, branch `main`.
- Aura-to-Hub work: `D:/21cncstudio/project_aura_aq/firmware-hub`, local
  `feature/aura-link`; preserve isolation and do not publish it.
- The common Git repository and dirty diagnostic root are preserved under
  `D:/21cncstudio/project_aura_aq/archive/diagnostic-root`.
- SDK and tools: `D:/21cncstudio/project_aura_aq/tools/idf61-sdk`.
- Use `tools/build.ps1` in the organization root for a local profile build.
- Older "do not move" instructions below describe the previous preparation.
  This authorized move is complete; preserve the new layout and all archives.
- `D:/21cncstudio/project_aura` and its old active-checkout/SDK paths are
  compatibility junctions. They are not duplicate source trees. Do not delete
  them recursively or use them as the destination of new work.
- No source branches were merged, pushed or reset, and no device was touched.

## Current firmware development (2026-09-17)

- The user authorized consolidating the ESP-IDF 6.1 / LVGL 9.5 migration into
  local `main`. Continue in
  `D:\21cncstudio\project_aura_aq\firmware-main`.
- Build both profiles with `scripts/build_idf.ps1`: `4_3` and `7_dual_i2c`.
  PlatformIO remains the host-test and profile-metadata path; its old firmware
  build is intentionally blocked. Do not downgrade dependencies to make it run.
- Canonical EEZ source is `ui/aura-lvgl.eez-project`; regenerate and run
  `tools/eez_ui_postprocess.py` together. Keep both display profiles supported.
- The old `aura-idf61` worktree (`archive/idf-6.1-migration-20260918`) is a retained checkpoint, not the active
  development destination. Preserve `codex/aura-main-before-idf61-20260917`,
  `dual-profile-release`, the root archive and exact tested artifacts.
- This consolidation authorizes local source/build work. It does not authorize
  flashing, signing a release, pushing, publishing, or changing production.

## Local Main Preparation (2026-08-30)

- The user authorized local preparation only. Do not push, publish, deploy, or
  change remote refs without new explicit approval.
- Read `D:\21cncstudio\project_aura_aq\firmware-main\docs\LOCAL_MAIN_PREPARATION_20260830.md`
  before branch integration or firmware edits. Find it without asking the user.
- Continue firmware work on `main` in
  `D:\21cncstudio\project_aura_aq\firmware-main`.
  The root `D:\21cncstudio\project_aura` now holds
  `archive/main-diagnostics-20260830` and preserved dirty experiments.
  Do not switch the root checkout to main, clean it, or move its nested worktrees.
- `dual-profile-release` is the renamed firmware checkpoint at `019d87b`.
  Preserve it and the tested BINs; continue new firmware changes on clean main.
- Do not merge the archived 28-commit diagnostic chain or copy the root dirty
  tree into main. Clean main descends from `da92fcc`; the old diagnostic main
  was `7437b2c`. Recheck refs before integration and do not force-push.
- The same old branch name in the separate Aura Link repository is outside this
  rename. Preserve Aura Link / Aura Hub isolation below.

## Branch Discipline

Read [the current branch map](docs/ACTIVE_BRANCHES.md) before choosing a checkout.
There are two active firmware development lines:

- `main`: common firmware, ESP-IDF 6.1 / LVGL 9.5, in
  `D:/21cncstudio/project_aura_aq/firmware-main`.
- `feature/aura-link`: local Aura-to-Hub work, in
  `D:/21cncstudio/project_aura_aq/firmware-hub`. It has no upstream;
  do not publish it without explicit approval. The retained name follows the
  existing project convention; this is the Aura firmware repo, not Link backend.
- The current Hub branch starts from current main. Old pairing/upload code is
  preserved on `archive/aura-link-pre-idf61-20260918`; it has NOT been ported
  into the new branch yet. Do not claim a combined candidate is ready.
- Keep general fixes on main, then merge main forward into the Hub branch.
  Keep Hub-specific code out of main until separately reviewed and accepted.
- Signing/OEM experiments and the completed migration are archive checkpoints.
  Do not merge their old branch contents wholesale into either active line.
- Preserve dirty archive checkouts, release checkpoints and exact tested BINs.
  Local branch organization is not authorization to flash, publish or deploy.
