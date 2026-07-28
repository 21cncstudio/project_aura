param(
  [string]$Env = "project_aura",
  [Parameter(Mandatory = $true)][string]$Version,
  [ValidateSet("stable", "beta", "recovery")][string]$Channel = "stable",
  [string]$KeyDirectory = (Join-Path $env:USERPROFILE ".project_aura\signing"),
  [string]$OutputRoot = "release-assets",
  [string]$NodePath,
  [string]$RecoveryBinary,
  [switch]$SkipBuild,
  [switch]$AllowDirty
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Security

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$privatePath = Join-Path $KeyDirectory "installer-release-private-key.dpapi"
$metadataPath = Join-Path $KeyDirectory "installer-release-key.json"
$notesPath = Join-Path $root ("docs\releases\v{0}.md" -f $Version)
$platformioIni = Join-Path $root "platformio.ini"
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

Push-Location $root
try {
  if (-not $AllowDirty) {
    $dirty = (& git status --porcelain).Trim()
    if ($dirty) {
      throw "Git working tree is dirty. Commit or stash changes before creating a signed release."
    }
  }
  $commit = (& git rev-parse HEAD).Trim()
  if ($LASTEXITCODE -ne 0 -or $commit -notmatch "^[a-f0-9]{40}$") {
    throw "Could not resolve the source Git commit."
  }
} finally {
  Pop-Location
}

if (-not (Test-Path $privatePath) -or -not (Test-Path $metadataPath)) {
  throw "Installer release signing key is missing. Run scripts\create_installer_release_key.ps1 first."
}
if (-not (Test-Path $notesPath)) {
  throw "Release notes are required: $notesPath"
}

if (Test-Path $platformioIni) {
  $iniText = Get-Content -Raw -LiteralPath $platformioIni
  $match = [regex]::Match($iniText, '-DAPP_VERSION=\\?"?([0-9A-Za-z._-]+)\\?"?')
  if ($match.Success -and $match.Groups[1].Value -ne $Version) {
    throw "Requested version $Version does not match platformio.ini version $($match.Groups[1].Value)."
  }
}

$metadata = Get-Content -Raw -LiteralPath $metadataPath | ConvertFrom-Json
$sourceDir = Join-Path $root (Join-Path $OutputRoot ("v" + $Version))
$bundleName = "aura-aq-v{0}-{1}.aura-release.zip" -f $Version, $Channel
$bundlePath = Join-Path $sourceDir $bundleName
$stagingDir = Join-Path ([System.IO.Path]::GetTempPath()) ("aura-release-" + [guid]::NewGuid())
$tempPrivate = Join-Path ([System.IO.Path]::GetTempPath()) ("aura-release-" + [guid]::NewGuid() + ".pem")

try {
  if ($Channel -eq "recovery") {
    if (-not $RecoveryBinary -or -not (Test-Path -LiteralPath $RecoveryBinary)) {
      throw "Recovery releases require -RecoveryBinary <path-to-signed-recovery.bin>."
    }
    New-Item -ItemType Directory -Force -Path $sourceDir | Out-Null
    Copy-Item -Force -LiteralPath $RecoveryBinary -Destination (Join-Path $sourceDir "recovery.bin")
  } else {
    & powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "prepare_release_assets.ps1") `
      -Env $Env -Version $Version -OutputRoot $OutputRoot -SkipWebInstallerSync -SkipBuild:$SkipBuild
    if ($LASTEXITCODE -ne 0) {
      throw "Firmware asset preparation failed."
    }
  }

  $protected = [Convert]::FromBase64String((Get-Content -Raw -LiteralPath $privatePath).Trim())
  $privateBytes = [System.Security.Cryptography.ProtectedData]::Unprotect(
    $protected,
    $null,
    [System.Security.Cryptography.DataProtectionScope]::CurrentUser
  )
  [System.IO.File]::WriteAllBytes($tempPrivate, $privateBytes)

  & $node (Join-Path $PSScriptRoot "package_installer_release.mjs") `
    --source $sourceDir `
    --staging $stagingDir `
    --version $Version `
    --channel $Channel `
    --commit $commit `
    --key-id $metadata.key_id `
    --private-key $tempPrivate `
    --notes $notesPath | Out-Null
  if ($LASTEXITCODE -ne 0) {
    throw "Release package validation or signing failed."
  }

  if (Test-Path $bundlePath) {
    throw "Release package already exists: $bundlePath"
  }
  Compress-Archive -Path (Join-Path $stagingDir "*") -DestinationPath $bundlePath -CompressionLevel Optimal
  Write-Host ""
  Write-Host "Signed Aura Admin package created:"
  Write-Host $bundlePath
  Write-Host ""
  Write-Host "Upload this ZIP in Aura Admin -> Firmware & Installers."
} finally {
  Remove-Item -LiteralPath $tempPrivate -Force -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath $stagingDir -Recurse -Force -ErrorAction SilentlyContinue
}
