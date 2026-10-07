# SEN69C and SFA40 upstream audit — Aura 1.2.1-beta

Checked 2026-10-07 against official Sensirion repositories and product downloads.

## Result

Aura uses project-maintained native I2C drivers, not installed copies of Sensirion's Arduino libraries. The applicable SEN69C measurement protocol is current. SFA40 needed one local timing alignment: its two remaining synchronous STOP paths now wait 700 ms, matching both official reference drivers. No concentration correction or calibration change was added.

Source commit: `0389ec9fb22f4a98297e67a0906070108939c071`.
Local Hub merge: `0a4c4a79fe892c1d12feaa397dc27084405b5d35`.
This report is added after those builds; subsequent documentation commits do not identify a newly built binary.

## Official driver versions

| Driver | Latest release | GitHub publication | Checked HEAD |
| --- | --- | --- | --- |
| [Arduino SEN69C](https://github.com/Sensirion/arduino-i2c-sen69c/releases/tag/1.1.0) | 1.1.0 | 2026-06-05 | `2b5835f4fac813636dc796d3c6a471515f1be710` |
| [Embedded C SEN69C](https://github.com/Sensirion/embedded-i2c-sen69c/releases/tag/1.1.0) | 1.1.0 | 2026-06-05 | `eb198c6513f0f979655a3015e3ac2cd4f7c9b229` |
| [Arduino SFA4x](https://github.com/Sensirion/arduino-i2c-sfa4x/releases/tag/1.0.0) | 1.0.0 | 2026-07-30 | `9e22f15e32e03813864a0c8c4ba21237c31c7ea9` |
| [Embedded C SFA4x](https://github.com/Sensirion/embedded-i2c-sfa4x/releases/tag/1.0.0) | 1.0.0 | 2026-07-30 | `1d7da43d9bd88ad5d9636d18807f52dbb6b86b4a` |

SFA4x's changelog labels 1.0.0 as May 12; July 30 is its actual GitHub public-release date. Generator model versions (SEN69C 1.2.1 / SFA4x 2.1.0) are distinct from library versions and Aura firmware versions.

The September 17 Python changes were also compared: [SEN69C](https://github.com/Sensirion/python-i2c-sen69c/compare/65e8c28f0f6959150fd95bcd18c3d02490238fc4...9eb0f5b165a1e680ce4fedcd8b8fc106a93581e4) and [SFA4x](https://github.com/Sensirion/python-i2c-sfa4x/compare/aafdfe7dd0ee9fdf2d3505646ffc3dd2c9c7a235...e5c217d9c785cba147e02888210b5d2e35b5b928). These change documentation, documentation dependencies and generation metadata; no sensor protocol implementation changed.

## SEN69C

The product page now links [SEN6x datasheet v1.0](https://sensirion.com/media/documents/FAFC548D/6AC50BC5/PS_DS_SEN6x.pdf). Its page footer says October 2026; its revision history dates v1.0 to September 2026. Earlier local implementation documentation used v0.92.

Compared the active measurement command `0x04B5`, ten-word CRC frame, signed values and missing-value sentinels, HCHO /10 scaling, 24-second CO2 conditioning, first HCHO availability, identity commands, STOP/reset delays, pressure/ASC/FRC operations and sticky status handling. No required change to the implemented paths was found.

The [1.1.0 upstream change](https://github.com/Sensirion/arduino-i2c-sen69c/compare/1.0.0...1.1.0) adds optional product-type identification and corrects images. Aura already identifies the exact product name and serial through `0xD014` / `0xD033`; adding `0xD002` is not required to read measurements.

## SFA40

The latest [Arduino implementation](https://github.com/Sensirion/arduino-i2c-sfa4x/blob/9e22f15e32e03813864a0c8c4ba21237c31c7ea9/src/SensirionI2cSfa4x.cpp) and [Embedded C implementation](https://github.com/Sensirion/embedded-i2c-sfa4x/blob/1d7da43d9bd88ad5d9636d18807f52dbb6b86b4a/sfa4x_i2c.c) both wait 700 ms after STOP.

Aura already used that interval before protocol detection and during cooperative startup. `Sfa40::stop()` and `ensureIdleBeforeStart()` still used the shared SFA30 50 ms constant. Both now use the existing SFA40 700 ms constant. SFA30 timing is unchanged.

There is a public-source discrepancy: the [SFA40 v1.1 PDF, April 2026](https://sensirion.com/media/documents/5B06EDD9/69F84BD8/Sensirion_Datasheet_SFA40.pdf) lists 25 ms. This change follows the more conservative official driver behavior; it does not prove the previous 50 ms caused a physical failure or elevated HCHO.

Measurement command, CRC, production status byte, HCHO scaling and RH/T conversion already agree with the checked sources. Optional product-ID diagnostics were not added. Legacy B4 support remains an Aura compatibility path; this audit does not newly qualify B4 hardware.

Synchronous STOP callers now wait 650 ms longer. Normal polling and cooperative startup timing are unchanged.

## Verification

- The two new STOP regression cases fail on the previous driver and pass after the fix.
- SFA40 real-driver/mock-I2C suite: 29 passed.
- SEN69C real-driver suite: 30 passed; SensorManager routing suite: 60 passed.
- Both ESP-IDF profiles built successfully in main and Hub: four builds total.
- All five stamped artifacts per build passed hash/identity verification.
- Existing dirty files were byte-preserved before updating the local CURRENT_STATE notes. Shared driver/config/test contents match between branches after Windows line-ending normalization.
- Builds retain the pre-existing dirty UI/EEZ work; the build IDs accurately contain `dirty`. No device was flashed/reset, no release signed and nothing pushed or published.

### Application artifacts

| Line/profile | Build ID | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| main 4.3 | `0389ec9-dirty` | 4646352 | `2f5ae893f2dd1a27d8ef62cc28c90f4befa7ed9c3a36b48bbcd70e553ce4e059` |
| main 7 | `0389ec9-7-dual-i2c-dirty` | 4646720 | `51e19acf7805d78b0d774dbd0a6a052db86d6f9b5f494903a0521969a983ca6b` |
| Hub 4.3 | `0a4c4a7-dirty` | 4720640 | `eda779c4d1b62a71c9576cb589128050d2a7f42e0c7177c05da85e0b249fb79e` |
| Hub 7 | `0a4c4a7-7-dual-i2c-dirty` | 4720976 | `acbe04b55e44b10a4d1208d4cd2252d069d6a6a8f978100279be21195f25aff6` |

Local evidence: `../firmware-main/build-idf-sensor-audit-20261007/` contains upstream metadata, the downloaded SEN6x PDF/text, before-state snapshots, regression/build logs, build-results.json, and separate before/after artifacts. Native runner reports: `20261007T104607Z-9381aa77` (expected regression failure), `20261007T104641Z-ef77866b` (29 passed), `20261007T104706Z-436414a5` (90 passed).

Physical behavior and accuracy remain unverified by this software audit. Any hardware qualification or beta publication is a separate step.
