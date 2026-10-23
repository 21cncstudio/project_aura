# SEN0466 optional absence log, 2026-10-23

Local main source change based on `26339000`; no device or published release change.

When a never-detected optional SEN0466 does not acknowledge its address and the
bus is idle after the probe, the driver now logs `SEN0466 not installed` at INFO.
Previously this path used WARN with `ESP_FAIL`, while suppressing user alerts.
The latter behavior is present in stable v1.2.0 and beta source `2dddfa2`.

The existing absence predicate is unchanged. A timeout, a line held low after
the probe, a required-device failure, and loss of a previously detected sensor
retain the existing warning and raw diagnostics. Probe timing and retries are
unchanged. The flexible optional gas slot keeps its existing INFO path.

## Existing implementation and upstream check

The sibling `firmware-hub` driver has the same absence classifier; it was read
for comparison, not modified. This is a logging change using the existing
classifier and Logger, so no new driver or classification layer is needed.

[Espressif I2C probe documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2c.html)
distinguishes an absent address from a timeout. Aura's existing I2C adapter maps
NOT_FOUND/INVALID_RESPONSE to ESP_FAIL. This patch does not change that mapping
or infer electrical health from a GPIO snapshot.

## Validation

Existing host suites passed: probe log policy (6), recent logger (9), event
mirroring (4), optional gas / SEN0466 driver (31): **50 cases**.
The production probe-log branch is excluded by UNIT_TEST, so host tests alone
do not exercise the replacement LOGI call; firmware compilation checks it.

Build evidence and the previous artifacts, copied and SHA256-verified before
building, are in `build-idf-sen0466-log-20261023/` (ignored local directory).
Both native ESP-IDF builds passed: 4_3 and 7_dual_i2c. EEZ postprocess --check and git diff --check passed; src/ui has no diff.

Published beta/stable packages, the Hub branch and installed devices are unchanged.

Local unflashed app images (SHA256):

- 4_3: b461c14fe026016baa195aa6722f8e4c9699b2c90398c66b997a30fbe74cbeeb (4643872 bytes).
- 7_dual_i2c: 7be0cd9dfb75a53f4ab417c8de63c4df49d475ec25b048bd5a5e938d5fddca10 (4646432 bytes).

## Follow-up: requested Aura-to-Hub port

The source-only main fix is committed as `4cc06321`.
At the owner's request it was cherry-picked as `f73c6ea2` into the current
Aura-to-Hub BLE/Outdoor/Wi-Fi checkout:
`D:/21cncstudio/aura-aq-hub/build/aura-wifi-recovery-20261021/aura`, branch
`codex/aura7-wifi-recovery-20261021`. This supersedes the earlier no-Hub-change
statement for the original main-only stage. Historical UI/archive branches,
installed devices and published releases remain unchanged.

The Hub branch independently passed the same 50 host cases, both IDF profile
builds, EEZ --check and clean-source review. Evidence is recorded in that
checkout's `docs/SEN0466_OPTIONAL_ABSENCE_LOG_20261023.md` (documentation commit
`d90f726c`, firmware builds from `f73c6ea2`). No remote push or flashing.
