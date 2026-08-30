# Post-v1.1.5 cleanup and 7-inch profile review

Date: 2026-08-30

This is the historical `fac6e30` cleanup/build/package snapshot, not the latest
flash candidate. Later commits `0b68e92` (panel sampling), `35001ed` (optional
sensor UI alerts), and the [built-in OTA hardware guard](OTA_HARDWARE_TARGET_GUARD.md)
supersede its next-test artifact selection. Hashes, package acceptance, and
physical-validation statements below apply only to the named historical
artifacts; they are not evidence for newer BINs or a public release approval.
The current profile and CH422G sections below also reflect the later
[native-USB migration](USB_CONSOLE_PROFILES.md); the `fac6e30` build and test
records retain their historical UART/upstream 4.3-inch configuration.

## Status

The source cleanup, production firmware split, and target-aware producer are
complete on the local branch `codex/dual-profile-release`.

Reviewed firmware commit in this snapshot:

```text
fac6e307b3d8df35db891b56f7022dbf89000d79
Make CH422G diagnostics profile safe
```

This is not a release approval. The firmware source, both exact production
builds, and the local dual-target packaging/control-plane/installer
implementation are ready for controlled physical validation. External 7-inch
publication remains locked until the exact clean artifact completes physical
qualification, a trusted 7-inch identity enrollment/attestation path is
implemented and reviewed, and the coordinated non-rolling control-plane
cutover is completed.

No serial port was opened or flashed during this cleanup review.

## Source boundaries

The cleanup did not copy the dirty COM7 diagnostic tree. It was reconstructed
as a narrow commit chain on top of the current `origin/main`:

```text
v1.1.5              8535723beddc696d974df94ea16111a5881f525a
origin/main         da92fcce310924ec43f7be4378dee5ced0f40c48
diagnostic beta     7437b2c782cc3e38e63e909f8ef29e30303facb4
runtime cleanup HEAD 989b323785fba0b05c930ed95d234136c64902e4
producer checkpoint  0a6ed35133853375109a91bb7cdf9aa73d4055c0
current source HEAD  fac6e307b3d8df35db891b56f7022dbf89000d79
```

Audited ranges:

- `v1.1.5..origin/main`: 85 commits.
- `origin/main..7437b2c`: 28 later diagnostic and recovery commits.
- `origin/main..989b323`: exactly 19 local commits: 18 implementation commits
  plus the first audit-document commit.
- `origin/main..0a6ed35`: 24 local commits after the target-aware producer and
  its release identity follow-up were added.
- `origin/main..fac6e30`: exactly 25 local commits before this final
  documentation update: 23 implementation commits plus the two audit-document
  commits `437c4e5` and `ba8f693`.

The three largest mixed commits were not transferred or reverted wholesale:

| Commit | Files | Insertions | Deletions | Decision |
| --- | ---: | ---: | ---: | --- |
| `a0642cd` | 100 | 7472 | 602 | Rework only independently justified parts |
| `bee515b` | 93 | 8278 | 550 | Rework only independently justified parts |
| `51f07ec` | 86 | 11907 | 733 | Reject wholesale; replace only the proven 7-inch CH422G behavior |

## Clean implementation chain

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
| `43c5f85` | Require the sensor host for OTA and Last Known Good validation |
| `989b323` | Harden build identity and native-launcher self-tests |
| `6e90340` | Identify dual-profile 1.1.6 beta builds |
| `dbbeae9` | Protect dual-profile firmware releases |
| `99202c5` | Fix release partition offset parsing |
| `0a6ed35` | Normalize seven-inch release identity |
| `fac6e30` | Keep the disabled CH422G diagnostic profile-safe |

Each fix is independently reviewable. None of these commits imports Aura Link
or Aura Hub code. The audit-only `437c4e5` and `ba8f693` commits are
intentionally omitted from the implementation table.

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

## Production hardware profiles (current native-USB policy)

These profile settings describe the native-USB source update. Select and
record fresh exact-source artifacts for physical checks; the `fac6e30` hashes
and test totals elsewhere in this document do not validate this update.

| Property | 4.3-inch | 7-inch |
| --- | --- | --- |
| PlatformIO environment | `project_aura` | `project_aura_7` |
| Hardware profile | `4_3` | `7_dual_i2c_scl6` |
| Hardware target | `aura-aq-v1` | `aura-aq-7-v1` |
| USB/logging | Native USB CDC on the USB Type-C connector | Native USB CDC on Type_C2 |
| Panel I2C host | I2C0 | I2C0 |
| Panel SDA/SCL | GPIO8/GPIO9 | GPIO8/GPIO9 |
| External sensors | Shared panel I2C0 | Separate I2C1 |
| Sensor SDA/SCL | GPIO8/GPIO9 | GPIO44/GPIO6 |
| Sensor pull-ups | Existing shared-bus behavior | ESP32 internal pulls disabled; module pulls used |
| CH422G library | Shared reviewed `ESP32_IO_Expander_Aura` fork of v1.1.0 | Same shared fork |
| CH422G initial `WR_IO` | `0xDF`, only USB_SEL cleared from `0xFF` | `0xD1`, unchanged |
| H3 EX_TXD/GPIO43 | Not repurposed | Must remain disconnected from sensor SCL |
| J8 Sensor AD/GPIO6 | Available as original input | Used as sensor SCL; analog Sensor AD is unavailable |
| SW1 | Existing product setup | UART2 while H3 EX_RXD carries SDA |

The profile selector is the source of truth. Build generation rejects unknown
profiles and rejects a mismatched profile/build-ID suffix. A build-time routing
check also rejects future external-sensor drivers that use panel-I2C constants.

## Frequency and panel decisions

The panel bus remains at 100 kHz. Direct comparison of `v1.1.5`, `origin/main`,
and the current custom-board configuration confirms the same value, so it is
not leftover diagnostic throttling.
The pinned Waveshare library can use a 400 kHz panel bus, but there is no
current product need or controlled physical validation that justifies changing
the stable Project Aura baseline during this cleanup.

The 7-inch external sensor bus is also 100 kHz, matching the physically tested
SDA44/SCL6 route. Its internal ESP32 pull-ups remain disabled because the
attached modules provide external 3.3 V pull-ups.

The display PCLK remains 16 MHz. That is vendor-equivalent for the selected
7-inch panel, matches `v1.1.5`, and was not reduced to the rejected 14 MHz
experiment.

## CH422G scope (current native-USB policy)

Both builds link `third_party/ESP32_IO_Expander_Aura`, renamed from the former
7-inch-only vendor directory. This supersedes the historical `fac6e30` split
between official upstream for 4.3-inch and the fork for 7-inch.

The fork is based on upstream `ESP32_IO_Expander` v1.1.0 commit
`e79a63876a1d8a834cf8ec8f8b698ff9d9374579`. The current policy is:

1. Default `WR_IO` is `0xDF` for 4.3-inch and the unchanged `0xD1` for 7-inch,
   keeping `USB_SEL` low. The 4.3-inch value changes only bit 5 from `0xFF`.
2. The output image is written before IO0-7 are enabled as outputs.

Expected initial writes:

```text
WR_OC  0x23 <- 0x0F
WR_IO  0x38 <- 0xDF (4.3-inch) or 0xD1 (7-inch)
WR_SET 0x24 <- 0x01
```

There are no retry loops, shadow-cache success claims, forced runtime writes,
or automatic recovery additions in this fork. The native test contract requires
the exact profile image/order and a stop without retry on each failed write.

The disabled legacy `Ch422gReadyProbe` now shares
`include/Ch422gBoardPolicy.h` with the driver: `0xDF` for 4.3-inch and `0xD1`
for 7-inch. CAN is unused; `Logger`, I2C routing/frequency, and the 7-inch
LCD/touch/backlight startup levels are unchanged by this migration. The
[USB transition checks](USB_CONSOLE_PROFILES.md) require a 4.3-inch basic pass
before disturbing the existing 7-inch capture.

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
- OTA validation and Last Known Good promotion require the sensor I2C host to
  be ready. This checks host installation only: absence or NACK of any
  individual optional RTC, sensor, or DAC does not invalidate the image.

## Final holistic review follow-up

Four confirmed review findings were fixed after the first audit document:

1. A 7-inch boot with a failed I2C1 driver installation could previously
   confirm OTA and accumulate Last Known Good dwell through a healthy display.
   `43c5f85` adds the sensor-host structural gate to both decisions. The 4.3-inch
   result is unchanged because its sensor readiness follows the panel host.
2. The canonical launcher had grown from six to nine environments, while its
   Python self-test and `TESTING.md` still expected six. `989b323` updates the
   inventory checks and documents all topology and CH422G suites.
3. Build identity ignored untracked files. An untracked source or header could
   therefore participate in a binary whose ID still looked commit-clean.
   `989b323` now includes untracked files in dirty detection and has an
   integration regression test.
4. The disabled legacy `Ch422gReadyProbe` still used the 4.3-inch `0xFF`
   output image. If enabled later in a 7-inch build, it could drive
   EXIO5/USB_SEL high and switch away from native Type_C2. `fac6e30` selects
   `0xD1` for the 7-inch profile and adds a topology regression assertion while
   preserving `0xFF` for 4.3-inch diagnostics.

The GPIO43/R107 source comment was also narrowed to the physical evidence:
the old route failed under combined load, while R107 remains the leading
electrical explanation rather than an independently proven cause.

Independent final source passes found no additional P0-P2 regressions in the
cleaned commit range after these fixes. Exact `fac6e30` build, package, and
verifier validation also completed successfully as recorded below.

## Known pre-v1.1.5 concurrency debt

The review reconfirmed one real baseline issue outside the audited regression
range: `StorageManager::config()` exposes mutable references while config reads,
mutations, and persistence can run on different tasks/cores. This behavior was
already present in `v1.1.5`; `origin/main..989b323` did not add or materially
strengthen that failure path.

The LKG mutex serializes durable writes and promotion of the already persisted
config bytes. It does not make arbitrary `config()` access thread-safe, and the
code must not be described that way. A correct repair needs a snapshot API for
readers and a locked transaction API covering mutation, persistence, rollback,
and WiFi/MQTT certificate sidecars. Locking only `saveConfig()` or only a few
setters would leave unsafe references and create a false guarantee, so that API
migration is deliberately a separate focused task rather than a partial change
in this hardware cleanup.

## Build and automated test evidence

Both production environments were rebuilt from exact clean source commit
`fac6e307b3d8df35db891b56f7022dbf89000d79`:

The durable software-evidence root is:

```text
D:\21cncstudio\project_aura\logs\firmware_candidate_fac6e30_DUAL_PROFILE_BUILD_EVIDENCE_20260830
```

| Environment | Build ID | RAM | Flash | BIN bytes | Firmware SHA-256 |
| --- | --- | ---: | ---: | ---: | --- |
| `project_aura` | `fac6e30` | 157860 / 327680 | 4297358 / 6553600 | 4297744 | `45BB0ADE4E673EEA4E13B3F09D56987A3135B5EBEE00BD67467F43BF065707CF` |
| `project_aura_7` | `fac6e30-7-dual-i2c-scl6` | 157956 / 327680 | 4291746 / 6553600 | 4292128 | `5B85F49BE8FE30FE213AAA216789CC91C751DDB0043050761A142E464340E4FB` |

Both builds passed:

- sensor-routing guard over 15 source files;
- retained RTC no-init ABI check;
- ESP32-S3 restart/cache linker check;
- correct dependency resolution: official CH422G for 4.3-inch and only the
  vendored profile fork for 7-inch.

The exact-source canonical native matrix passed 816 of 816 cases across 106
suites and nine invocations:

```text
D:\21cncstudio\project_aura\logs\firmware_candidate_fac6e30_DUAL_PROFILE_BUILD_EVIDENCE_20260830\native-tests\20260830T093109Z-74d73bce\launcher.json
status=PASSED
```

The total includes the main 740-case suite plus dedicated SFA30, SFA40, DFR,
GP8403, 4.3 topology, 7-inch topology, CH422G reset, and startup-policy suites.
The final producer checks also passed 63 Python tests, 20 Node package tests,
and all four PowerShell release-layout tests. The test-signed 4.3-inch and
7-inch ZIPs were both accepted by the real Aura Link verifier with their exact
target/profile/environment/build identity. A cross-target verification attempt
was rejected. The integration packages remain test-key artifacts, not release
assets:

| Target | Effective version | Package SHA-256 |
| --- | --- | --- |
| `aura-aq-v1` | `1.1.6-beta-fac6e30` | `8EB074F46A9D3C26983B7D051DC6E28DCC4F257D48AD243E6953AB8B02067954` |
| `aura-aq-7-v1` | `1.1.6-beta-fac6e30-7-dual-i2c-scl6` | `F0AAEF9C334C1C8831F9600C19B9AEED88FC6958B4770202AAEAFE524939F7D8` |

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
They do not prove that the exact clean `fac6e30` binaries were physically
flashed.

Remaining physical validation gaps:

1. Flash the exact clean `project_aura_7` `fac6e30-7-dual-i2c-scl6` artifact
   and record its SHA-256 in the capture.
2. Run controlled 7-inch physical cold boots with the final wiring and full
   sensor/DAC set.
3. Repeat the no-SFA30 long-run observation with a normally closed receipt.
4. Run at least one 4.3-inch physical regression on the exact clean 4.3 image.
5. Keep upload reset, EN reset, software restart, USB reconnect, and physical
   OFF/ON results classified separately.

## Release and installer safety follow-up

The first firmware-only review found a separate P1 release-safety issue. Both
profiles shared output names, the producer's signature contract did not match
the real importer, Aura Link keyed current releases without a hardware target,
and the installer could not bind a selected board to an exact target/version.
Publishing either profile through that flow was stopped.

The coordinated local implementation has now resolved those structural
blockers in three isolated worktrees:

| Scope | Reviewed commit | Result |
| --- | --- | --- |
| Project Aura producer | `fac6e307b3d8df35db891b56f7022dbf89000d79` | Target-specific artifacts, signature-v2, source-bound identities, profile-safe diagnostics, protected GitHub publication |
| Aura Link | `e826b46ff4952be2da616133af89b584a625360d` | Target-aware releases/pointers/transactions, strict importer, fail-closed server-owned identity storage/policy, locked 7-inch OTA |
| Aura AQ site | `85cb5f805e4e1a1252c05f333908790ecb2ee7c3` | Versioned attempt identities, physical-target acknowledgement, exact target/version manifests, locked 7-inch installer lane |

Aura Link's final follow-up after the initial `08a3b83` control-plane commit is
split into four reviewable commits:

```text
ea56aa8ab0b49d272a38a90a7907a34864563743 Harden legacy firmware import identity
a5ed293363bb08f8495a226aa715bc87b28e488b Make firmware package import commit-safe
c1a7ed0f27742465ee63f53b97656bb5fb5907a9 Serialize installer quota attempts
e826b46ff4952be2da616133af89b584a625360d Require server-owned OTA hardware identity
```

The canonical target model is:

| Firmware environment | Hardware target | Hardware profile |
| --- | --- | --- |
| `project_aura` | `aura-aq-v1` | `4_3` |
| `project_aura_7` | `aura-aq-7-v1` | `7_dual_i2c_scl6` |

These are strict pairs. The producer and importer reject a mismatched pair. The
installer and catalog also reject ambiguous duplicate target/version entries
and asset reuse across identities. Existing target-less requests remain a
4.3-inch-only compatibility path.

The integration verification established:

- both real producer ZIP layouts are accepted by the `e826b46` Aura Link
  verifier;
- signature, target, profile, environment, build ID, asset kind, offset, size,
  and SHA-256 are bound together;
- a cross-target expected identity is rejected;
- release/current uniqueness and publish/rollback operations include target;
- the 7-inch external GitHub publisher fails locally before credential lookup;
- recovery packaging remains fail-closed;
- the installer requires an explicit physical-target acknowledgement for all
  modes and freezes target/version/channel before erase or write.

The final independent Aura Link pass found no P0-P2 issue in
`08a3b83..e826b46`; all 194 Vitest files and 1425 tests passed, TypeScript
checking passed, and the range diff was clean. The final site pass found no
active-path issue, with 32 focused tests and TypeScript checking passing. Its
broader existing suite remains 292/293 because of the previously known,
unrelated favicon expectation.

Remaining stop conditions are deliberate:

- the generated integration ZIPs use a temporary test key and are not
  production release assets;
- Aura Link migrations `0042`, `0043`, and `0044` have not been applied, and
  none of the three local branches has been pushed or deployed;
- migration `0044` defines server-owned device target/profile/source fields,
  but the production inventory and reviewed backfill have not been executed;
- a trusted one-time 7-inch identity enrollment/grant/claim path has not been
  implemented, reviewed, or exercised. Aura Link `e826b46` plus migration
  `0044` provide fail-closed storage and policy only; they are not an unlock;
- the control-plane rollout must be a coordinated non-rolling maintenance
  operation: stop Installer/OTA/firmware mutations, drain or reject legacy
  unversioned attempts, apply migrations `0042`, `0043`, and `0044` in order,
  complete the reviewed inventory/backfill and strict database checks, then
  switch Aura Link and the installer site together. An application-only
  rollback to the legacy attempt formula or missing-identity inference is not
  supported;
- the 7-inch target remains locked in GitHub, Aura Link promotion, installer,
  public OTA, and recovery until physical qualification is completed and a
  dedicated reviewed unlock commit links the evidence;
- current device claim/telemetry does not yet attest hardware target/profile,
  so Aura Link must not automatically offer 7-inch OTA;
- a browser cannot electrically detect the display size. The protected
  installer reduces manual cross-flash risk through explicit target binding,
  but raw/manual OTA still requires an operator to verify the physical board.

## Final decision

| Question | Decision |
| --- | --- |
| Carry the dirty 28-commit chain wholesale? | No |
| Keep all of `origin/main` as an unexamined recovery solution? | No; active recovery paths were explicitly gated off |
| Separate 4.3-inch and 7-inch firmware builds? | Yes |
| Move the 7-inch external sensor chain to I2C1 SDA44/SCL6? | Yes |
| Change the panel bus to 400 kHz during cleanup? | No |
| Return PCLK to 16 MHz? | Already done and vendor-equivalent |
| Publish the 7-inch image now? | No; exact `fac6e30` physical qualification, a reviewed trusted identity-enrollment path, and the coordinated non-rolling cutover remain required |
| Proceed to exact clean-artifact physical validation? | Yes, as the next controlled stage |
