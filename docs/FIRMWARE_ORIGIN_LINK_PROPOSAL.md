# Installed firmware information in Link: proposed first step

Status: discussion only, 2026-09-17. No implementation or production change.

## What existing evidence establishes

Link verifies an Ed25519-signed release payload when importing a package. Its
signature covers release identity, target/profile, metadata and artifact hashes.
This establishes which bytes the publisher authorized for distribution. It does
not establish which bytes currently execute on a remote device.

The current device update-availability path compares a reported version with a
compatible published release. That signal answers whether an update is available,
not whether the running firmware is authentic. The experimental firmware-signing
branch separately checks signed provenance against its own running partition;
it is not hardware attestation and is not imported by this cleanup.

## Recommended initial product behavior

Keep normal firmware information in the device Firmware/About section: version,
build ID, hardware profile and last report time. Avoid a permanent main-header
badge. Keep update availability separate from origin information.

| Evidence | Suggested wording | Meaning |
| --- | --- | --- |
| Device reports a hash matching an eligible signed catalog artifact | Matches an Aura release, reported by device | Catalog match using device-reported data |
| Explicitly identified custom/development build | Custom firmware | Declared custom build, not a security verdict |
| No hash, old firmware, unknown release, or stale report | Origin not confirmed | Insufficient evidence, not proof of third-party origin |
| Verification fails during an actual update-package check | Update verification failed | That candidate update failed; show its specific reason |

Reserve “Unofficial firmware” for positively identified third-party builds if
that distinction helps the owner. Never derive it merely from an unknown version,
missing signature, connectivity loss, or a developer build. This status alone
should not block sensing, history or ordinary Link use.

## Small implementation proposal for a later task

1. Add a bounded, versioned firmware-information report with product/target,
   hardware profile, version, build ID and SHA-256 of the defined application
   image byte range. Use the exact range/length represented in the signed catalog,
   not the whole padded partition or a differently signed upload container.
2. Compute the hash once per boot/update in a worker, with bounded buffers and
   watchdog-friendly work. Never hash flash in the LVGL task or on every reading.
3. Deliver through the existing authenticated device/Hub path. Bind the report
   to the specific sensor identity, not the Hub's own identity. Validate fields
   and record server receipt time and evidence source. Compatibility checks must
   still enforce the target, profile and artifact kind.
4. Compare with a known, non-revoked signed release and present the result as
   device-reported. Retain stale/unknown explicitly rather than keeping a green
   authenticity badge indefinitely.
5. Test official, custom, old, missing, stale and malformed reports, same-version
   different images, target mismatches, OTA/rollback and replayed reports.

A hash or signed metadata returned by arbitrary firmware can be fabricated.
TLS and an authenticated device key identify the reporting endpoint; they do
not independently measure its executing code. A server challenge improves
freshness but does not turn a self-reported hash into measured attestation.

Stronger authenticity would require a separately designed trusted boot and
measurement chain, protected keys, provisioning, update/rollback and recovery
policies. Secure Boot alone is not a remotely verified measurement. That work
is disproportionate to removing a UI badge and is outside this task.

## Decision to make before implementation

Start with accurately labelled device-reported release matching if owners or
support need it. Do not introduce an automatic unofficial-firmware warning just
because package verification already exists. First finish the real Aura-to-Hub-
to-Link measurement/history check; then assess the value of this additional
Firmware-tab information.
