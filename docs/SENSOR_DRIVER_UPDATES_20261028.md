# Sensor driver update audit — 2026-10-28

Checked official GitHub releases and current default-branch commits on this date.
Aura uses project-maintained native I2C drivers in `src/drivers`, not installed
copies of the Arduino libraries below. Library versions are references, not Aura
firmware versions. No sensor source, calibration, dependency or bus setting changed.

| Sensor / official reference | Latest release; current commit | Result for Aura |
| --- | --- | --- |
| [SEN66 Arduino](https://github.com/Sensirion/arduino-i2c-sen66/releases/tag/1.3.1) | 1.3.1, 2026-06-08; `28dce74867c1675ecbc382c501c196cdb5416033` | Later August commits clarify wiring only. |
| [SEN66 Embedded C](https://github.com/Sensirion/embedded-i2c-sen66/releases/tag/1.3.1) | 1.3.1, 2026-09-14; `31cae0dd5253d256e8e647408ea1c544eff5d1c2` | Integer measurement command corrected from 0x0414 to 0x0300. Aura already sends 0x0300 (`SEN66_CMD_READ_VALUES`). |
| [SEN69C Arduino/C](https://github.com/Sensirion/arduino-i2c-sen69c/releases/tag/1.1.0) | 1.1.0, 2026-06-05; Arduino `2b5835f4fac813636dc796d3c6a471515f1be710`, C `eb198c6513f0f979655a3015e3ac2cd4f7c9b229` | Both unchanged since the October 7 audit. |
| [SFA4x Arduino/C](https://github.com/Sensirion/arduino-i2c-sfa4x/releases/tag/1.0.0) | 1.0.0, 2026-07-30; Arduino `9e22f15e32e03813864a0c8c4ba21237c31c7ea9`, C `1d7da43d9bd88ad5d9636d18807f52dbb6b86b4a` | Both unchanged since October 7; Aura already has the conservative 700 ms STOP waits. |
| [SFA3x](https://github.com/Sensirion/arduino-i2c-sfa3x/releases/tag/1.0.0) | 1.0.0, 2025-03-18; `bc5be3d349f8fea453ab45da767bdbf51c1766b7` | No recent update. Library API renaming/scaling does not require replacing Aura's native SFA30 transport. |
| [BMP388/390](https://github.com/boschsensortec/BMP3_SensorAPI/releases/tag/bmp3_v2.0.6) | 2.0.6, 2022-07-12; `db4cf8e4140c593b8c3d85f8c6c07335c7ffa9dc` | No recent update. |
| [BMP580/581/585](https://github.com/boschsensortec/BMP5_SensorAPI/releases/tag/v1.5.0) | 1.5.0, 2026-06-29; `3bd7c260419e4d08d7fa28343f27e0e6dd276411` | Substantial reference-driver update; applicability reviewed below. |
| [DPS310 / XENSIV](https://github.com/Infineon/arduino-xensiv-dps3xx/releases/tag/1.0.3) | 1.0.3, 2026-05-18; `6b90f368224cce170e336f280d9260dcf444cb83` | Old DPS310 repository is deprecated. Successor release concerns tooling/docs, DPS310 deprecation and Arduino GIGA SPI handling, not a required native ESP32 I2C measurement fix. |
| [SEN0466/0469 and optional MultiGas](https://github.com/DFRobot/DFRobot_MultiGasSensor/commit/93e72eb5239b0e8ca7f7f9a0803a060c94260d19) | No GitHub Release; latest commit `93e72eb5239b0e8ca7f7f9a0803a060c94260d19`, 2026-10-22 | New fix is Python-only: return concentration rather than zero when temperature correction is disabled. Aura directly validates/scales the sensor frame and has no Python tempSwitch path. |
| [SEN0321](https://github.com/DFRobot/DFRobot_OzoneSensor/commit/bf073b730981c0e4fc4a6e859173cee4edd8c1fe) | library 1.0.1; `bf073b730981c0e4fc4a6e859173cee4edd8c1fe`, 2023-08-02 | Exactly the reference already pinned in `docs/datasheets/SEN0321/DFRobot_OzoneSensor_reference/SOURCE.json`. |

## BMP5 applicability

The official [1.1.1 to 1.5.0 diff](https://github.com/boschsensortec/BMP5_SensorAPI/compare/v1.1.1...v1.5.0)
adds self-test support and improves FIFO parsing, fixed-point calculations,
error handling, IIR register preservation and explicit signed 24-bit temperature
conversion. Aura uses direct six-byte reads and floating-point T/65536 and P/64;
it does not use the upstream FIFO, fixed-point, NVM-write or self-test APIs.
Our configuration already preserves reserved IIR bits and checks I2C errors
before using read data. Its NVM-ready path does not depend on the extra POR
interrupt check removed upstream.

A useful separate cleanup candidate is replacing `Bmp580::signExtend24`'s signed
shift idiom with explicit conversion, and covering negative/boundary temperatures.
This audit did not reproduce incorrect readings or qualify a driver replacement.
Do not label the custom driver as an installed old Bosch library or automatically
import the entire upstream update into a beta candidate.

## Scope and evidence

Compared actual protocol/constants and changed paths, not just release numbers.
This is an update audit, not a full datasheet compliance review, accuracy test or
hardware qualification. RTCs, DAC, display/touch drivers and sensor-internal
firmware updates are outside this sensor-library audit.

Official metadata, checked refs and the DFRobot diff are retained locally in
`build/main-beta-20261028/upstream-audit.json`; previous detailed Sensirion work is
in [SENSOR_DRIVER_AUDIT_20261007.md](SENSOR_DRIVER_AUDIT_20261007.md).
Recommendation: retain the current sensor drivers for this prepared candidate;
consider BMP5 signed conversion/self-test work separately with focused tests and
hardware evidence. No new sensor driver update was installed.
