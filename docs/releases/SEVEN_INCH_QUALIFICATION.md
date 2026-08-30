# Aura AQ 7-inch firmware qualification

Status: **NOT QUALIFIED FOR EXTERNAL PUBLICATION**

The 7-inch source profile and local package path may be built and inspected,
but the GitHub, Aura Admin, installer, and OTA publication gates must remain
closed until this record is completed and reviewed.

## Fixed baseline

- Environment: `project_aura_7`
- Hardware target: `aura-aq-7-v1`
- Hardware profile: `7_dual_i2c_scl6`
- Panel bus: I2C0, SDA GPIO8, SCL GPIO9, 100 kHz
- Sensor bus: I2C1, SDA GPIO44, SCL GPIO6, 100 kHz
- GPIO43/H3 EX_TXD: physically disconnected from sensor SCL
- ESP32 internal sensor-bus pull-ups: disabled
- USB/logging: native USB CDC on Type_C2

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

Physical OFF/ON, upload reset, EN/RTS reset, software restart, and serial-open
effects must be logged as separate event classes. A USB gap alone is not proof
of a physical power cycle.

## Optional 400 kHz experiment

Only after the 100 kHz baseline is accepted may a separate 7-inch-only branch
test the panel bus at 400 kHz. That experiment needs its own build ID, capture,
and comparison record. It must not silently change the qualified 100 kHz
baseline or the 4.3-inch profile.

## Unlock rule

Publication may be enabled only in a dedicated reviewed commit that links the
completed evidence, preserves target/profile isolation, and keeps recovery
packages locked unless they have an independently built and qualified 7-inch
artifact.
