# Releasing Firmware

## 1) Bump firmware version
- Edit `platformio.ini`:
  - Stable: `-DAPP_VERSION=\"X.Y.Z\"`
  - Beta: `-DAPP_VERSION=\"X.Y.Z-beta\"`

The release channel and version shape are bound together. Stable rejects a
prerelease suffix, while Beta requires one.

## 2) Create the Aura Admin package

One-time setup on the release computer:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\create_installer_release_key.ps1
```

Keep the generated DPAPI-protected private key on this Windows account. Add the
public PEM and the printed key ID to `AURA_FIRMWARE_SIGNING_PUBLIC_KEYS` in Aura
Link.

Immediately create a portable encrypted backup. Use a long unique password and
store it in a password manager, not beside the backup file:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\backup_installer_release_key.ps1 `
  -AdditionalBackupDirectory "E:\Project Aura Keys"
```

The command creates a local backup under `.project_aura\backups`, copies the
same encrypted file to the additional directory, and performs a full restore
test before reporting success. A copied DPAPI file alone is not a portable
backup because it is tied to the Windows account that protected it.

For local automation, `-UseClipboardPassphrase` reads a single-line password
from the Windows clipboard without placing it in command-line arguments or
console output. Use it only when the clipboard is under the release operator's
control.

To recover on another Windows account, use a clean target key directory:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\restore_installer_release_key_backup.ps1 `
  -BackupPath "E:\Project Aura Keys\aura-installer-ed25519-KEYID.aura-key-backup"
```

The producer supports exactly two production identities:

| PlatformIO environment | Hardware target | Hardware profile |
| --- | --- | --- |
| `project_aura` | `aura-aq-v1` | `4_3` |
| `project_aura_7` | `aura-aq-7-v1` | `7_dual_i2c_scl6` |

For a 4.3-inch Stable package, run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\prepare_installer_release.ps1 -Version X.Y.Z
```

For a 7-inch Stable package, run the same producer with the explicit environment:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\prepare_installer_release.ps1 `
  -Env project_aura_7 -Version X.Y.Z
```

For the current Beta candidate, use the matching channel and source version:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\prepare_installer_release.ps1 `
  -Env project_aura -Version X.Y.Z-beta -Channel beta

powershell -ExecutionPolicy Bypass -File scripts\prepare_installer_release.ps1 `
  -Env project_aura_7 -Version X.Y.Z-beta -Channel beta
```

The command builds the firmware, checks that Git is clean, hashes every binary,
signs the release metadata with the control-plane Ed25519 key, and creates:

```text
release-assets/v<source-version>/aura-aq-v1/aura-aq-v1-v<effective-version>-<channel>.aura-release.zip
release-assets/v<source-version>/aura-aq-7-v1/aura-aq-7-v1-v<effective-version>-<channel>.aura-release.zip
```

The selected build emits a machine-readable identity. Only after both firmware
and filesystem builds succeed, the producer records the exact SHA-256 and size
of every binary in a post-build artifact stamp. Packaging rejects stale or
changed output, a dirty source identity, any environment/target/profile/build
ID mismatch, non-canonical flash offsets, or invalid ESP32-S3 binary headers.
Both ZIPs retain the importer's required generic filenames inside the archive
while their directories and ZIP names remain target-specific.

`-SkipBuild` is safe only when the exact generated identity and post-build
artifact stamp are still present; both are revalidated before packaging. A
prerelease such as `X.Y.Z-beta` receives one effective version containing its
build ID, and that same version is used in manifests, filenames, signatures,
and Aura Admin.

Upload that ZIP to **Aura Admin -> Firmware & Installers**. Import creates a
Draft. Publishing is a separate action.

Recovery package creation is currently fail-closed for both profiles. It must
not be re-enabled until a dedicated recovery build produces its own
artifact-bound identity and the resulting image is physically qualified. The
Stable ZIP never contains Recovery firmware.

The package uses the signature-v2 canonical payload accepted by Aura Link. The
signature protects the title, release-notes hash, hardware target/profile,
generated build identity, asset kinds, offsets, modes, sizes, and hashes. It
is separate from the experimental device OTA signing work and from ESP32 Secure
Boot.

## 3) Publish to GitHub Release

Only the qualified 4.3-inch target may currently be published externally:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\publish_github_release.ps1 `
  -Version X.Y.Z -HardwareTarget aura-aq-v1 -SkipReleaseUpdate
```

The publisher rejects `aura-aq-7-v1` locally, before credential lookup or any
GitHub request. Do not remove that gate until
[`SEVEN_INCH_QUALIFICATION.md`](SEVEN_INCH_QUALIFICATION.md) is complete and the
unlock change has its own review.

Notes:
- `-SkipReleaseUpdate` preserves metadata on an already existing release. A new
  release is still created as a draft, populated, and only then finalized.
- `-PruneAssetsToList` is intentionally rejected because a per-target run could
  delete the other profile's asset.
- The publisher verifies the target/profile/build identity, manifest identity,
  post-build firmware hash, checksum list, and exact source commit before any
  GitHub operation.
- Published files are immutable. An identical retry is a no-op when GitHub
  reports the matching SHA-256 and size; different bytes under the same name
  fail closed and require a new version/tag.
- If upload of a newly created release fails, the source-bound draft is retained
  for a safe retry. Untagged or differently bound drafts are never reused.
- Default target-qualified names are:
  - `project_aura_4_3_X.Y.Z_ota_firmware.bin`
  - `project_aura_7_X.Y.Z_ota_firmware.bin`

## 4) Website usage
- Aura Admin publishes immutable binaries to Private Blob.
- The protected Installer requests an explicit hardware target and receives
  short-lived URLs only from that target's current pointer.
- Rollback changes only the matching product/channel/target pointer; it never
  overwrites a published binary or changes the other board profile.

## 5) OTA dashboard update
- Use the OTA file whose target matches the physical board. Raw dashboard OTA
  does not identify the connected display hardware, so this remains a manual,
  operator-verified path.

Do not publish the 7-inch package until Aura Link and the protected installer
are deployed with target isolation and the exact clean 7-inch artifact has
passed the required physical baseline.
