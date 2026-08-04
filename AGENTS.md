# Agent Instructions

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
