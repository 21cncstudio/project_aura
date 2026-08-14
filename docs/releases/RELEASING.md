# Releasing Firmware

## 1) Bump firmware version
- Edit `platformio.ini`:
  - `-DAPP_VERSION=\"X.Y.Z\"`

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

For every Stable release, run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\prepare_installer_release.ps1 -Version X.Y.Z
```

The command builds the firmware, checks that Git is clean, hashes every binary,
signs the release metadata with the control-plane Ed25519 key, and creates:

`release-assets/vX.Y.Z/aura-aq-vX.Y.Z-stable.aura-release.zip`

Upload that ZIP to **Aura Admin -> Firmware & Installers**. Import creates a
Draft. Publishing is a separate action.

Recovery is deliberately packaged separately:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\prepare_installer_release.ps1 `
  -Version X.Y.Z -Channel recovery -RecoveryBinary C:\prepared\recovery.bin
```

The Stable ZIP never contains Recovery firmware.

The package signature protects release metadata and distribution artifacts. It
is separate from the experimental device OTA signing work and from ESP32 Secure
Boot.

## 3) Publish to GitHub Release
Recommended command:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\publish_github_release.ps1 -Version X.Y.Z -SkipReleaseUpdate -PruneAssetsToList
```

Notes:
- `-SkipReleaseUpdate` avoids metadata PATCH (useful on flaky networks).
- `-PruneAssetsToList` removes stale assets from previous uploads.
- By default, the script uploads only:
  - `project_aura_X.Y.Z_ota_firmware.bin`

## 4) Website usage
- Aura Admin publishes immutable binaries to Private Blob.
- The protected Installer receives short-lived URLs from the current release pointer.
- Rollback changes the pointer; it never overwrites a published binary.

## 5) OTA dashboard update
- Use `project_aura_X.Y.Z_ota_firmware.bin` in `/dashboard -> System -> Update Firmware`.
