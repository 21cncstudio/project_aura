# ESP-IDF 6.1 hardware validation, 2026-09-11

This records the user-authorized app-only trial on the two identified Aura AQ
boards. It is separate from the earlier local build qualification. No release
publication, remote Git update, cold power cycle or OTA round trip is included.

Result: both final images passed write/readback integrity, preservation, boot and
runtime checks. Final-build physical display/touch confirmation is pending.
The local evidence index is `SUMMARY.json` in the evidence root below.

## Source and immutable candidates

- Branch: `codex/idf-6.1-migration`, based on main `78f4e9ba`.
- Firmware source: `0eadb703302e1c25134a0ad2073f049415d7406a`.
- ESP-IDF: v6.1, SDK commit `fff9895c82d744c7237be8847347bdd1b07c6643`.
- Evidence root: `D:\21cncstudio\project_aura\logs\idf61_hardware_20260911T172641Z`.
- Final BIN/ELF, generated configuration, bootloader, partition table, build log
  and checksums are frozen under `candidates\0eadb70\<profile>` in that root.
  The generated bootloader/partition/initial-OTA files were not installed.
- Both builds passed compiled identity, RTC layout, OTA descriptor/checksum/hash,
  legacy-only I2C linkage and upstream restart-order gates.
- Earlier `2cbd213` candidates are separately retained under `candidates\2cbd213`.
  The older `tmp\idf61-sdk\final-build-manifest.json` describes those candidates,
  not the final socket-fix images.

| Profile | USB port and serial / MAC | Build ID | Bytes | BIN SHA256 |
| --- | --- | --- | --- | --- |
| 4.3-inch | COM11, `7C:2C:67:89:70:68` | `0eadb70` | 4032656 | `e7371039efa7c400acf7e695020067f2dd1235686918fd8a7376f7f60025fc0b` |
| 7-inch, dual I2C | COM10, `20:6E:F1:AB:14:70` | `0eadb70-7-dual-i2c` | 4033056 | `5ffaf9c7095b31be078641b0c673f377926335a4cee32d0f6b664eecf1a22323` |

## First hardware finding and correction

The initial `2cbd213` image booted on the 4.3-inch board with working display and
touch, physically confirmed by the user. Wi-Fi connected, but `esp_http_server`
rejected its configuration: Aura requests ten client sockets and the server
needs three additional internal sockets; the new native defaults allowed ten
total lwIP sockets.

Commit `0eadb70` restores the old Arduino SDK limit of 16 and adds a compile-time
check that the HTTP configuration fits it. Both profiles were rebuilt. The final
4.3-inch runtime served the dashboard and `/api/diag`, `/api/state`, `/api/events`.

## Installation and preservation evidence

Installers verify the USB serial, chip MAC, ESP32-S3 identity, 16 MiB flash,
candidate hash/target and exact existing partition table. They derive the active
bootable app from CRC-checked OTA entries before selecting a write offset, and
accept only an existing `VALID` or `UNDEFINED` state, never a pending OTA trial.
Only that application region is written. There is no full erase, partition-table
replacement, OTA-selection edit, force option or automatic write retry.

Before writing, the old active application and bootloader/partition/NVS/OTA
prefix are saved locally. MD5 comparisons cover the prefix, inactive app and
LittleFS/coredump region before and after the write, before booting the new app.
Normal application changes to persistent data after boot are outside that
comparison. Backups contain device settings and should remain local.

The 4.3-inch active slot was app1, offset `0x650000`, size `0x640000`. Its original
IDF 5.3 image is retained in `4_3-flash-esptool481\active-app-before.bin`, full
partition SHA256 `7a5f8b9eb75c30db9536dfcaf671eea55fee97c16a2587c0459c34419e9b4e1b`.

The 7-inch active slot was app0, offset `0x10000`, size `0x640000`. Its previous
running build reported `f877906-7-dual-i2c-dirty`; its stored IDF descriptor was
`v5.3.2-282-gcfea4f7c98-dirty`. The complete old partition is retained in
`7_dual_i2c-socketfix-flash\active-app-before.bin`, SHA256
`c026cc96831976f497322fa3c6fcb81cff957e85f43a5d978b4f6142bcc1c273`.
The final 7-inch image passed built-in digest verification, complete readback
SHA256 and the outside-app preservation comparisons in a single installation
attempt. See `7_dual_i2c-socketfix-flash\RESULT.json`.

The `0eadb70` write passed esptool's digest verification. A subsequent 64 KiB
read command returned a truncated packet; its failure report and partial output
remain in `4_3-socketfix-flash`. A separate read-only verification, using one
4 KiB packet per command after both builds finished, read the entire image and
matched its SHA256. All outside-app region hashes matched the pre-write values.
No second write was needed. See
`4_3-socketfix-readonly-verification\RESULT.json` and its complete readback.

Earlier esptool 5.3 read attempts also failed before any firmware write and are
retained in this evidence directory. An isolated esptool 4.8.1 installation was
used for the successful transfers. The precise cause of the USB read failures
is not established; this is a transport limitation of the tested setup, not
evidence of flash corruption. The 7-inch installer uses 4 KiB reads from the start.

## Runtime results

The final 4.3-inch image passed the GET-only runtime checks at uptimes 30, 176,
346 and 802 seconds. Evidence: `4_3-runtime-socketfix`,
`4_3-runtime-socketfix-mature`, `4_3-runtime-after-warmup`, `4_3-runtime-final`
and the completed 150-second RX-only capture `4_3-after-socketfix`.

- Device identity: `0eadb70`, `4_3`, `aura-aq-v1`; serial startup reports IDF v6.1.
- Wi-Fi, retained hostname `aura-897068`, address `192.168.88.247`, and NTP work.
- Dashboard and all three API endpoints return success. Uptime increases.
- Board and LVGL are ready, with no board failure, touch-offline state, display
  sync fault, framebuffer wait timeout, ownership violation or runtime lock error.
- The shared sensor bus remains I2C0, SDA8/SCL9. Temperature, humidity, pressure,
  particulate, CO2 and HCHO readings are available. HCHO warmup clears independently.
  VOC/NOx become available after the configured 300-second SEN66 gas warmup.
- No crash or spontaneous restart was observed during these samples. The existing
  low RTC-battery warning remains. An old generic precompiled-IPC-stack warning
  is still logged, although the native build actually compiles with 4096 bytes.
- API boot `i2c_status=sda_stuck_low` is explicitly a non-live snapshot taken
  before board initialization; board initialization later succeeds. The old
  7-inch baseline has the same historical snapshot. USB return from download is
  reported as reset reason 11 / `UNMAPPED`; this is not physical power-on evidence.

The final 7-inch image passed GET-only runtime checks at uptimes 46 and
339 seconds. Evidence: `7_dual_i2c-runtime-socketfix`, `7_dual_i2c-runtime-final`
and the completed 150-second RX-only serial capture `7_dual_i2c-after-socketfix`.

- Device identity: `0eadb70-7-dual-i2c`, `7_dual_i2c`, `aura-aq-7-v1`;
  serial startup reports IDF v6.1 and an 8 MiB / 80 MHz PSRAM memory test pass.
- Wi-Fi, retained hostname `aura-ab1470`, address `192.168.88.246`, and NTP work.
- Dashboard and all three API endpoints return success. The board, LVGL and
  GT911 `0x5D` are ready. IRQ registration and configuration are verified.
- Display callbacks advance, the 180-degree rotation pipeline is active and
  rotated-copy counters advance. No display sync fault, framebuffer wait timeout,
  ownership violation, runtime lock error or touch-read error is present.
- Sensor routing remains separate I2C1, SDA44/SCL6, external pull-ups. Temperature,
  humidity, pressure, particulate and CO2 readings are available. VOC/NOx are
  initially withheld during the configured 300-second gas warmup; both become
  available afterwards, with `gas_warmup=false` in the final sample.
- Retained units, time format, offsets, altitude correction, NTP preferences,
  display name and network settings match the saved pre-upgrade API state.
  CO/HCHO/optional gas sensors and DAC were absent in that baseline and remain
  absent, so this board does not validate those peripherals.
- The stable-boot gate sees the existing app0 state `UNDEFINED` as not pending;
  no OTA selection/state rewrite was required. This does not test OTA rollback.
- Both boot logs contain a vendor GT911 address-initialization warning followed
  by a successful ID probe, and an already-installed GPIO ISR service message
  followed by successful direct IRQ replacement. Runtime touch checks pass.

The user confirmed image and touch on the earlier 4.3-inch `2cbd213` image.
Final-image physical feedback was requested for both boards and is still pending.
All serial captures are closed. No spontaneous reset was seen in the bounded
observations; this does not establish unattended or overnight stability.

## Scope limits

- Serial captures are RX-only after the installer returns the board from its
  USB download session. This does not test a physical OFF/ON power cycle.
- MQTT was disabled on the boards. HTTP reachability does not establish MQTT
  transport health; it was not enabled for this trial.
- Short boot/runtime checks do not establish long-term stability, OTA upgrade
  and rollback behavior, or operation of absent optional sensors and DAC hardware.
- LVGL remains 8.4.0. The IDF 5.5.5 LCD adapter remains a temporary compatibility
  layer over the single legacy I2C driver. This is the first migration stage.
- The root archive checkout, main, tested release checkpoint and COM8 are outside
  the installation scope. No remote publication was performed.
