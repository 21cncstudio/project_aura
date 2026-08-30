# Post-v1.1.5 cleanup and 7-inch profile review

Date: 2026-08-30

## Status

The source cleanup and the production firmware split are complete on the
local branch `codex/post-115-cleanup`.

Current reviewed firmware commit:

```text
b44b7a6f72068e222e9a2c6ec0254eb8a0c71995
Add production 7-inch hardware profile
```

This is not a release approval. The firmware source and both production builds
are ready for controlled physical validation, but the release and installer
pipeline is still single-target and must not publish the 7-inch image yet.

No serial port was opened or flashed during this cleanup review.

## Source boundaries

The cleanup did not copy the dirty COM7 diagnostic tree. It was reconstructed
as a narrow commit chain on top of the current `origin/main`:

```text
v1.1.5             8535723beddc696d974df94ea16111a5881f525a
origin/main        da92fcce310924ec43f7be4378dee5ced0f40c48
diagnostic beta    7437b2c782cc3e38e63e909f8ef29e30303facb4
clean review HEAD  b44b7a6f72068e222e9a2c6ec0254eb8a0c71995
```

Audited ranges:

- `v1.1.5..origin/main`: 85 commits.
- `origin/main..7437b2c`: 28 later diagnostic and recovery commits.
- `origin/main..b44b7a6`: 16 clean commits.

The three largest mixed commits were not transferred or reverted wholesale:

| Commit | Files | Insertions | Deletions | Decision |
| --- | ---: | ---: | ---: | --- |
| `a0642cd` | 100 | 7472 | 602 | Rework only independently justified parts |
| `bee515b` | 93 | 8278 | 550 | Rework only independently justified parts |
| `51f07ec` | 86 | 11907 | 733 | Reject wholesale; replace only the proven 7-inch CH422G behavior |

## Clean commit chain

| Clean commit | Result |
| --- | --- |
| `ce77ef1` | Backport ESP32-S3 restart cache fix |
| `0d0e15b` | Pin HTTP server to the network core |
| `0705f02` | Preserve pressure history until wall time is trusted |
| `6b955bd` | Keep DFR gas startup read-only |
| `028e3e6` | Treat an absent optional gas probe as informational |
| `5311c6d` | Fix daily history loop stack overflow |
| `913e2fd` | Reconcile persisted histories with trusted time |
| `f33f161` | Return the newest support errors |
| `4b73d6f` | Harden MQTT client teardown ownership |
| `18393d7` | Fix board-init task ownership race |
| `da56c52` | Probe GP8403 without undocumented register reads |
| `3750c90` | Harden the canonical native-test launcher |
| `1bf07d8` | Promote Last Known Good only after healthy runtime dwell |
| `5375b52` | Disable unproven automatic panel recovery actions |
| `3fd2a88` | Add explicit hardware-profile build identities |
| `b44b7a6` | Add the production 7-inch dual-I2C profile |

Each fix is independently reviewable. None of these commits imports Aura Link
or Aura Hub code.

## Audit of v1.1.5 through origin/main

Seventy-six of the 85 commits in `v1.1.5..origin/main` are ordinary product,
storage, OTA, network, UI, and sensor changes that remain the baseline. No
unrelated Aura Link or Aura Hub implementation was found in this range.

Nine commits mix useful lifecycle or diagnostic work with panel-recovery
hypotheses. Their history is retained because `origin/main` is the base, but
their active behavior is not accepted wholesale:

| Baseline commit | Cleanup disposition |
| --- | --- |
| `848333f` | Keep lifecycle observation; automatic GPIO/I2C recovery remains off |
| `51beeb1` | Keep containment/readiness; one-shot startup restart remains off |
| `11f8f4c` | Keep cold-start classification; CH422G preflight remains off |
| `1f3f195` | Keep guarded backlight/LVGL wake; do not treat it as touch-offline recovery |
| `a0642cd` | Keep independently reviewed driver/readiness/OTA parts; disable active recovery hypotheses |
| `11bd987` | Keep owner/drain mechanisms; automatic shared-bus shutdown/restart remains off |
| `e78e9c3` | Keep address selection and passive diagnostics; runtime CH422G hard recovery remains off |
| `9a95278` | Do not use startup pacing or CH preflight as treatment; decide shutdown policy separately |
| `f4dd7d5` | Keep guarded wake; its mixed MQTT/UI parts are superseded by narrow clean fixes |

This is a behavior cleanup, not a destructive rewrite of published `main`
history. The production-off gates are explicit and covered by native tests.

## Transfer decisions for the later 28 commits

### Exact patch-equivalent transfers

`git cherry` and direct patch comparison confirmed these narrow equivalents:

| Diagnostic-chain commit | Clean commit |
| --- | --- |
| `d1e2cd3` | `ce77ef1` |
| `1033386` | `0d0e15b` |
| `66339f0` | `0705f02` |
| `55276e8` | `6b955bd` |
| `7eab073` | `028e3e6` |

### Reworked, not copied

| Original area | Clean representation |
| --- | --- |
| Stack portion of `9190d78` | `5311c6d` |
| History portions of `8222fbb` and `bee515b` | `913e2fd` |
| MQTT teardown portion of `bee515b` | `4b73d6f` |
| Board task lifecycle portion of `bee515b` | `18393d7` |
| Web event ordering portion of `6ae2ed5` | `f33f161` |
| Launcher portion of `3b099b2` | `3750c90` |
| Build identity intent of `2ca3c96` | `3fd2a88` |
| Useful 7-inch CH422G reset behavior from `51f07ec` | Fresh profile-only fork in `b44b7a6` |

The mixed recovery commits already present between v1.1.5 and `origin/main`
were also treated as policy input, not accepted as one indivisible solution.
`5375b52` keeps passive diagnostics and guarded wake behavior while production
flags disable the unproven automatic CH422G preflight, pre-init bus recovery,
GT911 hard recovery, touch-offline restart, and stuck-panel-bus restart paths.

### Rejected as active production behavior

The following hypotheses did not stabilize COM7 and were not carried into the
new 7-inch profile as active recovery actions:

- `ceb44d6`: durable automatic recovery lifecycle.
- `0c0e953`: automatic headless restart.
- `78684d9`: automatic shared-I2C recovery decisions.
- `ce5ffc5`: CH422G startup retry loop.
- `7276f95`: routing runtime I2C faults directly into restart.
- `0d69000`: pre-init I2C controller recovery.
- `3bfefe6`: GPIO clocking and bus sanitization before restart.
- `35d4b2e`: shared-bus startup pacing as a treatment.
- `311d32f`: extra pre-init settle delay as a treatment.
- `353b7dd`: provisioning writes during DFR startup.
- `51f07ec`: the complete mixed vendor fork, retries, forced writes,
  diagnostics, and recovery policy.

This also rejects the idea that 50 kHz panel I2C, 14 MHz PCLK, CAP1, a new USB
cable, or selective touch fail-open established a stable COM7 solution.

### Deferred for a separate policy decision

These ideas may still be useful, but they are not required by the new physical
topology and must not be smuggled into the profile implementation:

- `b86d29f`: passive CH422G diagnostics.
- `dd13161`: bounded fail-closed shutdown. It can protect SD, but it may leave
  a controller headless until physical power removal.
- `3a61bb5`: additional restart barriers, breadcrumbs, and network drain.
- `d653b65`: startup and address diagnostics without automatic recovery.
- `e2988c0`: Wi-Fi preservation only with an explicit crash-loop policy.
- `7437b2c`: diagnostic read admission after a rejected restart, only together
  with a defined support-report endpoint.

## Production hardware profiles

| Property | 4.3-inch | 7-inch |
| --- | --- | --- |
| PlatformIO environment | `project_aura` | `project_aura_7` |
| Hardware profile | `4_3` | `7_dual_i2c_scl6` |
| USB/logging | Existing profile behavior | Native USB CDC on Type_C2 |
| Panel I2C host | I2C0 | I2C0 |
| Panel SDA/SCL | GPIO8/GPIO9 | GPIO8/GPIO9 |
| External sensors | Shared panel I2C0 | Separate I2C1 |
| Sensor SDA/SCL | GPIO8/GPIO9 | GPIO44/GPIO6 |
| Sensor pull-ups | Existing shared-bus behavior | ESP32 internal pulls disabled; module pulls used |
| CH422G library | Official upstream v1.1.0 | Profile-only reviewed fork of upstream v1.1.0 |
| H3 EX_TXD/GPIO43 | Not repurposed | Must remain disconnected from sensor SCL |
| J8 Sensor AD/GPIO6 | Available as original input | Used as sensor SCL; analog Sensor AD is unavailable |
| SW1 | Existing product setup | UART2 while H3 EX_RXD carries SDA |

The profile selector is the source of truth. Build generation rejects unknown
profiles and rejects a mismatched profile/build-ID suffix. A build-time routing
check also rejects future external-sensor drivers that use panel-I2C constants.

## Frequency and panel decisions

The panel bus remains at 100 kHz. This value predates the COM7 experiments in
the Project Aura configuration, so it is not leftover diagnostic throttling.
The pinned Waveshare library can use a 400 kHz panel bus, but there is no
current product need or controlled physical validation that justifies changing
the stable Project Aura baseline during this cleanup.

The 7-inch external sensor bus is also 100 kHz, matching the physically tested
SDA44/SCL6 route. Its internal ESP32 pull-ups remain disabled because the
attached modules provide external 3.3 V pull-ups.

The display PCLK remains 16 MHz. That is vendor-equivalent for the selected
7-inch panel and was not reduced to the rejected 14 MHz experiment.

## CH422G scope

The official CH422G implementation is untouched for 4.3-inch firmware. Only
the 7-inch build links `third_party/ESP32_IO_Expander_7`.

The fork is based on upstream `ESP32_IO_Expander` v1.1.0 commit
`e79a63876a1d8a834cf8ec8f8b698ff9d9374579`. All vendored runtime files were
compared with upstream; the only semantic differences are:

1. Default `WR_IO` is `0xD1`, keeping `USB_SEL` low for native Type_C2.
2. The output image is written before IO0-7 are enabled as outputs.

Expected initial writes:

```text
WR_OC  0x23 <- 0x0F
WR_IO  0x38 <- 0xD1
WR_SET 0x24 <- 0x01
```

There are no retry loops, shadow-cache success claims, forced runtime writes,
or automatic recovery additions in this fork. Native tests assert the exact
order, reject `0xFF`, and verify that each failure stops without retry.

## I2C fault domains

The 4.3-inch profile preserves the previous shared-bus startup order and
failure behavior.

The 7-inch profile treats panel I2C0 and sensor I2C1 as separate fault domains:

- A panel/GT911/CH422G startup failure does not make the sensor host unavailable.
- RTC, environmental sensors, optional gas sensors, and GP8403 all use the
  sensor-bus constants and I2C1 host.
- A controlled restart drains and accounts for both domains.
- Safe sensor/DAC shutdown on the 7-inch profile does not require a healthy
  panel drain or LVGL acknowledgement, but it still requires the sensor domain
  to be idle.
- On 4.3-inch hardware the same operation remains fail-closed across the shared
  panel and sensor owners.

## Build and automated test evidence

Both production environments were rebuilt from clean commit `b44b7a6`:

| Environment | Build ID | RAM | Flash | Firmware SHA-256 |
| --- | --- | ---: | ---: | --- |
| `project_aura` | `b44b7a6` | 157860 / 327680 | 4297230 / 6553600 | `98A400D4634C20B805935C8CDEF066563C86ACBC613C17C3CBD6D21333CAD7D3` |
| `project_aura_7` | `b44b7a6-7_dual_i2c_scl6` | 157956 / 327680 | 4291598 / 6553600 | `B0A8AA65351000D5163BECA1CF6798E4867B62FB89C0994E975C7DEC3167360D` |

Both builds passed:

- sensor-routing guard over 15 source files;
- retained RTC no-init ABI check;
- ESP32-S3 restart/cache linker check;
- correct dependency resolution: official CH422G for 4.3-inch and only the
  vendored profile fork for 7-inch.

The post-commit canonical native matrix passed 816 of 816 cases:

```text
.pio/native-tests/reports/20260830T044301Z-d8f84407/launcher.json
status=PASSED
```

The total includes the main 740-case suite plus dedicated SFA30, SFA40, DFR,
GP8403, 4.3 topology, 7-inch topology, CH422G reset, and startup-policy suites.

`git diff --check` and `git diff --cached --check` were clean before the
firmware commit. The worktree was clean after the post-commit builds and tests.

## Physical evidence and its limits

The retained physical evidence lives outside this clean worktree under:

```text
D:\21cncstudio\project_aura\logs\COM10_7IN_SCL6_I2C_HANDOFF_20260829.md
D:\21cncstudio\project_aura\logs\firmware_candidate_7_DUAL_I2C_RAW_ACK_COM10_20260829
D:\21cncstudio\project_aura\logs\firmware_candidate_7_SCL6_NORMAL_COM10_20260829
```

The controlled T10 physical cold boot established the routing result:

- Sensor I2C1 was SDA44/SCL6 with both lines high at idle.
- GPIO43/H3 EX_TXD was physically open from SCL.
- `0x58`, `0x5D`, `0x68`, `0x6B`, `0x74`, and `0x77` all ACKed.
- GP8403, PCF8523, BMP388, SEN66, SEN0466, and SFA30 coexisted.
- Display, first frame, and touch worked.
- No panic, second reset, USB loss, or automatic restart occurred in the
  bounded 180-second capture.

The T05-T10 bracket rejects an I2C address conflict and a GP8403 command
conflict. The bracket isolated the destructive condition to the old
GPIO43/H3 SCL path, and moving SCL to direct GPIO6 removed that path. R107
under the combined pull-up load is the leading electrical explanation, not a
directly proven cause; the bracket did not independently separate R107 from
FSUSB42, H3, the connector, or wiring.

The later normal-profile no-SFA30 capture was inspected over a bounded window
of approximately 3 hours 53 minutes without the prior failure pattern. This is
useful evidence, not a completed release qualification. The original handoff
requested a longer soak and repeat exact-normal-image cold boots.

The dirty diagnostic binaries and their logs prove hardware observations only.
They do not prove that the clean `b44b7a6` binaries were physically flashed.

Remaining physical validation gaps:

1. Flash the exact clean `project_aura_7` artifact and record its build ID and
   SHA-256 in the capture.
2. Run controlled 7-inch physical cold boots with the final wiring and full
   sensor/DAC set.
3. Repeat the no-SFA30 long-run observation with a normally closed receipt.
4. Run at least one 4.3-inch physical regression on the exact clean 4.3 image.
5. Keep upload reset, EN reset, software restart, USB reconnect, and physical
   OFF/ON results classified separately.

## Release and installer blocker

The firmware profiles must not be published with the existing packaging flow.
The review found a separate P1 release-safety issue:

- Both environments currently write the same release directory and generic
  artifact names, so one profile can overwrite the other.
- The producer currently creates signature-v1 packages while the current Aura
  Link importer requires the signature-v2 canonical payload. Adding fields is
  not sufficient: `signature.schema`, `title`, `release_notes_sha256`, and
  `asset_kind` must participate in the signed payload. The outer package schema
  remains `aura-firmware-release-package-v1`.
- Aura Link currently admits only `aura-aq-v1`, keys releases without hardware
  target, and stores one current pointer per product/channel.
- The installer and OTA selection are also product/channel based, so publishing
  the 7-inch image as the old target could replace the 4.3-inch current image.
- GitHub publication with `-PruneAssetsToList` can delete the other profile's
  OTA asset if the command is run once per profile with an incomplete list.

Immediate stop conditions:

- Do not import newly produced signed ZIP files for either `project_aura` or
  `project_aura_7` until the producer emits the signature-v2 contract accepted
  by the real Aura Link importer.
- Do not alter existing published 4.3-inch releases while implementing the new
  package path.
- Do not publish the 7-inch image through the installer, GitHub OTA assets,
  the public OTA catalog, or recovery packages until target isolation is live.

Required target model:

| Firmware environment | Hardware target | Hardware profile |
| --- | --- | --- |
| `project_aura` | `aura-aq-v1` | `4_3` |
| `project_aura_7` | `aura-aq-7-v1` | `7_dual_i2c_scl6` |

These are strict pairs. The importer must reject `aura-aq-v1` with the 7-inch
profile and reject `aura-aq-7-v1` with the 4.3-inch profile.

The safe release implementation requires coordinated changes in three scopes:

1. Project Aura producer:
   - use target-specific outer directories, ZIP names, and public OTA names;
   - keep the importer's required generic names inside each ZIP:
     `bootloader.bin`, `partitions.bin`, `boot_app0.bin`, `firmware.bin`,
     `littlefs.bin`, `release-notes.md`, and `release.json`;
   - read the actual generated build ID, profile, and target from the selected
     PlatformIO environment and include them in signed metadata;
   - implement the complete signature-v2 canonical payload while retaining the
     package-v1 outer schema;
   - either publish both OTA assets with one complete prune list or disable
     `-PruneAssetsToList` for dual-profile tags;
   - keep legacy static manifests profile-local unless every public URL and BIN
     is made profile-qualified.
2. Aura Link on `feature/aura-link`:
   - make the release uniqueness key include `hardwareTarget`;
   - make the current-pointer uniqueness key include `hardwareTarget`;
   - scope publish, rollback, and conflict handling by target;
   - transactionally backfill existing releases and pointers as `aura-aq-v1`;
   - keep a missing target in a legacy installer request equivalent to
     `aura-aq-v1`;
   - enforce the strict target-profile pairs in the import allowlist;
   - keep 7-inch OTA disabled until enrollment/device metadata reports an
     explicit hardware target. A missing device target means 4.3-inch only and
     must never be inferred from version or channel.
3. Aura AQ site:
   - add explicit hardware selection and target propagation;
   - verify that the returned manifest target matches the user's selection
     before erase or write;
   - make public OTA catalog lookup include target before listing two images
     with the same product/version;
   - remove the display-only 7-inch block only after the backend supports both
     targets.

The existing recovery package is 4.3-inch-only. A 7-inch recovery package needs
its own target-specific binary, packaging, and physical validation.

Minimum release acceptance tests:

- A real producer ZIP is accepted by the real Aura Link importer.
- Two packages with the same version and channel but different targets coexist.
- A target-profile mismatch is rejected.
- Publish or rollback of one target does not change the other target.
- A legacy request without target resolves only to the 4.3-inch image.
- A 7-inch manifest contains its target and the site verifies it before write.
- Signed build ID, profile, and target match the selected environment's
  generated identity.

Those repositories and branch scopes were deliberately not modified by this
firmware cleanup. Until the coordinated release work is complete, the 7-inch
profile is source- and build-ready but unpublished.

## Final decision

| Question | Decision |
| --- | --- |
| Carry the dirty 28-commit chain wholesale? | No |
| Keep all of `origin/main` as an unexamined recovery solution? | No; active recovery paths were explicitly gated off |
| Separate 4.3-inch and 7-inch firmware builds? | Yes |
| Move the 7-inch external sensor chain to I2C1 SDA44/SCL6? | Yes |
| Change the panel bus to 400 kHz during cleanup? | No |
| Return PCLK to 16 MHz? | Already done and vendor-equivalent |
| Publish the 7-inch image now? | No; target-aware release pipeline is required first |
| Proceed to exact clean-artifact physical validation? | Yes, as the next controlled stage |
