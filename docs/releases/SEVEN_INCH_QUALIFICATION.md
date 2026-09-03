# Aura AQ 7-inch firmware qualification

Status: **NOT QUALIFIED FOR EXTERNAL PUBLICATION**

The 7-inch source profile and local package path may be built and inspected,
but the GitHub, Aura Admin, installer, and OTA publication gates must remain
closed until this record is completed and reviewed.

## Fixed baseline

- Environment: `project_aura_7`
- Hardware target: `aura-aq-7-v1`
- Hardware profile: `7_dual_i2c` for new `1.2.0-beta` builds
- Panel bus: I2C0, SDA GPIO8, SCL GPIO9, 100 kHz
- Sensor bus: I2C1, SDA GPIO44, SCL GPIO6, 100 kHz
- GPIO43/H3 EX_TXD: physically disconnected from sensor SCL
- ESP32 internal sensor-bus pull-ups: disabled
- USB/logging: native USB CDC on Type_C2

The former name `7_dual_i2c_scl6` remains part of historical artifacts and
signed metadata. The rename does not change routing or transfer qualification
to a new BIN. See [local 1.2 preparation](LOCAL_1_2_PREPARATION_20260903.md)
for current evidence boundaries and coordinated consumer compatibility.

The [shared native-USB profile policy](../USB_CONSOLE_PROFILES.md) preserves
this 7-inch baseline and model identity. Its staged migration requires the
4.3-inch basic check to pass before changing the 7-inch device or capture.

## Historical cleanup candidate

The following is the historical `fac6e30` software-evidence snapshot, not the
latest candidate to flash. Its recorded checks do not qualify the later panel
sampling, optional-alert, [built-in OTA guard](../OTA_HARDWARE_TARGET_GUARD.md),
or [shared native-USB](../USB_CONSOLE_PROFILES.md) changes. Select and record a
fresh exact-source artifact before the next physical qualification. At the
time of this snapshot neither binary below had been physically flashed or
qualified.

7-inch candidate:

- Source commit: `fac6e307b3d8df35db891b56f7022dbf89000d79`
- Version/build ID: `1.1.6-beta` / `fac6e30-7-dual-i2c-scl6`
- Firmware path:
  `D:\21cncstudio\project_aura\logs\firmware_candidate_fac6e30_DUAL_PROFILE_BUILD_EVIDENCE_20260830\artifacts\project_aura_7\firmware.bin`
- Firmware bytes: `4292128`
- Firmware SHA-256:
  `5B85F49BE8FE30FE213AAA216789CC91C751DDB0043050761A142E464340E4FB`

Exact 4.3-inch regression candidate:

- Version/build ID: `1.1.6-beta` / `fac6e30`
- Firmware path:
  `D:\21cncstudio\project_aura\logs\firmware_candidate_fac6e30_DUAL_PROFILE_BUILD_EVIDENCE_20260830\artifacts\project_aura\firmware.bin`
- Firmware bytes: `4297744`
- Firmware SHA-256:
  `45BB0ADE4E673EEA4E13B3F09D56987A3135B5EBEE00BD67467F43BF065707CF`

The exact-source native matrix passed 816/816 cases. Both integration ZIPs
were accepted by the `e826b46` Aura Link verifier, and a cross-target attempt
was rejected. Those ZIPs use a temporary test key and are not production
release assets. The 7-inch test-key ZIP SHA-256 is
`F0AAEF9C334C1C8831F9600C19B9AEED88FC6958B4770202AAEAFE524939F7D8`;
a production-key package remains pending release approval.

## Evidence required before unlocking publication

Record all identifiers from the exact clean candidate:

- source commit;
- firmware version and build ID;
- `firmware.bin` SHA-256;
- signed package SHA-256;
- board identity and final connector routing;
- logger implementation and capture folder.

The controlled test series must include:

1. Repeated physical OFF/ON cold boots at the 100 kHz panel baseline.
2. First frame and touch confirmation on every boot.
3. Sensor-host readiness and the expected RTC, pressure, SEN66, SEN0466,
   optional gas-sensor, and GP8403 behavior for the fitted configuration.
4. No unexpected software restart, EN/RTS reset, USB reconnect loop, or
   headless shutdown.
5. A normally closed long-run capture with the chosen full sensor set.
6. At least one exact-artifact 4.3-inch regression check.
7. On guarded firmware, built-in web OTA rejects the opposite model and an
   unlabelled legacy BIN with zero written bytes, no OTA restart, a readable
   inline error, and a working subsequent matching-file update on both models.
8. Native USB reflash/reset and port re-enumeration checks, plus a separate
   physical cold boot with serial monitors closed and auto-connect disabled.

Physical OFF/ON, upload reset, EN/RTS reset, software restart, and serial-open
effects must be logged as separate event classes. A USB gap alone is not proof
of a physical power cycle.

## Optional 400 kHz experiment

Only after the 100 kHz baseline is accepted may a separate 7-inch-only branch
test the panel bus at 400 kHz. That experiment needs its own build ID, capture,
and comparison record. It must not silently change the qualified 100 kHz
baseline or the 4.3-inch profile.

## Unlock rule

Publication may be enabled only after all of the following are complete:

- the exact artifact passes this physical qualification;
- a trusted one-time 7-inch identity enrollment/attestation path is
  implemented, reviewed, and exercised. Aura Link `e826b46` plus migration
  `0044` are fail-closed storage/policy only and are not an unlock;
- Aura Link and the installer site complete the documented coordinated
  non-rolling cutover with migrations, reviewed inventory/backfill, and strict
  database checks;
- a dedicated reviewed unlock commit links the completed evidence and preserves
  target/profile isolation.

Recovery packages remain locked unless they contain an independently built and
qualified 7-inch artifact.
