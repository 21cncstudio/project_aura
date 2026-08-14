param(
  [string]$KeyDirectory = (Join-Path $env:USERPROFILE ".project_aura\signing"),
  [string]$BackupDirectory = (Join-Path $env:USERPROFILE ".project_aura\backups"),
  [string]$AdditionalBackupDirectory,
  [switch]$UseClipboardPassphrase,
  [string]$NodePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Security

$privatePath = Join-Path $KeyDirectory "installer-release-private-key.dpapi"
$publicPath = Join-Path $KeyDirectory "installer-release-public-key.pem"
$metadataPath = Join-Path $KeyDirectory "installer-release-key.json"
$node = $NodePath
if (-not $node) {
  $node = (Get-Command node -ErrorAction SilentlyContinue).Source
}
if (-not $node) {
  $node = Join-Path $env:ProgramFiles "nodejs\node.exe"
}
if (-not (Test-Path $node)) {
  $node = Join-Path $env:USERPROFILE ".cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe"
}
if (-not (Test-Path $node)) {
  throw "Node.js is required. Install Node.js or pass -NodePath <path-to-node.exe>."
}
foreach ($path in @($privatePath, $publicPath, $metadataPath)) {
  if (-not (Test-Path -LiteralPath $path)) {
    throw "Installer release key file is missing: $path"
  }
}

$metadata = Get-Content -Raw -LiteralPath $metadataPath | ConvertFrom-Json
$safeKeyId = [string]$metadata.key_id
if ($safeKeyId -notmatch '^aura-installer-ed25519-[a-f0-9]{16}$') {
  throw "Installer release key metadata contains an invalid key ID."
}
$fileName = "$safeKeyId.aura-key-backup"
$primaryBackup = Join-Path $BackupDirectory $fileName
$additionalBackup = if ($AdditionalBackupDirectory) {
  Join-Path $AdditionalBackupDirectory $fileName
} else {
  $null
}
foreach ($path in @($primaryBackup, $additionalBackup)) {
  if ($path -and (Test-Path -LiteralPath $path)) {
    throw "Backup already exists and will not be overwritten: $path"
  }
}

$passwordPointer = [IntPtr]::Zero
$confirmationPointer = [IntPtr]::Zero
$plainPassword = $null
$plainConfirmation = $null
$tempDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("aura-key-backup-" + [guid]::NewGuid())
$completed = $false

try {
  if ($UseClipboardPassphrase) {
    $plainPassword = Get-Clipboard -Raw
    if ([string]::IsNullOrEmpty($plainPassword)) {
      throw "The clipboard does not contain a backup password."
    }
    if ($plainPassword -match '[\r\n]') {
      throw "The clipboard password must be a single line."
    }
    $plainConfirmation = $plainPassword
  } else {
    $password = Read-Host "Create a backup password (minimum 16 characters)" -AsSecureString
    $confirmation = Read-Host "Repeat the backup password" -AsSecureString
    $passwordPointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($password)
    $confirmationPointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($confirmation)
    $plainPassword = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($passwordPointer)
    $plainConfirmation = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($confirmationPointer)
  }
  if ($plainPassword.Length -lt 16) {
    throw "The backup password must contain at least 16 characters."
  }
  if ($plainPassword -cne $plainConfirmation) {
    throw "The backup passwords do not match."
  }
  $env:AURA_INSTALLER_BACKUP_PASSPHRASE = $plainPassword
  New-Item -ItemType Directory -Force -Path $BackupDirectory | Out-Null
  New-Item -ItemType Directory -Force -Path $tempDirectory | Out-Null
  $tempPrivate = Join-Path $tempDirectory "private.pem"
  $restoredPrivate = Join-Path $tempDirectory "restored-private.pem"
  $restoredPublic = Join-Path $tempDirectory "restored-public.pem"
  $restoredMetadata = Join-Path $tempDirectory "restored-metadata.json"

  $protected = [Convert]::FromBase64String((Get-Content -Raw -LiteralPath $privatePath).Trim())
  $privateBytes = [System.Security.Cryptography.ProtectedData]::Unprotect(
    $protected,
    $null,
    [System.Security.Cryptography.DataProtectionScope]::CurrentUser
  )
  [System.IO.File]::WriteAllBytes($tempPrivate, $privateBytes)
  [Array]::Clear($privateBytes, 0, $privateBytes.Length)

  & $node (Join-Path $PSScriptRoot "installer_key_backup.mjs") create `
    --private-key $tempPrivate `
    --public-key $publicPath `
    --metadata $metadataPath `
    --output $primaryBackup | Out-Null
  if ($LASTEXITCODE -ne 0) {
    throw "Could not create the encrypted installer key backup."
  }

  & $node (Join-Path $PSScriptRoot "installer_key_backup.mjs") restore `
    --backup $primaryBackup `
    --private-out $restoredPrivate `
    --public-out $restoredPublic `
    --metadata-out $restoredMetadata | Out-Null
  if ($LASTEXITCODE -ne 0) {
    throw "Could not verify the encrypted installer key backup."
  }
  if ((Get-FileHash -Algorithm SHA256 -LiteralPath $tempPrivate).Hash -ne (Get-FileHash -Algorithm SHA256 -LiteralPath $restoredPrivate).Hash) {
    throw "Backup verification restored different private key bytes."
  }
  if ((Get-FileHash -Algorithm SHA256 -LiteralPath $publicPath).Hash -ne (Get-FileHash -Algorithm SHA256 -LiteralPath $restoredPublic).Hash) {
    throw "Backup verification restored a different public key."
  }
  if ((Get-Content -Raw -LiteralPath $restoredMetadata | ConvertFrom-Json).key_id -ne $safeKeyId) {
    throw "Backup verification restored a different key ID."
  }

  if ($additionalBackup) {
    New-Item -ItemType Directory -Force -Path $AdditionalBackupDirectory | Out-Null
    Copy-Item -LiteralPath $primaryBackup -Destination $additionalBackup
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $primaryBackup).Hash -ne (Get-FileHash -Algorithm SHA256 -LiteralPath $additionalBackup).Hash) {
      throw "The additional backup copy does not match the primary backup."
    }
  }

  $backupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $primaryBackup).Hash.ToLowerInvariant()
  $completed = $true
  Write-Host "Installer key backup created and round-trip verified."
  Write-Host "Key ID: $safeKeyId"
  Write-Host "Primary backup: $primaryBackup"
  if ($additionalBackup) {
    Write-Host "Additional backup: $additionalBackup"
  }
  Write-Host "SHA-256: $backupHash"
} finally {
  $env:AURA_INSTALLER_BACKUP_PASSPHRASE = $null
  $plainPassword = $null
  $plainConfirmation = $null
  if ($passwordPointer -ne [IntPtr]::Zero) {
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($passwordPointer)
  }
  if ($confirmationPointer -ne [IntPtr]::Zero) {
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($confirmationPointer)
  }
  Remove-Item -LiteralPath $tempDirectory -Recurse -Force -ErrorAction SilentlyContinue
  if (-not $completed) {
    Remove-Item -LiteralPath $primaryBackup -Force -ErrorAction SilentlyContinue
    if ($additionalBackup) {
      Remove-Item -LiteralPath $additionalBackup -Force -ErrorAction SilentlyContinue
    }
  }
}
