# SEN0321 local integration, 2026-09-26

Status: local implementation and validation complete; physical sensor and Home
Assistant installation unverified. No flash, reset, signing, commit, push, release,
Discord message or production change was performed. Branch main, base commit
`a2891b7443e8f8778a35162021924c6639e6227e`. Build IDs are `a2891b7-dirty`
(4.3-inch) and `a2891b7-7-dual-i2c-dirty` (7-inch).

## Behavior

- One optional gas slot, independently of the dedicated CO sensor. Existing
  DFR modules at 0x75 retain their protocol and UI. SEN0321 uses 0x73 on the
  configured sensor bus. There is no current configured address conflict.
- If both ACK during startup, 0x75 wins and a warning is logged. The unselected
  SEN0321 receives only an address probe, no mode command. Selection stays fixed
  until reboot, including during read failures. Dual installation is unsupported.
- An absent SEN0321 is probed at startup and twice more, 10 seconds apart.
  Default address 0x73 is supported; this integration does not scan 0x70-0x72.
- The dedicated driver uses passive mode: write 03/01, request 04/01, select 07,
  then read two big-endian ppb bytes. Waiting occurs between polls, without
  holding the shared bus across the manufacturer's 100 ms delays. A poll issues
  at most one bounded I2C transaction; acquisitions are about 1.2 seconds apart
  with prompt polling. There is no bus reset or sensor averaging in the adapter.
- Values become valid only on a successful read after 180 seconds of warmup.
  Failed/short transfers and values above 10000 ppb invalidate immediately;
  they never create a valid zero. A last reading older than 18 seconds is invalid.
  Any failed transfer or invalid frame requires reconfiguration and a new full
  warmup, since even one failed transaction can indicate a power interruption.
  The 180-second clock starts on successful passive-mode configuration; time
  spent disconnected or awaiting reconfiguration does not count. Power cycles
  with no observable communication failure cannot be detected by this protocol.
- Zero, 20 ppb and other valid returned values are preserved without correction.
  No floor subtraction or calibration is inferred from community observations.

## Units and compatibility

The user chose **integer ppb**, including 52, 53 and 55, without rounding to tens.
The manufacturer specifies **10 ppb resolution**, not a numerical accuracy bound;
integer formatting is not a claim of 1 ppb resolution or accuracy. The actual
granularity of individual I2C frames remains a hardware-test question. Unlike the
reference library, Aura does not introduce moving-average values at acquisition.

SensorData and the on-disk history retain canonical ppm. ID 7 (`O3_SEN0321`)
distinguishes source/units from legacy O3 ID 5. Existing IDs and record layout
are unchanged; history filters different source IDs instead of drawing one curve.
Canonical precision is three decimal places in ppm. Source data is not rounded
to a 10 ppb step. Versioned raw `aura.history.v3` export remains canonical ppm.

Device tile, detail, graph statistics, thresholds, web dashboard, chart API,
daily CSV and MQTT use ppb for SEN0321. Existing O3 reference boundaries remain
physically unchanged: 50 / 100 / 500 ppb. No new health threshold is introduced.
Live API/MQTT `optional_gas` and MQTT `o3` are in ppb with
`optional_gas_unit=ppb` and `optional_gas_decimals=0`;
`optional_gas_ppm_decimals=3` records the canonical precision.

Home Assistant keeps the existing entity IDs. The dedicated **O3** entity uses
ppb for SEN0321; discovery refreshes when the source becomes known after
connection. The legacy **Optional Gas** entity stays unitless and retains its
original numeric scale through the additive MQTT `optional_gas_canonical` field:
ppm for gases, %Vol for O2. For a 52 ppb SEN0321 reading, `optional_gas` and `o3`
are 52, while `optional_gas_canonical` is 0.052. Invalid samples are null in all
three fields. This compatibility entity is deliberately not the ppb presentation;
use O3 for that in Home Assistant.

Assigning ppm or ppb to the old unitless entity would change the meaning of its
existing recorder statistics. The contract test covers all optional source types,
including absent/invalid readings. Actual HA discovery and recorder behavior
still require verification on the tester's installation.

## Local verification

- 263 native Unity cases passed across 19 suites. After review fixes, 198 cases
  in 10 affected suites were rerun; the 65 unchanged I2C/routing cases retain the
  earlier successful evidence. The canonical launcher verified
  each selected suite's actual source identity. Coverage includes SEN0321
  timing, big-endian decoding, warmup, zero/full range, failures and short
  transfers, power-loss recovery, stale data, timer wrap, absence retries,
  dual-source priority/lock, legacy drivers, sensor routing for both profiles,
  UI/API/MQTT units, CSV, history statistics and source changes.
- Four Node tests execute the actual embedded dashboard functions, including
  rendered chart HTML: integer ppb including 52/53/55, correctly scaled
  thresholds, min/max rounding of fractional history means, invalid values,
  and unchanged ppm/O2/HCHO/pressure formatting. The complete embedded
  JavaScript also parses successfully.
- Recovery regressions cover one/two/three failures, long host pause during
  an outage, delayed initial configuration, the 179999 ms boundary, and millis
  rollover. They use the real driver with mocked I2C, not a simulated driver.
- Both ESP-IDF 6.1 profile builds passed the normal image identity, retained RTC
  ABI, native I2C linkage and restart checks. Build artifacts are local candidates,
  not signed installer releases or evidence of physical behavior.
- EEZ postprocess check passed. All five pre-existing dirty files match their
  recorded hashes, including screens.c and the EEZ project.
- Manufacturer downloads and pinned reference code match SHA256SUMS.txt.

Run host checks with `scripts/run_tests.ps1`; SEN0321 belongs to
`native_test_dfr_optional_gas_driver`. Run dashboard checks with
`node --test scripts/tests/dashboard-optional-gas.test.mjs`. Build using
`D:/21cncstudio/project_aura_aq/tools/build.ps1 -Line main -Profile <profile> -Jobs 4`.

| Profile | Application bytes | SHA-256 |
| --- | --- | --- |
| 4_3 | 4599488 | `b4b7651ee450f05efb8983d1ff0259845943f2d1436850de19fcc70926fe975f` |
| 7_dual_i2c | 4599808 | `6d66f992e024b78abdc7e344868df008a924cc6beddaefa6042c0f196a149e44` |

Applications are at `build-idf-<profile>/aura_aq.bin`; their directories may be
overwritten by later local builds. Exact identities, artifact stamps and test
report locations are retained in
[validation-20260926.json](datasheets/SEN0321/validation-20260926.json).

## Prepared OTA test files

The exact reviewed application BINs are copied to
`release-assets/SEN0321-test-20260926/`, independently of mutable build folders.
The bundle includes README.md for the tester, SHA256SUMS.txt, manifest.json, and
private evidence (build logs, test reports and the uncommitted source delta).
Each copied image matches its build stamp and passes complete ESP checksum/hash
and target/flavor checks; the opposite hardware target is rejected by the parser.
These are application-only files for the built-in device OTA uploader. No OTA
upload or public beta publication was performed. The existing 1.2.1-beta version
label is unchanged; filenames and SHA-256 distinguish this test candidate.

## First test with fwa_be

Confirm the display profile and installed firmware before selecting any upload
artifact. Use the sensor bus for that profile and 0x73 (A0=1, A1=1), one optional
gas module installed. The SEN0321 board's connector-side I2C pull-ups go to VCC;
use 3.3 V for direct connection to Aura's 3.3 V bus. See the retained schematic.

After an explicitly authorized installation, collect startup diagnostics proving
SEN0321 detection, warmup and first valid sample. Observe raw reading granularity
and equal values across device, web and MQTT; confirm O3 uses ppb in HA and
the legacy Optional Gas entity retains its original unitless scale/history.
Check graph/statistics units and invalid-data behavior on a separately authorized
disconnect/reconnect test. The initial 24-48 hour conditioning guidance remains
distinct from the 180-second startup warmup. No measurement-accuracy claim should
be derived from these software tests.

References: [official sources, downloaded PDFs and protocol audit](datasheets/SEN0321/README.md).
