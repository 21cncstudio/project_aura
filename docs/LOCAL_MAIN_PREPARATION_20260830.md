# Local main preparation, 2026-08-30

The user approved local branch preparation and instructions for future chats.
Do not push, publish, deploy, or change remote refs without new explicit approval.

## Continue here

| Purpose | Branch | Worktree / checkpoint |
| --- | --- | --- |
| New firmware work, both hardware profiles | `main` | `D:\21cncstudio\project_aura\tmp\worktrees\aura-post-115-clean` |
| Tested firmware checkpoint | `dual-profile-release` | `019d87b2bda51d1d6ae9c8d4c967e2cb4af4e8d9`; no separate checkout |
| Old diagnostic history and dirty experiments | `archive/main-diagnostics-20260830` | `D:\21cncstudio\project_aura`, HEAD `7437b2c782cc3e38e63e909f8ef29e30303facb4` |

We renamed firmware branch `codex/dual-profile-release` to
`dual-profile-release`. We kept that checkpoint at the tested commit and created
local `main` from it. Main adds only documentation from this preparation;
we did not change firmware source or rebuild the tested packages.

Run firmware edits, builds, and Git commands in the clean main worktree.
The original root still contains dirty code, logs, packages, and nested worktrees.
Do not check out main in that root, move it, clean it, or run worktree prune as
part of this preparation. Use the existing clean worktree instead.

The same branch name in the separate Aura Link repository remains unchanged.
This preparation did not integrate Aura Link, Aura Hub, or site branches.

## Why we did not merge the old local main

We checked GitHub `refs/heads/main` with `git ls-remote` on 2026-08-30:
`da92fcce310924ec43f7be4378dee5ced0f40c48`.
This is a dated observation. Recheck the remote before any approved push.

The old local main added 28 diagnostic/recovery commits after `da92fcc`.
The clean checkpoint added a separate chain of 30 reviewed commits after the
same base. An ordinary merge into old main could retain old additions that the
clean branch never introduced. It would not replace main with the clean tree.

We renamed old main to the archive branch without switching its checkout.
We then created main at `019d87b` in the clean worktree. Its upstream is
`origin/main`; the archive branch has no upstream. Main descends from the remote
commit we checked, so this transition needs no remote history rewrite.

Do not merge the archive branch into main or transplant the root dirty tree.
Review and port an individual historical change only for a new scoped task.

## Preserved local evidence

Backup directory:
`D:\21cncstudio\project_aura\logs\LOCAL_MAIN_PREPARATION_20260830`.

- `old-main.bundle`: complete committed history through old main `7437b2c`;
  `git bundle verify` passed. This bundle does not contain uncommitted changes.
- `tracked-working-tree.patch` and `staged.patch`: original tracked/index diffs.
  There were no staged changes before preparation.
- `dirty-files/`: byte-for-byte copies of 39 modified or untracked authored
  files outside `logs/` and `tmp/`.
- `BEFORE.json`: original paths, byte counts, SHA256, and branch identities.

All 39 original files retained their bytes through the branch changes. We then
updated the root AGENTS.md and detailed handoff for this user request. Their
preparation-time originals remain in the backup. We left other dirty files intact.

Existing logs, ignored files, and nested worktrees remain in their original
locations. The backup directory is not a full copy of those directories.
No reset, stash, checkout of the root, or recursive move was used.

## Before continuing or publishing

Read the local evidence handoff without asking the user to find it again:
`D:\21cncstudio\project_aura\docs\DUAL_PROFILE_HANDOFF_20260830.md`.
It records wiring, COM identities, tested BIN/ZIP hashes, OTA results, and gates.
It is local evidence, not live device or production state. If working on another
machine where it is unavailable, do not infer missing test results or approval.

Preserve `dual-profile-release` at `019d87b` as the artifact checkpoint. A new
commit, including this documentation commit, has a different build identity;
do not label a fresh build as the exact already-tested `019d87b` image.

Both profiles remain in one firmware source tree: `project_aura` for 4.3-inch
and `project_aura_7` for 7-inch GPIO44/6. This branch preparation does not qualify
new hardware tests or unlock 7-inch publication. Existing release gates remain.

Before an approved push, inspect main status and diff, read the live remote main,
and confirm it is an ancestor of the intended local main. If the remote changed,
review its changes first. Do not force-push or bring back the archived recovery
chain. Do not publish the archive, backup directory, raw logs, or dirty code.

The user has not authorized any push in this preparation. Firmware flashing,
serial-open/reset, signing, production migrations, and publication also require
their own current scope and approval.
