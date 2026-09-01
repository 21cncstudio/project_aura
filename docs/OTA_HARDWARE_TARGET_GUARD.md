# Built-in OTA hardware-target and firmware-flavor guard

## Scope and migration

The built-in web OTA endpoint accepts only an application BIN allowed by both
its embedded hardware identity and firmware flavor:

| Running firmware | Accepted embedded identity |
| --- | --- |
| Aura AQ 4.3-inch production (`project_aura`) | `aura-aq-v1` + `production` |
| Aura AQ 7-inch production (`project_aura_7`) | `aura-aq-7-v1` + `production` |
| Aura AQ 7-inch diagnostic (`project_aura_7_gt911_5d`) | `aura-aq-7-diag-v1` + `diagnostic`, or `aura-aq-7-v1` + `production` as an exit |

Production firmware never accepts diagnostic firmware through web OTA.
Diagnostic firmware may update within the diagnostic lane and may return to the
normal production lane. Entering the diagnostic lane requires a separately
controlled service/USB installation; the OTA endpoint is not an entry path.
`aura-aq-7-diag-v1` is an internal OTA lane marker, not a third physical model
or a public release hardware target. APIs and packages continue to identify the
physical 7-inch model as `aura-aq-7-v1`.

Protection starts only after a firmware containing this guard is installed.
Old running firmware cannot acquire the check retroactively. The first guarded
firmware must still be installed using the correct model selected by the operator.
The original target-only guard cannot understand the appended flavor record.
To close the first-transition gap, a new diagnostic BIN deliberately places
`aura-aq-7-diag-v1` in its original 64-byte target descriptor. An already
installed old production guard rejects that unknown target before flash. The
new production BIN keeps the old `aura-aq-v1` or `aura-aq-7-v1` descriptor
unchanged, so an old production guard can install the new guarded production
firmware. Afterward, legacy target-only BINs are rejected, even if their
filename or historical version suggests the right model. There is no web-OTA
override. A deliberate downgrade or service recovery can still use USB/esptool.

An old target-only diagnostic firmware, if one was ever installed, can OTA to
the new production image as its exit path, but cannot directly recognize the
new diagnostic lane marker. The reviewed diagnostic candidate preceding this
change was recorded as unflashed, so no hardware PASS is inferred here.

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
| 352 | 16 | `AURA_OTA_FLAVOR` plus NUL |
| 368 | 2 | Little-endian flavor descriptor version, currently 1 |
| 370 | 2 | Little-endian flavor descriptor length, currently 32 |
| 372 | 12 | `production` or `diagnostic`, NUL-terminated and zero-padded |

The descriptor is explicitly retained by `-Wl,-u,aura_ota_image_identity`.
The build fails if any of the three firmware environments lacks its exact
target/flavor pair, has an invalid ESP checksum/hash, or disagrees with the
generated build identity. `tools/ota_image_identity.py` performs the same
whole-artifact check without opening a serial port.

## Device acceptance path

1. Keep the existing physical confirmation, admission gate, image-size/slot
   checks, deadlines, and boot-validation restriction.
2. Accept at most the first 384 bytes into a fixed RAM buffer. Multipart Start
   does not call `Update.begin()`. Arbitrarily split chunks are supported.
3. Check ESP32-S3 application framing, the original model descriptor, the
   appended flavor descriptor, and the allowed target/flavor pair.
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
| Diagnostic BIN offered to production firmware | 409 | `FIRMWARE_FLAVOR_MISMATCH` |
| Legacy or absent descriptor | 400 | `HARDWARE_TARGET_MISSING` |
| Unsupported descriptor version/layout | 400 | `HARDWARE_METADATA_UNSUPPORTED` |
| Target exists but flavor descriptor is absent | 400 | `FIRMWARE_FLAVOR_MISSING` |
| Unsupported flavor descriptor | 400 | `FIRMWARE_FLAVOR_UNSUPPORTED` |
| Unknown flavor or inconsistent target/flavor pair | 400 | `FIRMWARE_FLAVOR_INVALID` / `FIRMWARE_IDENTITY_INVALID` |
| Invalid target, malformed or incomplete header | 400 | `INVALID_FIRMWARE` |

For these rejections, POST `/api/ota` reports `success:false`, `written:0`, and
`rebooting:false`. `/api/state` retains the same error/code for the existing
terminal-result TTL, allowing the browser to recover a lost upload response.
Text and codes are not inferred from the filename or untrusted form fields.

### Repeated requests after rejection

The first failure retains its original 60-second result deadline. A later POST
with the same previously validated physical-confirmation ID and declared size
returns that failure without creating a new OTA session, extending the deadline,
showing Installing again, or invoking flash operations. The consumed confirmation
does not authorize another write. A new upload requires a new valid confirmation.

Requests with a different or invalid confirmation, invalid size, pending boot
validation, or a busy admission gate receive their own HTTP refusal without
replacing the retained result. Request-local response data and session ownership
prevent late callbacks or an unrelated request from cleaning up another upload
or reporting a previous upload as its own success. Invalid/missing size returns
HTTP 400 with `INVALID_SIZE` before opening a session.

This preserves the reason even if the browser or network repeats a rejected
POST. It does not establish the source of those repeats or change HTTPD drain,
connection-close, or response-delivery behavior. Physical confirmation and
zero-write rejection still need verification on the exact installed candidate.

## Local checks and physical follow-up

Local regression coverage includes the real OTA handlers with a fake flash
writer, every prefix split/truncation, both target directions, the production
and diagnostic lanes, the diagnostic-to-production exit, legacy target-only
BINs, malformed identity, cleanup/retry, write failures, deadlines, explicit
API codes, and the real embedded JavaScript callbacks. A transport
source-contract test guards the early-return link; it is not a live TCP/HTTPD
test.

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

### Dated candidate results, 2026-08-31

The local dirty candidate at HEAD 5383b77 has now completed the scoped API/browser
checks on both devices. Exact records are under
`D:\21cncstudio\project_aura\logs\ota_hardware_feedback_20260831T124146Z`:
`RESULT_4_3_BLOCK.md` and `RESULT_7_BLOCK.md`.

The checked BIN SHA256 values are:

- 4.3-inch: `57027220ae40b228da87e480116569eaa6ed048c2b89213965fd38ffa0455930`.
- Normal 7-inch: `36a1173803c421eecf5c707026c2666e9a8244aa20cdb13f4e3d487eaa38d6d9`.

Both model directions and genuine missing-metadata BINs retained the original
text/code across two 352-byte prefix replays using consumed confirmation IDs.
API counters stayed at zero written bytes, observed uptime did not reset, and
replays did not extend the original TTL. New matching uploads then completed
with physical confirmation, automatic restart and new-partition stable_boot.

This is neither physical flash readback nor proof of full-file retransmission
behavior. Operator screen/touch reports, negative-case physical-observation
gaps, and two corrected/offline-audited 4.3-inch recorder limitations are recorded
per case. These results do not establish the cause of transport timeouts, qualify
a different BIN, validate GT911 0x5D or authorize a release.

These dated BINs predate the appended flavor descriptor and the distinct
diagnostic OTA lane. Their 352-byte results remain evidence for those exact
artifacts only and do not qualify the new 384-byte target/flavor guard.
