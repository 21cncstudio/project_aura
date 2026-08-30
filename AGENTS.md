# Agent Instructions

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

While `feature/aura-link` is active, keep Aura Link / Aura Hub work isolated on that branch.

- Code changes related to Aura Link or Aura Hub belong on `feature/aura-link`.
- Code changes unrelated to Aura Link or Aura Hub must be made and committed on `main`.
- If the current branch is `feature/aura-link` and the task is unrelated to Aura Link or Aura Hub, switch to `main` before editing or committing.
- If the scope is ambiguous, clarify before mixing changes across branches.

## EEZ Studio UI Generation

EEZ Studio overwrites files under `src/ui` and does not know about every
Project Aura extension maintained outside the EEZ project.

- After every EEZ Studio **Build**, run `python tools/eez_ui_postprocess.py`.
- Before accepting generated UI changes, run
  `python tools/eez_ui_postprocess.py --check` and review `git diff -- src/ui`.
- A normal `project_aura` PlatformIO build runs the same post-process
  automatically before compilation.
- Do not commit `src/ui/.eez-project-build`.
- If a new project-maintained addition is repeatedly removed by EEZ, add it
  as an idempotent invariant in `tools/eez_ui_postprocess.py`, add a unit test,
  and document it in `docs/EEZ_UI_WORKFLOW.md`. Do not rely on remembering a
  manual repair after each generation.
- Preserve user-authored EEZ changes while cleaning generator noise. General
  UI changes belong on `main`; Aura Link / Aura Hub-specific UI changes belong
  on `feature/aura-link`.
