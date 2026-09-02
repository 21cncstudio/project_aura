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

## Next-beta backlog addition, 2026-08-31

The user requested a GT911 address experiment at `0x5D` only for the 7-inch
dual-I2C profile `7_dual_i2c_scl6` / `project_aura_7` / `aura-aq-7-v1`.
Keep 4.3-inch `project_aura` / `aura-aq-v1` at `0x14`.

Start with a diagnostic candidate. Confirm the actual address, cold boots,
software-restart return, and sustained screen/touch operation without new
failures. Keep bus speeds, power and wiring unchanged during this experiment;
check the 4.3-inch build for regressions. Keep `0x5D` in the production 7-inch
profile only after the agreed test series passes. Until then, this is pending
acceptance, not a proven fix.

Uncommitted GT911 work, including `project_aura_7_gt911_5d`, was already present
when this item was recorded. Inspect and coordinate that work before editing;
its existence does not prove build, flash or physical-test success. This backlog
update changes documentation only and grants no immediate hardware authority.
The detailed handoff's open-tasks section also retains the deferred OTA
rejection-message fix.

## Deferred discussion: idle touch polling, 2026-08-31

The user proposed reducing touch polling while Aura is idle, until a touch
occurs, because the device is not interacted with continuously. Discuss the
existing IRQ/wake behavior, response latency and failure detection before
choosing an implementation. This is a backlog idea only: do not change polling
or wake behavior as part of the OTA rejection fix or the GT911 address trial.

## Deferred discussion: backlight before the logo, 2026-08-31

During cold-start cycle C02, the user requested keeping the backlight off until
the logo frame is ready, if the initialization sequence permits it. The user
reported normal screen/touch operation; this request is not a new failure claim.
Review the existing startup/backlight sequence and physical behavior separately,
including the interval after power is applied but before software controls the
backlight. Do not promise that software alone can suppress that initial interval.
Keep this change separate from idle touch polling and the GT911 address trial.
The current five cold starts must use the unchanged diagnostic BIN. This entry
records a future task only; it does not authorize a new flash or hardware test.

## Local OTA candidate evidence, 2026-08-31

The rejection-message fix was built and exercised on both physical profiles
under separately approved OTA blocks. Exact source/build evidence:
`D:\21cncstudio\project_aura\logs\ota_rejection_preservation_20260831T121241Z\RESULT.md`.
Hardware records, including recorder caveats and limits:
`D:\21cncstudio\project_aura\logs\ota_hardware_feedback_20260831T124146Z\RESULT_4_3_BLOCK.md`
and `RESULT_7_BLOCK.md` in the same directory.

Opposite-model and genuine legacy BIN refusals retained their original reason
across two short consumed-confirmation replays, with zero-write API counters,
no observed restart and no extension of the original result TTL. Subsequent
matching uploads completed with automatic restarts and new-partition stable_boot.
User feedback confirmed screen/touch after the matching installations. Do not
infer separate physical LCD restoration PASS for every negative case; see the
per-case records. Short prefix replays are not full-file retransmission tests.

These results apply only to the saved dirty candidates at HEAD 5383b77, identified
by full SHA256 in the records. Both normal profiles still use GT911 0x14.
The 7-inch 0x5D diagnostic BIN has build/software evidence only. Its installation,
cold-start/restart/soak series and any production promotion remain separate.
No publication, remote changes or package replacement was authorized here.

## First diagnostic 0x5D installation, 2026-08-31

Under a new explicit approval, the exact diagnostic 7-inch BIN
`ff8c6045ffdb6a967cc3831d299830fd98acd8e774bf5b75d348d7b9095a93f3`
was installed once through OTA on `192.168.1.165` / `aura-f16e20`.
Its automatic software restart passed: valid GT911 ID at 0x5D, config read,
one Board/LVGL initialization and app0 stable_boot. The operator reported
normal screen/touch. The four deliberate GT911DIAG WARN lines, including the
opposite-address 0x14 ESP_FAIL, were reviewed separately from runtime faults.

Exact evidence and limits:
`D:\21cncstudio\project_aura\logs\ota_hardware_feedback_20260831T124146Z\I05_GT911_5D_FIRST_7\RESULT.md`.
The installed 7-inch device now runs the diagnostic 0x5D candidate. Both normal
source environments still select 0x14, and the 4.3-inch board was not changed.
At that first-boot checkpoint, a repeated cold-start/software-restart/soak series
had not yet been agreed or run.
This first diagnostic result does not qualify production 0x5D or a different BIN.

## Subsequent cold-start-only series, 2026-08-31

The user subsequently authorized exactly five operator-controlled cold starts
of the same installed 7-inch diagnostic BIN, using the same sole power cable.
The scope and per-cycle raw evidence are retained under
`D:\21cncstudio\project_aura\logs\ota_hardware_feedback_20260831T124146Z\GT911_5D_COLD5_20260831`.
Read `AUTHORIZED_SCOPE.md` and each completed cycle's `RESULT.md` and
`EVIDENCE_AUDIT.json`; an armed observer or API-only result is not physical PASS.

C01 used the original minimum 30-second OFF procedure. After C01 was powered
on, the user changed C02-C05 to at least 120 seconds from their OFF confirmation
before permission to reconnect. Do not describe all five as a uniform two-minute
OFF series or infer electrically measured rail-discharge intervals.

No software-restart series, long soak, new flash, source/build change or
production promotion is authorized by this cold-start-only block. Keep the
backlight/logo and idle-touch ideas deferred until this unchanged-BIN series
is complete, and do not transfer its results to a different artifact.

The user ended cold-start testing after C04. C01-C03 passed their scoped
operator and approximately three-minute API observations. C04 has normal
operator screen/touch feedback and a separate healthy snapshot at uptime 114s,
but its original observer had timed out before seeing the new boot. Retain
that capture gap; C04 is not a full observation PASS or a proven firmware fault.
C05 was not performed. Do not resume, repeat C04 or add a fifth start without
a new user request. The series is closed by the user, not a five-start PASS.
See `RESULT.md` and `SERIES_RESULT.json` in the cold-start evidence directory.

## Approved software-restart block, 2026-08-31

After closing cold starts, the user separately approved ten sequential software
restarts through the 7-inch device's ordinary Restart menu, each with about
180 seconds of API observation and operator screen/touch feedback. They then
explicitly asked to start when ready without repeated permission prompts.
Approval, scope, recorder validation and per-cycle evidence are kept under
`D:\21cncstudio\project_aura\logs\ota_hardware_feedback_20260831T124146Z\GT911_5D_SW10_20260831`.
This approval does not reopen cold starts or authorize the proposed 12-hour soak.

Keep the same diagnostic BIN, wiring, power, frequencies and 4.3-inch device
unchanged. Use a separate GET-only observer: operator waiting must not consume
the boot deadline, and an early new boot must be captured even before the click
report arrives. Record menu action, restart epoch, observation and physical
feedback separately. No automatic resend or recovery on uncertain results.
An approval or armed recorder is not evidence that a restart has occurred.

### Updated control and feedback for the same ten restarts

Before any restart was performed, the user explicitly asked the assistant to
perform the restarts remotely and continue without per-cycle acknowledgments.
The operator has the screen in view, may touch it occasionally, and will report
incidents. Silence must not be recorded as physical screen/touch PASS.

The manual observer was stopped without a restart; its immutable evidence remains
under `GT911_5D_SW10_20260831`. The new approval, scope and cycle records are under
`D:\21cncstudio\project_aura\logs\ota_hardware_feedback_20260831T124146Z\GT911_5D_SW10_AUTO_20260831`.
The ordinary dashboard reboot route is `POST /api/settings` with the sole JSON
field `restart: true`. A separate actuator records one attempt before one POST;
the GET-only observer collects the resulting boot. An uncertain response is
never retried blindly. Review each 180-second API observation before proceeding.
Stop subsequent restarts on a reported incident or unexpected fault.

This updated control method does not authorize any new firmware, serial access,
power cycle, settings change, long soak, publication or production promotion.

### Completed HTTP software-restart block, 2026-08-31

The assistant sent ten separately recorded, accepted, one-shot Restart requests
to the unchanged 7-inch diagnostic device. No request was retried. All ten
subsequent captured boots reported SW, Board/LVGL ready and the expected four
GT911 diagnostic records at 0x5D (observed config 0x58; opposite 0x14 ESP_FAIL).
This is not a blanket ten-cycle physical or uninterrupted-observation PASS.

- S01: the original recorder falsely stopped on the ordinary old-boot nested
  `ota.status=busy` projection. Preserve that incomplete capture. Independent
  state/diag/events at uptime 21s showed normal initialization, followed later
  by a separate 180.125-second observation at device time 160474-340319ms.
- S02-S05 and S07-S10: eight complete API observations passed, approximately
  181-182 seconds each, with no post-boot API gap or additional observed reset.
- S06: complete 181.844-second observation; its raw `REVIEW_REQUIRED` result
  remains unchanged. Separate source-based review classified five numeric PM
  `critical` messages as sensor-value threshold warnings and accepted only the
  GT911/API scope. The cause of PM values 999 ug/m3 / 3000 #/cm3 remains unknown.

Recorder corrections were saved as separate AUTO/V2/V3 tools and evidence roots;
they did not change the firmware. The BIN remains
`ff8c6045ffdb6a967cc3831d299830fd98acd8e774bf5b75d348d7b9095a93f3`.
No systematic per-cycle physical screen/touch verification was performed, and
silence was not converted into physical PASS. Finite API rings and polling do
not prove zero low-level faults. The superseded manual series sent no restart.

The supplemental S01 capture also recorded CO warmup and one sample with
`warmup=false` but `co=null`, then CO=0. The independent warmup clock and 3-second
sensor poll make that short transition compatible with normal startup timing;
the earlier user observation of `--` at an unknown time remains unexplained.

Read the aggregate report and per-cycle records before extending any claim:
`D:\21cncstudio\project_aura\logs\ota_hardware_feedback_20260831T124146Z\SOFTWARE_RESTART_10_20260831_RESULT.md`.
Normal 4.3-inch and 7-inch source environments still select 0x14. No new firmware
build/upload, serial/COM8 access, power/wiring/frequency change, publication,
package replacement or production promotion occurred in this restart block.
The cold series stays closed with its existing limits. A long screen/touch soak
and any production 0x5D decision remain separate, uncompleted steps.

## Deferred soak and external diagnostic feedback, 2026-08-31

The user deferred the long screen/touch run until nighttime. No scheduled start,
automatic overnight monitor, new flash or production promotion was requested by
that deferral. Keep the currently installed diagnostic candidate distinguishable
from any subsequent local implementation/build.

The user then supplied another tester's written report for the stated version
`1.1.6-beta-019d87b-7-dual-i2c-scl6`: uptime 48262s (13h 24m 22s), no reported
restart or touch loss, screen on, SEN66 + CO + DAC connected, Wi-Fi and MQTT
enabled, pull-up switches OFF. The tester described one transient SEN0466 CO
write event around six hours and one NTP timeout without observed disruption.
These are external operator-reported observations, not an independently audited
soak PASS or evidence for the current 5383b77/0x5D candidate. The quoted message
said diagnostics were attached, but the actual attachment was not supplied here;
the two events and their raw line states have not been independently checked.

Source review of exact 019d87b and current main confirmed that `boot.i2c_status`,
`sda_high` and `scl_high` expose a saved pre-initialization sample of the panel
bus GPIO8/9. They do not sample the sensor bus GPIO44/6 or current runtime health.
GPIO8/9 remain the active panel bus, not an obsolete GPIO43 route. The name
`sda_stuck_low` comes from classifying a low SDA snapshot; persistence of that
JSON value does not prove the line remains low. The initial low sample's cause
is unknown.

The /diag download control is absent in the 019d87b/current-main template.
Historical commit `6ae2ed5` added downloadable support reports together with
GT911 runtime recovery in the archived diagnostic chain. Do not merge that
commit or the archived chain merely to restore the button.

Proposed next local work: restore diagnostic export as an isolated feature and
label the I2C values with their startup phase, bus and pins, preserving raw data
and API compatibility. Then address backlight-before-logo separately. Idle touch
polling remains a separate optimization after the unchanged-BIN qualification;
screen-off touch already has IRQ handling and a sparse fallback poll. This entry
records findings and prioritization, not a new hardware or publication approval.

## Local diagnostic export implementation, 2026-08-31

The user approved the proposed local `/diag` work. Added a JSON report download
using fresh, sequential GETs to the existing diag/events APIs, with bounded
requests and explicit partial results. Added firmware/profile identity and
startup panel-bus metadata while preserving the old raw I2C fields. The page
distinguishes saved pin levels from compile-time panel/sensor routing. No new
I2C sampling, recovery, GT911 address, timing or hardware behavior was added.
Format and interpretation: `docs/DIAGNOSTIC_REPORT.md`.

Original dirty files and the committed base were preserved before this work in
`D:\21cncstudio\project_aura\logs\diag_report_20260831T172858Z`.
The same directory contains the separate new BINs, hashes, build logs and test
evidence. Native diagnostics tests passed 7/7; actual-template JavaScript tests
passed 11/11. Both normal firmware environments compiled successfully and their
OTA hardware identity checks passed. These remain GT911 0x14 builds, not a new
0x5D diagnostic candidate or hardware-qualified release.

Browser checks used synthetic localhost data only, covering both routing
profiles, escaped text and partial-report feedback. The page initiated download,
but the in-app browser download event timed out and file saving was not verified.
Do not record an end-to-end browser download PASS. Hardware HTTP behavior and
physical screen/touch operation of the new BINs have not been tested.

The installed diagnostic BIN remains separately identified by SHA256
`ff8c6045ffdb6a967cc3831d299830fd98acd8e774bf5b75d348d7b9095a93f3`.
Do not use the now-modified main source as its exact source snapshot; use the
preserved pre-change source/evidence. The deferred nighttime run was not started
or scheduled. No board HTTP requests, reset, serial/COM8, flash, push, publication,
package replacement or production 0x5D promotion occurred in this local task.

## Local startup backlight implementation, 2026-08-31

The user approved the next local change: backlight OFF until the boot logo is
ready. Both profiles now use the vendor custom backlight callback to avoid the
switch-expander driver's early ON write. The 4.3-inch CH422G initial image changes
only EXIO2 (`0xDF` to `0xDB`); the 7-inch image stays `0xD1`. Idle-OFF is enabled.
UI startup waits for the complete logo fade, invalidates a full frame, then waits
for its framebuffer switch/VSYNC acknowledgement. Only then may the existing
guarded wake path enable the backlight, recorded as `startup_wake`. The visible
logo dwell and schedule grace are based on the confirmed software ON state.

The implementation and hardware acceptance checklist are in
`docs/STARTUP_BACKLIGHT.md`. No acknowledgement means no startup ON and failure
is returned to the existing UI recovery/headless policy. Software cannot
guarantee darkness before the first successful CH422G write; neither a log nor
a frame acknowledgement proves the panel's optical output.

Original dirty files were saved before editing. Evidence, source snapshots,
native reports and separate new BINs are under
`D:\21cncstudio\project_aura\logs\startup_backlight_20260831T174744Z`.
All 141 selected native cases passed; both normal firmware builds and their OTA
identity, RTC-layout and restart-linker checks passed. EEZ postprocess check and
diff whitespace check passed. Full startup behavior remains untested on hardware.

These normal candidates still use GT911 `0x14` and include the preceding local
diagnostic-export work. The installed 7-inch `0x5D` BIN remains separately
identified by SHA256
`ff8c6045ffdb6a967cc3831d299830fd98acd8e774bf5b75d348d7b9095a93f3`;
its preserved file hash was rechecked, without contacting the board. No flash,
reset, serial/COM8 access, hardware HTTP requests, power-supply/wiring/frequency
change, remote operation, package replacement or production promotion occurred.
The deferred soak was neither started nor scheduled.

## Combined local candidate preparation, 2026-08-31

The user asked to finish the local work before flashing. The combined OTA,
diagnostic-export and startup-backlight source was reviewed without new blockers.
All firmware source bytes matched the preceding startup candidate; this step
added validation, documentation and a fresh isolated 7-inch diagnostic build.
Main and the tested release checkpoint were not moved; no commit was made.

The complete default native run passed 939 cases, plus seven in the separate
GT911 0x5D environment. All 88 Python and 11 JavaScript checks passed. The first
Python invocation could not import two project modules under isolated Python;
the corrected invocation added only the explicit project path. Both logs remain
preserved. Local browser downloads of complete and partial JSON reports were
verified on disk, closing the earlier local saving gap without using a board.

Evidence and the exact two OTA candidates for later selection are under
`D:\21cncstudio\project_aura\logs\beta_local_completion_20260831T182528Z`.
See `RESULT.md`, `CANDIDATES.json`, and `ota-candidates/SHA256SUMS.txt` there.
The 4.3-inch candidate is the already-built, unchanged-source 0x14 BIN
`9f4d9002c637ae3cc556d3990bfa43c545630574eb04cf8721dcd0f6f27d4a24`.
The fresh 7-inch diagnostic 0x5D BIN is
`1024a0c04fc17db411a8e6b865207827641651930427bdb43e85c80f64bfc547`,
4309392 bytes. Its build passed OTA image identity/integrity, RTC layout and
restart-linker checks. These are unsigned internal BINs, not public packages.

The new 7-inch BIN repeats the old dirty version string. A matching version alone
cannot distinguish it from the installed `ff8c6045...93f3`; retain the new full
BIN hash, source snapshot and a future installation receipt. The old cached
`.pio/build/project_aura_7_gt911_5d/firmware.bin` is a different stale artifact
and was not selected or overwritten. Neither normal environment promotes 0x5D.

Idle touch polling remains unchanged pending the user's choice. Its read-only
analysis and proposed separate test plan are in
`docs/IDLE_TOUCH_POLLING_PROPOSAL.md`. No source for that optimization is included.
No board HTTP requests, serial/COM8, reset, flash, power-supply/wiring/frequency
change, push, publication, deployment, package replacement or soak occurred.

## Authorized combined-candidate OTA on both boards, 2026-08-31

The user subsequently requested installing both prepared candidates before
deciding on idle touch polling. Each matching OTA used fresh physical Allow,
one firmware POST, and the normal automatic software restart. There was no
serial/COM8 access, manual reset, cold cycle, retry, source rebuild, wiring,
power-supply or frequency change. Evidence and per-board warning reviews:
`D:\21cncstudio\project_aura\logs\combined_beta_ota_both_20260831T200544Z\RESULT.md`.

- 7-inch `192.168.1.165` / `aura-f16e20`: installed diagnostic 0x5D BIN SHA256
  `1024a0c04fc17db411a8e6b865207827641651930427bdb43e85c80f64bfc547`,
  4309392 bytes. HTTP 200 exact write, new boot, app1 stable_boot at 63127 ms;
  final follow-up uptime 505 s. Four intended GT911DIAG WARN rows and one LVGL
  mutex miss (`lock_fail=1`) were retained. The latter showed no touch error,
  VSYNC timeout or stall indication; its source is not established, and no
  additional UI warning was captured in the bounded follow-ups.
- 4.3-inch `192.168.1.14` / `aura-672c7c`: installed 0x14 BIN SHA256
  `9f4d9002c637ae3cc556d3990bfa43c545630574eb04cf8721dcd0f6f27d4a24`,
  4307888 bytes. HTTP 200 exact write, new boot, app0 stable_boot at 63293 ms;
  final follow-up uptime 163 s. RTC battery-low, CO address probes and missing
  DAC were reviewed separately. CO/DAC were already unavailable before OTA;
  RTC battery-low was recorded previously. VOC/NOX were still in gas warmup.

Both boards returned the new diagnostic identity and configured I2C topology,
the UI logo framebuffer acknowledgement, Board/LVGL ready after one attempt,
and actual/target backlight ON with no transition. Eleven API checks passed
per board, but strict machine results remain REVIEW_REQUIRED for the recorded
warnings, with separate written reviews. Both /diag HTML responses now contain
Download report; a physical-device browser export was not exercised here.

Physical logo/light/touch feedback is pending. No optical PASS, cold-start
qualification, long soak, or general sensor-regression PASS is inferred from
these APIs. These new BINs do not inherit the prior test series. Normal 7-inch
production remains 0x14; 0x5D is still isolated to its diagnostic environment.
Idle touch polling is unchanged. Both upload/observation scripts have exited;
no future hardware action or soak is scheduled. Main/release refs and prepared
packages were not moved or replaced; no push, publication or deployment occurred.

### User restarts and warning follow-up, 2026-08-31

The user subsequently reported GT911DIAG/UI heartbeat warnings and independently
restarted the boards. The assistant only captured GET snapshots. See
`D:\21cncstudio\project_aura\logs\combined_beta_ota_both_20260831T200544Z\USER_RESTART_WARNING_REVIEW.md`.
On the new 7-inch boot the four GT911DIAG startup rows repeated, and a new
heartbeat at 65441 ms again showed lock_fail=1, touch_err=0, vsync_timeout=0,
paused=NO with healthy handler/VSYNC ages. No additional UI warning was captured
by uptime 144 s. Successful GT probes are unconditionally logged as WARN in this
diagnostic environment. The cause/call site of the single timed LVGL mutex miss
is not identified by the current log; startup and runtime use the same counter.
Do not conceal actual failures by suppressing the whole heartbeat.

4.3-inch returned after one GET timeout; at uptime 69 s it had Board/LVGL ready,
settled backlight ON and the new logo-frame ACK, with no UI heartbeat WARN in
that snapshot. Its API reported POWERON/cold_power_start=true, whereas the
7-inch API reported SW. The physical 4.3-inch reset/power procedure and OFF
interval were not specified. RTC battery-low and one startup WiFi AUTH_FAIL
were preserved in the later 4.3-inch snapshot. These are separate user starts,
not repeats of the OTA action
or a controlled cold-start series. Physical light/logo/touch feedback remains
pending. No firmware changes, additional flash/reset/serial actions, or future
observation schedule were introduced in this follow-up.

## Local diagnostic warning policy candidate, 2026-08-31

The user approved the warning-classification change and clarified that 0x5D is
intended as the normal working address, not itself a warning condition. The
local implementation is documented in `docs/DIAGNOSTIC_WARNING_POLICY.md`.
Successful configured GT911 selection/ID/config checks are INFO; an alternate
ESP_FAIL is INFO only after those selected-address checks succeeded. Actual
selected-address/read/ID/reset errors and unexpected alternate results retain
WARN/ERROR. GT911DIAG INFO stays available through /api/events and existing
event mirroring, without creating user-facing alerts.

Only short boot-logo mutex retries now use an explicit startup accounting path.
Atomic counters preserve startup misses separately from normal lock_fail,
including ordinary callers during startup. A compact UI INFO logs both counts.
No counter baseline, runtime heartbeat/stall rule, mutex timing or logo timeout
was changed. The cause of earlier lock_fail=1 records remains unproven.

Evidence: `D:\21cncstudio\project_aura\logs\diagnostic_warning_policy_20260831T204001Z`.
All 89 selected native cases passed, and fresh builds passed for 4.3-inch,
normal 7-inch and diagnostic 7-inch, with no compiler warning/error lines.
OTA image integrity/target, RTC layout, restart-linker, EEZ and whitespace checks
passed. The first native invocation was blocked by the local PlatformIO lock
file permission; both that record and the later passing runs are preserved.

New unflashed candidates in ota-candidates:
- 4.3-inch 0x14: 4308240 bytes, SHA256
  `d514f276c93df11656c92d5e5c96b48eec47f28c6fcd18a2f4e1c4d8509dea92`.
- 7-inch diagnostic 0x5D: 4309872 bytes, SHA256
  `900842aef79289e71ac76ae9b6d623023c89939cf46a4a86e8728cb964ba15c1`.

Version strings still repeat; exact BIN hashes and a new installation receipt
are required. Old BINs/packages were not replaced. Source/default addresses,
bus operations/frequencies, power/wiring, idle polling and OTA behavior remain
unchanged. No hardware HTTP, flash/reset/serial, commit, push/publication/deploy
or scheduled test occurred. Normal production 7-inch is not promoted to 0x5D
by this logging-only task. Hardware/optical acceptance of these new BINs remains
pending and cannot inherit prior results. Do not rerun the old OTA scripts,
which pin earlier hashes and expected WARN records.

## Full 7-inch candidate audit and rare vertical shift, 2026-08-31

The user reported a rare single slight vertical screen movement on the main
screen without touch or page navigation, followed by continued normal operation.
Exact event time, screen_flip_180 setting and event BIN are unknown. Cause is
not established. The newest warning-policy candidate 900842ae remains unflashed.

Full local audit: `docs/FIRMWARE_7_AUDIT_20260831.md`. Evidence:
`D:\21cncstudio\project_aura\logs\audit_7_candidate_20260831T205752Z`.
All 52 prior dirty authored files and three BIN hashes matched their manifest.
Fresh tests passed: 959 native cases in 115 environment-suite entries, 88 Python,
11 JavaScript; EEZ check and configured git diff check passed. Previous fresh
three-profile build results apply to this identical source snapshot, not hardware.

Read-only review found pre-existing P2 defects: reuse of the active rotated
framebuffer when flip180 is enabled; read errors clearing the post-wake clean
release gate; and unsynchronized Logger rings. Expected GT911 INFO also needs a
separate startup snapshot, because the finite raw log ring can lose it before
the first HTTP snapshot. Do not restore normal 0x5D messages to WARN to retain
them. No firmware fixes were applied during this audit.

Pinned RGB binary contains silent bounce/DMA recovery; its use on the board is
unproven. The application's vsync counter uses on_bounce_frame_finish in this
configuration, not a physical VSYNC measurement. Startup ACK confirms driver
handoff, not optical full-frame completion. The compiled ui_runtime.c uses
lv_scr_load; generated ui.c/fade is excluded. Actual prebuilt SDK does not enable
the RGB/LCD_CAM IRAM-safe flags declared in sdkconfig.defaults; XIP is enabled.
These limits and a controlled follow-up checklist are documented in the audit.

GET at 20:59:40 UTC captured the previously installed 1024a0c0 build, uptime2035s,
Board/LVGL ready, backlight ON, sensor values present and the same five saved
startup/heartbeat warnings. This does not validate the unflashed 900842ae BIN
or exclude a short visual issue. No reset, serial/COM8, OTA, bus/timing/power/wiring
change, publication, ref change, package replacement or scheduled soak occurred.

## Historical integrated post-audit candidate (pre-flavor), 2026-09-01

This section records the superseded candidate built before the embedded OTA
flavor boundary. Its counts and hashes must not be selected as the current BINs;
the later `Post-review fixes candidate` section is authoritative for local work.

The user asked to fix all findings before the next flash. F1-F4 from
`docs/FIRMWARE_7_AUDIT_20260831.md` are now closed locally: rotated framebuffer
ownership, post-wake release gating, Logger rings, and bounded GT911 startup
capture. The LVGL first-flush/callback startup race and display-diagnostic
availability during UI startup failure were also corrected. Active runtime LVGL
deinit fails closed; reboot keeps its separate quiesce path.

The split profiles now intentionally use:

- `project_aura`: 4.3-inch, GT911 0x14, startup diagnostics OFF, flip default OFF.
- `project_aura_7`: 7-inch dual-I2C SCL6, GT911 0x5D, startup diagnostics OFF,
  flip default ON.
- `project_aura_7_gt911_5d`: the same 7-inch address/profile/flip with bounded
  startup diagnostics and `diagnostic_only=true` identity.

The rare single CO `--` was reviewed in
`docs/SEN0466_TRANSIENT_INVALID_AUDIT_20260901.md`. SEN0466 now commits only a
fully validated sample, retains the last good value through the first two
consecutive semantic/transport failures, retries only one runtime ESP_FAIL or
timeout after 150 ms, and retains the 18-second stale safety limit. The old
wrong-type and stale catch-up invalid windows were real code paths; the cause of
the observed hardware event remains unknown.

Final local validation passed: canonical native 977 cases plus 10 in the
separate GT911 0x5D run, Python 88, diagnostic-template JavaScript 11, EEZ check,
and three isolated firmware builds. The builds passed target/integrity, RTC ABI
and restart-linker checks and contained no compiler warning/error lines.

Exact unflashed BINs:

| Environment | Bytes | SHA256 |
| --- | ---: | --- |
| `project_aura` | 4316912 | `0d48aadcea89f02524fcbd8879bd6e613e5a0d46f0f2a16f451e7fcfb50d5bd0` |
| `project_aura_7` | 4317408 | `9470262a974a3ee85d2587cf35fda962956f5b5620fb74fb634f012c12c7ed52` |
| `project_aura_7_gt911_5d` | 4318608 | `45c1fbcdca82f6abe275122e362a892d5ae853e90ddd7b7672467303cc799c7e` |

Evidence and copied BIN/ELF/map/bootloader/partition/identity artifacts are under
`D:\21cncstudio\project_aura\logs\post_audit_release_candidate_20260831T234315Z`.
These are local unsigned artifacts, not release packages. No device HTTP, flash,
reset, serial/COM8, push, publication, deployment, package replacement or ref
change occurred. `dual-profile-release` remains at `019d87b`; old tested BINs
were not overwritten. Screen/GT911/CO fixes remain hardware-unqualified for these
exact new BINs.

## Post-review fixes candidate, 2026-09-01

The full review after `dual-profile-release` found and locally closed four
additional gaps:

- the first accepted CO sample after the 300-second boundary now completes
  warmup atomically; an old warmup sample or rejected frame cannot briefly
  publish `warmup=false, valid=false`. If every frame remains bad, the existing
  18-second stale window bounds the grace and exposes invalid state at 318 s;
- optional-gas changes equal to one declared resolution step are published with
  float tolerance, and `/api/charts` builds JSON from one heap-owned immutable
  snapshot containing type, count, epoch, entries and derived latest from the
  same generation;
- the canonical native launcher now includes the 7-inch GT911 0x5D and profile
  rotation tests instead of relying on a separate manual run;
- production and diagnostic 7-inch OTA lanes now have distinct embedded
  target/flavor identities and reject an incompatible BIN before `Update.begin`;
- one Logger Serial line is protected by a dedicated output mutex, separate
  from the recent/alert buffer mutex.

Fresh validation passed on the current dirty snapshot: canonical native 996/996
in 117 environment-suite entries, Python 94/94, diagnostic JavaScript 11/11,
EEZ postprocess and whitespace checks, and all three firmware builds. Each build
passed OTA identity/integrity, RTC ABI and restart-linker checks.

One earlier full native attempt did not execute `test_time_manager_ntp` because
MinGW `ld.exe` briefly received `Permission denied` while reopening the shared
native `program.exe`. The isolated suite then passed 9/9, the adjacent
TimeManager plus NTP sequence passed 51/51, and the authoritative clean canonical
rerun below passed 996/996. This was classified as a Windows build file-lock
transient, not a firmware or NTP test failure.

Exact local unsigned and unflashed BINs:

| Environment | Bytes | SHA256 | Identity |
| --- | ---: | --- | --- |
| `project_aura` | 4318640 | `2393ec0621ca9eb8b16dbc257a349fad5830a788de29c783b25d3680425b64d6` | `aura-aq-v1` + `production` |
| `project_aura_7` | 4319344 | `8d8e3744892bd2ab90fbd2d74502c7d4529a6f329714522f20f8b6d23dffb189` | `aura-aq-7-v1` + `production` |
| `project_aura_7_gt911_5d` | 4320352 | `3b664e20baf2158c584946d355fc460c6edffb612f4abadee4f739924bb9343d` | `aura-aq-7-diag-v1` + `diagnostic` |

Evidence: `D:\21cncstudio\project_aura\logs\post_review_fixes_candidate_20260901T090555Z`.
No hardware, COM/serial, flash, reset, OTA, signed package, commit, push,
publication, deployment or ref mutation was performed. The rare vertical shift
and the observed one-off CO `--` remain hardware observations with unknown cause;
neither is transferred as PASS to these exact BINs.

## GT911 diagnostic environment retirement, 2026-09-02

After the address experiment completed and `project_aura_7` was validated with
GT911 `0x5D`, the user retired the separate `project_aura_7_gt911_5d` firmware
environment. Current firmware builds expose only the two production environments:
`project_aura` at `0x14` and `project_aura_7` at `0x5D`.

The extra startup product-ID/config/opposite-address reads, `GT911DIAG` logging,
and `boot.gt911_startup` API snapshot were removed with that environment. The
normal RESET/INT address-selection path and its hard profile/address checks remain.
Production 7-inch regression coverage now runs inside the ordinary 7-inch native
profile rather than a diagnostic-named test environment.

The OTA validator still recognizes the old diagnostic target/flavor bytes only
as a fail-closed compatibility tombstone: production rejects those saved BINs,
and a historical diagnostic installation can return to production. No current
environment can generate a diagnostic BIN. Historical documents, commits, logs,
hashes and preserved artifacts remain unchanged as evidence.
