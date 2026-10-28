# Standalone main push and beta preparation — 2026-10-28

The owner authorized merging/pushing this work to GitHub main and preparing a
new beta for the web installer. Branch renaming was limited to this work:
`main-logo-rgb565` already has no prefix; existing archive and other branches
were preserved.

## Source and scope

Built, packaged and pushed source: `1625a09bc402d34ce6facd87e18993e7b79e4e51`.
Normal fast-forward push advanced GitHub main from `2633900043bb7cc590e75c16c4a54257775f9824`.
GitHub accepted the owner's existing bypass for the expected CLA status; branch
protection was not modified. Remote HEAD was checked with ls-remote and GitHub API.
The later commit recording this report changes documentation only; the binaries
retain source identity `1625a09`, not that later documentation commit.

The merge joins HTTP diagnostics (`cb86fc8f`) and the two tested logo commits
(`1181f217`, `7ab5cb4a`). Only CURRENT_STATE history conflicted; both records were
retained. UI/EEZ and logo source match the tested logo branch. All 14 removed
Hub-only commits remain outside main ancestry. No Hub/archive branch or tag was
pushed, no site/backend was deployed and no production beta pointer changed.

## Verification

- Both ESP-IDF 6.1 profiles built from clean source, all five artifact stamps valid.
- 1,167 native tests across 10 environments; 114 tooling tests; 18 packaging tests.
- Five HTTP SDK TCP scenarios passed their expected success/failure assertions.
- EEZ postprocess check and diff checks passed.
- Linked RGB565 logo SHA256 matches the qualified asset in both ELFs:
  `07d6dd0775640996ccff7f9f3b366387e8e486093d7756747a34adf3d1ab3479`.
  Obsolete LVGL8 logo symbol is absent; both images use 50 XIP pages.
- All 10,812 snapshotted dependency/config files retain their hashes. Component
  Manager used an isolated lock and exact constraints; no driver/SDK update entered.
- Both signed ZIPs passed the backend package verifier: Ed25519, five binaries,
  sizes, hashes, offsets and hardware identity; both cross-target requests rejected.
  Verifier checkout: `D:/21cncstudio/aura-aq-site/tmp/reseller-access-link`, HEAD
  `79513393239934956f3f1901640ada4df217f257`. No database or production import used.
  The first check against the older `aura-link` working checkout rejected native
  IDF's erased OTA initialization; the published-line verifier includes the
  already-established support. No validator or binary was weakened/modified.

## Exact artifacts

| Profile | App bytes | App SHA256 | Static PSRAM bytes |
| --- | ---: | --- | ---: |
| 4_3 | 3389088 | `5912583343595e0926da481ae5b1966cf4d7c831e86356f202b52f5294d5e672` | 3256232 |
| 7_dual_i2c | 3391232 | `abcf4a7b375c6c2c941d940c8b8676da36b1f8c5be99b4e860bf96b50618d258` | 3257052 |

Packages are local, signed with the existing installer key, and **not published**.
No GitHub Release/tag was created.

- aura-aq-v1: `1.2.1-beta-1625a09`, ZIP 1979616 bytes, SHA256 `0c02b7e6ac995d5c1cb233546f919c94f28845e3b3a44c0d3e04a0daf7c8858b`.
  [Package](D:/21cncstudio/project_aura_aq/firmware-main/release-assets/20261028-1625a09/v1.2.1-beta/aura-aq-v1/aura-aq-v1-v1.2.1-beta-1625a09-beta.aura-release.zip).
- aura-aq-7-v1: `1.2.1-beta-1625a09-7-dual-i2c`, ZIP 1982589 bytes, SHA256 `c83c877546a090ede24ffdc975b3c59730b3a106131190a038672b108770016c`.
  [Package](D:/21cncstudio/project_aura_aq/firmware-main/release-assets/20261028-1625a09/v1.2.1-beta/aura-aq-7-v1/aura-aq-7-v1-v1.2.1-beta-1625a09-7-dual-i2c-beta.aura-release.zip).

The local package directory also contains `packages.json`, manifests, release
notes and OTA BINs. Existing October 7 packages and prior tested BINs are preserved.

## Sensors and remaining release work

Owner priority is SEN69C, SFA40 and SEN66; other working sensor drivers were left
alone. Their official releases were checked: SEN69C 1.1.0 and SFA4x 1.0.0 have no
changes since the October 7 audit; SEN66 1.3.1's corrected integer-read command
0x0300 is already used. All three driver files match Main, Aura-to-Hub and the Hub
reference checkout. [Full update audit](SENSOR_DRIVER_UPDATES_20261028.md).

These exact combined BINs have not been flashed. The owner's logo confirmation
and prior app-only/readback check apply to the earlier logo-only `ff3a8a6` BIN.
HTTP diagnostic peak RAM/stack and on-device transfer cases remain unqualified;
issue #158's cause remains unknown. Earlier overnight results do not qualify this
new candidate, SD/OTA recovery or 7-inch hardware.

Before publication, select/complete the agreed exact-BIN hardware checks, inspect
live site/backend state, prepare their exact-version policies (especially 7-inch),
then separately authorize Aura Admin import/promotion and any required deployment.
Keep Stable 1.2.0 unchanged and retain model/migration guards. These packages are
ready for review and local import validation, not a claim of live availability.

Local evidence: `build/main-beta-20261028/` (build/test logs, pinned inputs,
verified artifacts, package verification and upstream audit); native runner
`.pio/native-tests/reports/20261028T165647Z-70ab0992/`.
