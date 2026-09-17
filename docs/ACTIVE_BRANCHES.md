# Active Aura firmware branches

Updated: 2026-09-18. This is a local source organization record.

## Continue here

| Work | Branch | Checkout |
| --- | --- | --- |
| General Aura firmware, both display profiles | `main` | `D:/21cncstudio/project_aura/tmp/worktrees/aura-post-115-clean` |
| Aura connection to Hub | `feature/aura-link` (local, no upstream) | `D:/21cncstudio/project_aura/tmp/worktrees/aura-hub` |

These are the two active lines for current firmware work. Other release,
contribution and diagnostic checkpoints are retained history, not alternative
current main branches. The root `D:/21cncstudio/project_aura` remains a dirty
diagnostic archive. Do not switch it, clean it, or move its nested worktrees.

Main already contains the ESP-IDF 6.1 / LVGL 9.5 migration, both display profiles,
five-minute history and the removal of the firmware badge from EEZ and runtime.
Installer release-package signing remains. The experimental on-device trust,
OEM licensing and trust badge are not being reintroduced.

The refreshed Hub branch is based on main `83b7c83b` plus this branch-map update.
It is a prepared development base, not a completed Aura-to-Hub implementation.
Old pairing, upload and web integration still require a selective port and review.
Main currently has no AuraHubClient or Hub pairing endpoint. Creating a branch
does not enable those features or complete the secure update chain.

## Archived lines (all history preserved)

| Previous branch | Retained local branch | Exact tip |
| --- | --- | --- |
| `feature/aura-link` | `archive/aura-link-pre-idf61-20260918` | `b493be75725f0080c2fee5109f9a324c5bffd4cd` |
| `feature/aura-link-legacy-ota-mixed` | `archive/aura-link-legacy-ota-mixed-20260918` | `99dab5b796872b69be705d654a3bb2faeb16c14b` |
| `feature/firmware-signing` | `archive/firmware-signing-oem-20260918` | `651d4d96449ae36b4896189eab76c4d49d757a85` |
| `idf-6.1-migration` | `archive/idf-6.1-migration-20260918` | `ae8fe02cd123d637df95e4d041e8369180dff5e3` |

Existing migration and signing worktrees remain at their original paths and
commit contents. Their branches now clearly identify them as archives.
`dual-profile-release` remains at `019d87b2bda51d1d6ae9c8d4c967e2cb4af4e8d9`;
`codex/aura-main-before-idf61-20260917` and other historical refs are preserved.
Older reports name branches as they existed on the date of that report.

## Next implementation stage (discuss separately)

The old Hub branch combines three areas:

- `e9c75ca3`: pairing/upload client, transport, sequence policies and host tests.
- `e13a8124`: application/web integration mixed with experimental signed OTA.
- `8042a7b9`: experimental trust, provenance and OEM mechanisms.

Port only the needed connection, telemetry and history work onto the current
main base. Review pairing credentials, bounded network work, retry limits,
sequence persistence and the current Hub API before enabling it. Preserve the
current UI, sensor interfaces and native IDF build. Do not blindly cherry-pick
the mixed integration commit or merge the old branch.

That stage needs meaningful host tests and both `scripts/build_idf.ps1` profiles
(`4_3`, `7_dual_i2c`), followed by separately authorized physical checks.
Signed updates through Link -> Hub -> Aura/satellites, interrupted-update recovery
and end-to-end validation remain separate unfinished work. A signed installer
package does not prove which firmware is running on a device.

## Evidence boundaries

This organization changes Git branch names and documentation only. It does not
change firmware source, SDK pins, generated UI or release artifacts. Existing
main validation is recorded in [the consolidation report](MAIN_IDF61_UI_CLEANUP_20260917.md).
No new firmware build or physical validation is claimed for this documentation
commit. Installed firmware and production are unchanged; nothing was pushed.

Local before/after refs, dirty-file hashes and validation are retained under
`D:/21cncstudio/aura-aq-hub/build/aura-branch-cleanup-20260918`.
