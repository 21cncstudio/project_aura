# Local 1.2.0-beta preparation, 2026-09-03

Scope: local source changes, regression tests, clean builds and signed release
packages. The operator separately approved local Aura Link and site alias
compatibility. No push, deployment, production database migration, package
import/publication, promotion-gate enablement or hardware action is authorized.

## Source locations

| Component | Local worktree | Branch and starting point |
| --- | --- | --- |
| Firmware | `D:\21cncstudio\project_aura\tmp\worktrees\aura-post-115-clean` | `main`, UI commit `60763c23` |
| Aura Link | `D:\21cncstudio\project_aura\tmp\worktrees\aura-link-profile-rename` | `codex/7-dual-i2c-compat`, `d9bf23c51bc3c3f2d34532a56ded5a19a3be1f4f` |
| Installer site | `D:\21cncstudio\project_aura\tmp\worktrees\aura-site-profile-rename` | `codex/7-dual-i2c-compat`, `0ebb7aafcb9794099bfd3a85bd189f65e6923456` |

Consumer starting points are the newest locally cached `origin/main` commits
inspected for this task, not a fresh remote or production-state check. Dirty
root checkouts and older worktrees remain untouched. Firmware checkpoint
`dual-profile-release` stays at `019d87b`.

## Closed operator test of the preceding EEZ candidate

Exact preserved inputs:
`D:\21cncstudio\project_aura\logs\firmware_candidate_ee3d8a7_DIRTY_EEZ_CO2_BUTTONS_BOTH_20260902T201701Z`.

| Board | Firmware SHA256 |
| --- | --- |
| 4.3-inch | `8211A28F68BE27820CF334A031F5A7355BDCD893CDE483E3F210055DF51238DF` |
| 7-inch | `AAC0EEEDDFEF6AA803C272C5FAB646888659CC86BF565E3C438ECFC776DCA1E6` |

The accepted EEZ source SHA256 is
`3AE35C5A3094AE4F8A72A0D1221E46F66367EB1C44E6509D7ACF020B063F75BD`.
Its generated CO2/button changes are committed in `60763c23`. The ignored EEZ
source and tested images remain preserved locally, not silently added to Git.

The read-only morning capture is
`D:\21cncstudio\project_aura\logs\overnight_ee3d8a7_dirty_eez_20260903T081836Z`.
At the final sample the devices reported uptime 40,580 s and 40,503 s,
consistent with the evening boots, and ready Board/LVGL with no observed
display/framebuffer/touch faults in the available diagnostics. Private RGB
recovery instrumentation had been removed; no direct zero-recovery-counter
claim is made.

Subsequent operator feedback: Wi-Fi was deliberately off for about ten minutes;
PM excursions occurred while vaping. The operator confirmed normal screen,
CO2 card, buttons and touch on both boards, then reported a successful physical
cold-boot check. The number of cold cycles and OFF interval were not specified.
No post-cold-boot API trace was taken by this preparation. This feedback closes
the user's current test, not an invented repeated-cold or Full Install series.
The raw morning report remains unchanged; this section records later feedback.

## Rename contract

New firmware emits `7_dual_i2c`, build suffix/artifact slug `7-dual-i2c` and
source version `1.2.0-beta`. Environment `project_aura_7`, target `aura-aq-7-v1`,
production flavor, native USB, GPIO routing, bus speeds and GT911 selection
remain unchanged. The producer rejects stale or mixed identity fields.

Consumers must accept both profile names only for the same 7-inch target,
preserve raw signed metadata and each release's actual profile, and keep old
enrolled identities valid. Database support is an additive migration, prepared
locally but not executed. Alias equivalence must not relax cross-target,
signature, provenance or server-owned-device-identity checks.

The existing exact USB beta version gate remains pinned to its old approved
candidate. New-profile import compatibility is not permission to publish
`1.2.0-beta`. All 7-inch service OTA denial and Stable/Recovery restrictions
remain unchanged. Final candidate promotion needs a separate reviewed decision
after exact-artifact validation and a live state check.

## Final artifacts and remaining actions

Local software-verification logs, final artifact hashes and component commit
IDs are recorded under
`D:\21cncstudio\project_aura\logs\release_1_2_local_20260903T091528Z`.
That directory's final receipt is authoritative for what actually completed;
the existence of this plan is not a build, package or deployment PASS.

New clean `1.2.0-beta` images remain unflashed until separately approved and
identified for the correct physical board. The preceding overnight and
operator cold-boot results do not transfer to the rebuilt BINs. A short final
hardware smoke check remains, and Full Install qualification is separate.

Before any future publication: review consumer commits, apply only the
approved migration/deploy sequence, recheck remote and live gates, bind any
new version approval to exact hashes, and preserve the separate 4.3-inch and
7-inch release pointers. Do not run an old one-shot flash script or overwrite
historical packages.
