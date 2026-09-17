# Agent Instructions

## Current firmware development (2026-09-17)

- The user authorized consolidating the ESP-IDF 6.1 / LVGL 9.5 migration into
  local `main`. Continue in
  `D:\21cncstudio\project_aura\tmp\worktrees\aura-post-115-clean`.
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
- Read `D:\21cncstudio\project_aura\tmp\worktrees\aura-post-115-clean\docs\LOCAL_MAIN_PREPARATION_20260830.md`
  before branch integration or firmware edits. Find it without asking the user.
- Continue firmware work on `main` in
  `D:\21cncstudio\project_aura\tmp\worktrees\aura-post-115-clean`.
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

## Firmware, USB and Release Handoff

- Before continuing Aura AQ 4.3-inch or 7-inch firmware profiles, dual I2C,
  GP8403/GPIO44/GPIO6 routing, native USB/COM10/COM11, OTA, or beta publication,
  read `D:\21cncstudio\project_aura\docs\DUAL_PROFILE_HANDOFF_20260830.md`.
  Find and read it without asking the user to supply the filename again.
- This is the entry point for source worktrees, exact tested artifacts, wiring,
  closed test evidence, signed packages, known defects, and publication gates.
  Use the absolute path when working from a nested worktree.
- Treat the handoff as a dated record, not live device or production state.
  Reverify branch/artifact/device identities and publication status before acting;
  the handoff does not authorize flashing, resets, migrations, or publication.

## Branch Discipline

Read [the current branch map](docs/ACTIVE_BRANCHES.md) before choosing a checkout.
There are two active firmware development lines:

- `main`: common firmware, ESP-IDF 6.1 / LVGL 9.5, in
  `D:/21cncstudio/project_aura/tmp/worktrees/aura-post-115-clean`.
- `feature/aura-link`: local Aura-to-Hub work, in
  `D:/21cncstudio/project_aura/tmp/worktrees/aura-hub`. It has no upstream;
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

## EEZ Studio UI Generation

EEZ Studio overwrites files under `src/ui` and does not know about every
Project Aura extension maintained outside the EEZ project.

- After every EEZ Studio **Build**, run `python tools/eez_ui_postprocess.py`.
- Before accepting generated UI changes, run
  `python tools/eez_ui_postprocess.py --check` and review `git diff -- src/ui`.
- The native ESP-IDF build runs the same post-process automatically before
  compilation.
- Do not commit `src/ui/.eez-project-build`.
- If a new project-maintained addition is repeatedly removed by EEZ, add it
  as an idempotent invariant in `tools/eez_ui_postprocess.py`, add a unit test,
  and document it in `docs/EEZ_UI_WORKFLOW.md`. Do not rely on remembering a
  manual repair after each generation.
- Preserve user-authored EEZ changes while cleaning generator noise. General
  UI changes belong on `main`; Aura Link / Aura Hub-specific UI changes belong
  on `feature/aura-link`.
