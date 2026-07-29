# Releasing Firmware

## 1) Bump firmware version
- Edit `platformio.ini`:
  - `-DAPP_VERSION=\"X.Y.Z\"`

## 2) One-time Aura Admin signing setup

On the trusted Windows release computer, run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\create_installer_release_key.ps1
```

The private Ed25519 key is encrypted for the current Windows user with DPAPI.
Only add the printed public-key JSON to
`AURA_FIRMWARE_SIGNING_PUBLIC_KEYS` in Aura Link.

## 3) Prepare the signed Aura Admin package

Commit the release source, then run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\prepare_installer_release.ps1 -Version X.Y.Z
```

The command builds Full and Update images, verifies `APP_VERSION`, Git state,
flash layout and release notes, then creates:

```text
release-assets/vX.Y.Z/aura-aq-vX.Y.Z-stable.aura-release.zip
```

Drop this one ZIP into **Aura Admin > Firmware & Installers**. Importing creates
a Draft. Review the checks, then publish it with the separate **Publish**
action. Previous releases stay available for rollback.

The current package format uses the hardened v2 signature contract. It covers
the product title, release-notes hash, asset kinds, offsets, modes, sizes and
binary SHA-256 hashes. Regenerate older local ZIP files before importing them.

Recovery is packaged separately:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\prepare_installer_release.ps1 `
  -Version X.Y.Z `
  -Channel recovery `
  -RecoveryBinary path\to\recovery.bin
```

## 4) Prepare legacy/GitHub assets

Run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\prepare_release_assets.ps1 -Version X.Y.Z
```

This prepares files in `release-assets/vX.Y.Z` and refreshes local website installer files in `web-installer/`.

Important:
- GitHub Release should publish OTA file only.
- Full installer binaries/manifests are for your private hosting workflow.

Main generated files:
- `bootloader.bin`
- `partitions.bin`
- `boot_app0.bin`
- `firmware.bin`
- `littlefs.bin`
- `manifest.json`
- `manifest-update.json`
- `project_aura_X.Y.Z_ota_firmware.bin`
- `sha256sums.txt`

## 5) Publish to GitHub Release
Recommended command:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\publish_github_release.ps1 -Version X.Y.Z -SkipReleaseUpdate -PruneAssetsToList
```

Notes:
- `-SkipReleaseUpdate` avoids metadata PATCH (useful on flaky networks).
- `-PruneAssetsToList` removes stale assets from previous uploads.
- By default, the script uploads only:
  - `project_aura_X.Y.Z_ota_firmware.bin`

## 6) Website Installer

Aura Admin owns the private release binaries and current-release pointer.
The protected installer manifest receives short-lived exact-object URLs from
the control plane. Do not copy new manifests or binaries into the public site.

## 7) OTA dashboard update
- Use `project_aura_X.Y.Z_ota_firmware.bin` in `/dashboard -> System -> Update Firmware`.
