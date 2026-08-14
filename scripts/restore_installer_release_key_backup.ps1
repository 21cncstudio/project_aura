param(
  [Parameter(Mandatory = $true)][string]$BackupPath,
  [string]$KeyDirectory = (Join-Path $env:USERPROFILE ".project_aura\signing"),
  [string]$NodePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Security

if (-not (Test-Path -LiteralPath $BackupPath)) {
  throw "Installer key backup does not exist: $BackupPath"
}
$privatePath = Join-Path $KeyDirectory "installer-release-private-key.dpapi"
$publicPath = Join-Path $KeyDirectory "installer-release-public-key.pem"
$metadataPath = Join-Path $KeyDirectory "installer-release-key.json"
foreach ($path in @($privatePath, $publicPath, $metadataPath)) {
  if (Test-Path -LiteralPath $path) {
    throw "Refusing to overwrite an existing installer release key file: $path"
  }
}
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

$password = Read-Host "Enter the installer key backup password" -AsSecureString
$passwordPointer = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($password)
$plainPassword = $null
$tempDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("aura-key-restore-" + [guid]::NewGuid())

try {
  $plainPassword = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($passwordPointer)
  $env:AURA_INSTALLER_BACKUP_PASSPHRASE = $plainPassword
  New-Item -ItemType Directory -Force -Path $tempDirectory | Out-Null
  $tempPrivate = Join-Path $tempDirectory "private.pem"
  $tempPublic = Join-Path $tempDirectory "public.pem"
  $tempMetadata = Join-Path $tempDirectory "metadata.json"

  & $node (Join-Path $PSScriptRoot "installer_key_backup.mjs") restore `
    --backup $BackupPath `
    --private-out $tempPrivate `
    --public-out $tempPublic `
    --metadata-out $tempMetadata | Out-Null
  if ($LASTEXITCODE -ne 0) {
    throw "Could not restore the encrypted installer key backup."
  }

  $privateBytes = [System.IO.File]::ReadAllBytes($tempPrivate)
  $protected = [System.Security.Cryptography.ProtectedData]::Protect(
    $privateBytes,
    $null,
    [System.Security.Cryptography.DataProtectionScope]::CurrentUser
  )
  [Array]::Clear($privateBytes, 0, $privateBytes.Length)
  $metadata = Get-Content -Raw -LiteralPath $tempMetadata | ConvertFrom-Json
  $metadata | Add-Member -NotePropertyName public_key_path -NotePropertyValue $publicPath

  New-Item -ItemType Directory -Force -Path $KeyDirectory | Out-Null
  [System.IO.File]::WriteAllText($privatePath, [Convert]::ToBase64String($protected))
  Copy-Item -LiteralPath $tempPublic -Destination $publicPath
  $metadata | ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath $metadataPath
  Write-Host "Installer release key restored and protected for this Windows account."
  Write-Host "Key ID: $($metadata.key_id)"
  Write-Host "Protected private key: $privatePath"
} finally {
  $env:AURA_INSTALLER_BACKUP_PASSPHRASE = $null
  $plainPassword = $null
  if ($passwordPointer -ne [IntPtr]::Zero) {
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($passwordPointer)
  }
  Remove-Item -LiteralPath $tempDirectory -Recurse -Force -ErrorAction SilentlyContinue
}
