# Aura dependency update, 2026-10-05

Status: local source update and validation completed on 2026-10-05. No device or publication evidence is implied.

## Selected versions

| Component | Before | Updated local version |
| --- | --- | --- |
| ESP-IDF | 6.1, `fff9895c82d744c7237be8847347bdd1b07c6643` | unchanged |
| Arduino-ESP32 | 3.3.11 | 3.3.12 |
| LVGL ESP component | 9.5.0 | 9.6.0~1 (LVGL 9.6.0) |
| mDNS | 1.12.0 | 1.14.0 |
| esp-modbus (Arduino transitive dependency) | 2.1.3 | 2.1.4 |
| ArduinoJson | 7.4.3 firmware, ^7.0.0 host tests | 7.4.3 in both |

Sources checked on 2026-10-05:
- [Arduino 3.3.12 release](https://github.com/espressif/arduino-esp32/releases/tag/3.3.12).
  Its component manifest permits IDF >=5.3 and <6.2. The release title's IDF
  5.5.5 refers to its prebuilt Arduino distribution, not Aura's native IDF build.
- [LVGL 9.6 release](https://github.com/lvgl/lvgl/releases/tag/v9.6.0),
  [migration guide](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/changelog/migration-v9-6.mdx),
  [ESP registry revision](https://components.espressif.com/components/lvgl/lvgl/versions/9.6.0~1/readme).
- [mDNS 1.14 changelog](https://components.espressif.com/components/espressif/mdns/versions/1.14.0/changelog?language=en).

Arduino 4.0.0-RC1 is a prerelease and is not selected. Keep the current patched
Display Panel 1.0.5, IO Expander 1.1.1 and esp-lib-utils 0.3.0. The registry's
Display Panel 1.0.4 would not replace Aura's local native-IDF adaptations.
MQTT 1.1.0, ArduinoJson 7.4.3 and LittleFS 1.22.3 already match the checked
stable registry versions. Arduino pins some unused transitive components;
do not override network_provisioning/Zigbee/Ethernet compatibility ranges just
to make every lockfile entry match the newest independent release.

## Compatibility work

- Preserve the fail-closed Arduino STA overlay. Its existing callback regression
  executes against the actual new managed source and checks six reconnect cases.
- Select RGB565 through LVGL 9.6's explicit `LV_COLOR_FORMAT_DEFAULT` setting.
  Remove the obsolete native-build `LV_COLOR_16_SWAP` define and retain generic,
  null and allocation assertions. Set argument validation explicitly so IDF
  Kconfig and the host renderer use the same setting.
- Migrate old cached SDK settings without resetting the profiles:
  `CONFIG_LV_MEM_SIZE_KILOBYTES=0` and `CONFIG_LV_ASSERT_HANDLER_INCLUDE=""`.
  The byte-sized heap setting remains available; Aura uses its custom
  PSRAM-first allocator. These legacy zero/empty values are also recorded in
  `sdkconfig.defaults.idf61` for new configurations. Existing external caches
  must apply these two values when moving from LVGL 9.5. The initial firmware
  build rejected the old defaults; warning/error enforcement remains enabled.
- Disable optional desktop dependency fetching and ThorVG in the host renderer.
- Keep the EEZ source and existing theme edits intact. The EEZ project's 9.5
  generator target remains; generated v9 APIs still work through LVGL 9.6's
  compatibility mappings. A v10 migration would require a separate generator/API
  review. Some old object-flag calls emit LVGL deprecation messages.
- Native firmware uses `main/idf_component.yml` and `dependencies.lock`.
  The old PlatformIO firmware configuration remains blocked and is retained
  for profile metadata; it is not the installed firmware dependency set.

## Evidence and preservation

Main evidence: `build-idf-dependency-update-20261005/` in firmware-main.
Hub evidence: the same relative directory in firmware-hub.
These ignored local directories contain before-state hashes and byte-for-byte
copies of pre-existing edits, configuration and both BIN/ELF/map artifact sets.
They are local recovery/test evidence, not release packages.

The before/after host renderer exercises 15 generated screens and a CO2 warmup
state, plus all 65,536 persisted RGB565 values. The raw generated sensor-info
screen intentionally contains overlapping conditional groups; runtime selection
is outside this small harness. Compare it to the baseline, not a physical UI.

## Future maintenance

Check stable upstream releases and the current lockfile before every dependency
update. Use exact direct pins plus the checked-in lockfile; update in reviewable
steps rather than making normal builds fetch floating latest versions.

1. Back up dirty authored files and exact build artifacts; preserve main/Hub isolation.
2. Review compatibility and Aura's Wi-Fi, I2C/display and EEZ adaptations.
3. Resolve with `scripts/build_idf.ps1 -Action reconfigure` using the existing SDK.
4. Run `scripts/run_tests.ps1`, Python tool tests, EEZ/font checks and the host UI renderer.
5. Build both profiles with the organization-root `tools/build.ps1` wrapper.
6. Commit only the update, merge main forward into local feature/aura-link, and
   validate both Hub profiles plus relevant Hub client tests.
7. Record source identities, exact artifact hashes and remaining limits.
   Flashing, hardware validation and publication require their own authorization.

No periodic job or automatic upgrade of devices is configured by this change.

## Completed host validation

- Main native launcher: 1,140 cases passed, 125 suite/environment runs across
  10 environments. Evidence: `.pio/native-tests/reports/20261005T110429Z-8783939b/launcher.json`.
- Python tools: 113 tests passed, including real Arduino STA callback scenarios.
- EEZ postprocessor check and all six font-coverage checks passed.
- All 16 LVGL frames rendered and all RGB565 values round-tripped. Nine frames
  match exactly; other differences are limited to <=0.887% of pixels per frame.
  The largest is a small text-position change in the CO2 calibration prompt;
  the reviewed before/after images preserve its content and layout.

- Aura-to-Hub client tests: 60 cases passed across seven suites, with verified
  Unity source identity. Evidence in firmware-hub:
  `.pio/native-tests/reports/20261005T112335Z-af74705f/launcher.json`.
- The shared SDK remains clean at exact ESP-IDF v6.1 commit shown above.

## Final firmware artifacts

Code commit on main: `b2ffddb0cdb7daf00ad3bca74ddfaf40c1c4342d`.
Forward merge on feature/aura-link: `0aee9f4ac91b928f21fbd9386b4560a2f6baa100`.
Any subsequent report-only commit does not describe a new compiled source revision.
All four builds passed compiled build identity, RTC retained-layout ABI,
OTA image identity/checksum/hash, new I2C driver/native LCD linkage, and
restart/cache ordering checks. All fit the existing application partitions.

The `-dirty` suffix records the preserved, pre-existing local changes, including
user-authored UI edits. Those original files were checked byte-for-byte against
the before-state snapshots after all builds. AGENTS and CURRENT_STATE then
received only the applicable version/status documentation updates.

| Line | Profile | Build ID | App bytes | App SHA-256 |
| --- | --- | --- | ---: | --- |
| main | 4_3 | `b2ffddb-dirty` | 4,646,336 | `0a5cc7c46d95ddc6947499f59fc9c3c000ab4662daf9b2f54105ee76b0421add` |
| main | 7_dual_i2c | `b2ffddb-7-dual-i2c-dirty` | 4,646,704 | `c2f7c539aed36756d59005810979c8b6ad3bd423c80d4e4115f5ef63d63c16a1` |
| hub | 4_3 | `0aee9f4-dirty` | 4,720,624 | `5f65518ed0284c85f9ce49788c614dc2409de5e69738c2f46b589eefdb4fa553` |
| hub | 7_dual_i2c | `0aee9f4-7-dual-i2c-dirty` | 4,720,960 | `92a373b03e939b2a02c8764dd15e92385380456e70b876ef2e6ff38afe2f0c94` |

Each evidence directory contains `after-manifest.json` and verified copies in
`after/<profile>/` of all five installer assets, ELF/map, identity and artifact
stamp. The stamp was checked against the actual bytes. The before-state archive
remains intact. Normal build outputs remain in `build-idf-<profile>/`.

Validation is local: no board was flashed, reset or read through serial, and no
remote Git ref, release, production service or running HA/MQTT setup was changed.
Screen rendering and unit tests do not establish physical touch/display behavior,
Wi-Fi reconnect behavior or sensor performance. Hardware smoke checks are the
next step before choosing any of these candidates for a device or release.
