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
