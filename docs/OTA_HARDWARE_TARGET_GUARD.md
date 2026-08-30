# Built-in OTA hardware-target guard

## Scope and migration

The built-in web OTA endpoint accepts only an application BIN with the same
embedded target as the running firmware:

| Running firmware | Accepted target |
| --- | --- |
| Aura AQ 4.3-inch (`project_aura`) | `aura-aq-v1` |
| Aura AQ 7-inch (`project_aura_7`) | `aura-aq-7-v1` |

Protection starts only after a firmware containing this guard is installed.
Old running firmware cannot acquire the check retroactively. The first guarded
firmware must still be installed using the correct model selected by the operator.
Afterward, legacy BINs without the descriptor are rejected, even if their
filename or historical version suggests the right model. There is no web-OTA
override. A deliberate downgrade or service recovery can still use USB/esptool.

This is accidental cross-model protection, not a digital signature, trusted
physical-board identification, or a replacement for signed release packages.
It compares against the running firmware profile. A manual service flash can
still install the wrong profile, and relabelled malicious firmware is outside
this guard's threat model. Public installer/Aura Link release and 7-inch
qualification gates remain unchanged.

## Standard ESP32-S3 BIN layout

The image remains a normal ESP32 application BIN. There is no prepended wrapper
and no filename-based selection. The descriptor uses ESP-IDF's fixed
`.rodata_custom_desc` slot after the standard application descriptor. The pinned
[ESP-IDF 5.3.2 format and retention rules](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/api-reference/system/app_image_format.html#adding-a-custom-structure-to-an-application)
define this location; static assertions and a post-BIN check enforce it locally.

| File offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 24 | Standard ESP image header |
| 24 | 8 | First segment header |
| 32 | 256 | Standard `esp_app_desc_t` |
| 288 (`0x120`) | 16 | `AURA_OTA_TARGET` plus NUL |
| 304 | 2 | Little-endian descriptor version, currently 1 |
| 306 | 2 | Little-endian descriptor length, currently 64 |
| 308 | 32 | Canonical target string, NUL-terminated and zero-padded |
| 340 | 12 | Reserved, all zero |

The descriptor is explicitly retained by `-Wl,-u,aura_ota_image_identity`.
The build fails if either production BIN lacks its exact expected target at
offset 288, has an invalid ESP checksum/hash, or disagrees with the generated
build identity. `tools/ota_image_identity.py` performs the same whole-artifact
check without opening a serial port.

## Device acceptance path

1. Keep the existing physical confirmation, admission gate, image-size/slot
   checks, deadlines, and boot-validation restriction.
2. Accept at most the first 352 bytes into a fixed RAM buffer. Multipart Start
   does not call `Update.begin()`. Arbitrarily split chunks are supported.
3. Check ESP32-S3 application framing and the versioned model descriptor.
4. Reject mismatched, absent, malformed, unsupported, or truncated identity
   before `Update.begin()` or `Update.write()`. Stop the multipart stream and
   reach the normal final response/cleanup path, with a bounded pending-body
   drain rather than receiving the whole rejected file.
5. Only for a compatible prefix, call `Update.begin()`, write the buffered prefix
   exactly once, and continue with the rest of that chunk and later chunks.
6. Require the declared byte count and normal OTA finalization before success
   or scheduling a restart. A failed attempt clears neither its original reason
   nor its explicit code; the next admitted attempt resets both and the prefix.

The zero-write guarantee applies to rejection at the identity/header gate.
Later body corruption, disconnects, or flash-write failures can occur after
writing has started; they use the normal abort/finalization path. The metadata
check does not replace whole-image verification or rollback policy.

## User feedback and API

The primary error is inline in **System > Firmware OTA**, associated with the
file selector and announced by a live region. It stays visible across ordinary
state refreshes and late upload-progress events. The failed progress indicator
resets and file selection/retry become available. Choosing another file clears
the previous message. There is no new LCD error modal; failed OTA cleanup hides
the update screen and restores the regular UI.

| Failure | HTTP | `error_code` |
| --- | --- | --- |
| Known but different model | 409 | `HARDWARE_TARGET_MISMATCH` |
| Legacy or absent descriptor | 400 | `HARDWARE_TARGET_MISSING` |
| Unsupported descriptor version/layout | 400 | `HARDWARE_METADATA_UNSUPPORTED` |
| Invalid target, malformed or incomplete header | 400 | `INVALID_FIRMWARE` |

For these rejections, POST `/api/ota` reports `success:false`, `written:0`, and
`rebooting:false`. `/api/state` retains the same error/code for the existing
terminal-result TTL, allowing the browser to recover a lost upload response.
Text and codes are not inferred from the filename or untrusted form fields.

## Local checks and physical follow-up

Local regression coverage includes the real OTA handlers with a fake flash
writer, every prefix split/truncation, both target directions, unlabelled BINs,
malformed identity, cleanup/retry, write failures, deadlines, explicit API
codes, and the real embedded JavaScript callbacks. A transport source-contract
test guards the early-return link; it is not a live TCP/HTTPD test.

Both production builds must pass the post-BIN identity/integrity check. Archive
their generated build identities and SHA256 hashes under the exact committed
source before any physical installation. A raw BIN is not a signed public
release package.

The next physical test is separate: install the matching guarded firmware with
explicit device authorization, try the opposite-model BIN through web OTA,
verify the warning, zero written count, unchanged uptime/build, restored LCD,
and a subsequent matching-file update. Repeat in the opposite direction and
check a legacy unlabelled BIN. Do not infer physical success from native tests
or local builds, and do not disturb running soak loggers without agreement.
