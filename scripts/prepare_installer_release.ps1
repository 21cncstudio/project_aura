param(
  [string]$Env = "project_aura",
  [Parameter(Mandatory = $true)]
  [string]$Version,
  [ValidateSet("stable", "beta", "recovery")]
  [string]$Channel = "stable",
  [string]$KeyFile = (Join-Path $env:USERPROFILE ".aura-aq\installer-release-key.dpapi.json"),
  [string]$ReleaseNotes,
  [string]$RecoveryBinary,
  [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Security

function Resolve-NodeCommand {
  $command = Get-Command node -ErrorAction SilentlyContinue
  if ($command) {
    return $command.Path
  }
  $programFilesNode = Join-Path $env:ProgramFiles "nodejs\node.exe"
  if (Test-Path -LiteralPath $programFilesNode) {
    return $programFilesNode
  }
  throw "Node.js is required to prepare an installer release."
}

function Invoke-Checked {
  param(
    [string]$FilePath,
    [string[]]$Arguments
  )
  & $FilePath @Arguments
  if ($LASTEXITCODE -ne 0) {
    throw "Command failed: $FilePath $($Arguments -join ' ')"
  }
}

if ($env:OS -ne "Windows_NT") {
  throw "The installer release key is protected with Windows DPAPI; run this on the trusted Windows release computer."
}

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$platformioIni = Join-Path $root "platformio.ini"
$iniText = Get-Content -Raw -Path $platformioIni
$versionMatch = [regex]::Match($iniText, '-DAPP_VERSION=\\?"?([0-9A-Za-z._-]+)\\?"?')
if (-not $versionMatch.Success) {
  throw "APP_VERSION was not found in platformio.ini."
}
$configuredVersion = $versionMatch.Groups[1].Value

Push-Location $root
try {
  $commit = (& git rev-parse HEAD).Trim()
  if ($LASTEXITCODE -ne 0 -or $commit -notmatch "^[a-f0-9]{40}$") {
    throw "A full Git commit could not be resolved."
  }
  $dirtyLines = @(& git status --porcelain --untracked-files=all)
} finally {
  Pop-Location
}
$workingTreeClean = $dirtyLines.Count -eq 0
if (-not $workingTreeClean) {
  throw "The Git working tree is dirty. Commit the release source before packaging."
}

if (-not (Test-Path -LiteralPath $KeyFile)) {
  throw "Signing key not found. Run scripts\create_installer_release_key.ps1 first."
}
$keyDocument = Get-Content -Raw -LiteralPath $KeyFile | ConvertFrom-Json
if (
  $keyDocument.schema -ne "aura-installer-release-dpapi-key-v1" -or
  -not $keyDocument.key_id -or
  -not $keyDocument.protected_private_key
) {
  throw "The installer release signing key file is invalid."
}

$protectedBytes = [Convert]::FromBase64String([string]$keyDocument.protected_private_key)
$privateBytes = [System.Security.Cryptography.ProtectedData]::Unprotect(
  $protectedBytes,
  $null,
  [System.Security.Cryptography.DataProtectionScope]::CurrentUser
)
$privateKeyPem = [System.Text.Encoding]::UTF8.GetString($privateBytes)

$tag = "v$Version"
$releaseDirectory = Join-Path $root (Join-Path "release-assets" $tag)
$sourceDirectory = $releaseDirectory

if ($Channel -eq "recovery") {
  if (-not $RecoveryBinary -or -not (Test-Path -LiteralPath $RecoveryBinary)) {
    throw "Recovery releases require -RecoveryBinary path\to\recovery.bin."
  }
  $sourceDirectory = Join-Path $releaseDirectory "recovery-source"
  New-Item -ItemType Directory -Force -Path $sourceDirectory | Out-Null
  Copy-Item -Force -LiteralPath $RecoveryBinary -Destination (Join-Path $sourceDirectory "recovery.bin")
} else {
  $prepareAssets = Join-Path $PSScriptRoot "prepare_release_assets.ps1"
  $prepareArguments = @(
    "-ExecutionPolicy", "Bypass",
    "-File", $prepareAssets,
    "-Env", $Env,
    "-Version", $Version,
    "-SkipWebInstallerSync"
  )
  if ($SkipBuild) {
    $prepareArguments += "-SkipBuild"
  }
  Invoke-Checked -FilePath "powershell" -Arguments $prepareArguments
}

Push-Location $root
try {
  $commitAfterBuild = (& git rev-parse HEAD).Trim()
  if ($LASTEXITCODE -ne 0 -or $commitAfterBuild -ne $commit) {
    throw "The Git commit changed while preparing the release. Start the release command again."
  }
  $dirtyAfterBuild = @(& git status --porcelain --untracked-files=all)
  if ($dirtyAfterBuild.Count -ne 0) {
    throw "The build changed tracked or untracked source files. Review and commit them before packaging."
  }
} finally {
  Pop-Location
}

if (-not $ReleaseNotes) {
  $ReleaseNotes = Join-Path $root ("docs\releases\v{0}.md" -f $Version)
}
if (-not (Test-Path -LiteralPath $ReleaseNotes)) {
  throw "Release notes not found: $ReleaseNotes"
}

$stagingDirectory = Join-Path $releaseDirectory ".aura-package-staging-$Channel"
$archiveName = "aura-aq-v{0}-{1}.aura-release.zip" -f $Version, $Channel
$archivePath = Join-Path $releaseDirectory $archiveName
$temporaryArchive = "$archivePath.tmp.zip"
$node = Resolve-NodeCommand
$packager = Join-Path $PSScriptRoot "create_installer_release_package.mjs"

if (Test-Path -LiteralPath $stagingDirectory) {
  Remove-Item -Recurse -Force -LiteralPath $stagingDirectory
}
if (Test-Path -LiteralPath $temporaryArchive) {
  Remove-Item -Force -LiteralPath $temporaryArchive
}

try {
  $env:AURA_INSTALLER_RELEASE_PRIVATE_KEY_PEM = $privateKeyPem
  $packageResult = & $node $packager `
    --source $sourceDirectory `
    --staging $stagingDirectory `
    --notes $ReleaseNotes `
    --version $Version `
    --configured-version $configuredVersion `
    --channel $Channel `
    --commit $commit `
    --build-id "project-aura-v$Version-$($commit.Substring(0, 7))" `
    --key-id ([string]$keyDocument.key_id) `
    --working-tree-clean "true"
  if ($LASTEXITCODE -ne 0) {
    throw "Release package signing failed."
  }

  Compress-Archive -Path (Join-Path $stagingDirectory "*") -DestinationPath $temporaryArchive -CompressionLevel Optimal
  $archiveSize = (Get-Item -LiteralPath $temporaryArchive).Length
  if ($archiveSize -lt 1 -or $archiveSize -gt 64MB) {
    throw "Release package must be between 1 byte and 64 MB."
  }
  Move-Item -Force -LiteralPath $temporaryArchive -Destination $archivePath
} finally {
  Remove-Item Env:AURA_INSTALLER_RELEASE_PRIVATE_KEY_PEM -ErrorAction SilentlyContinue
  [Array]::Clear($privateBytes, 0, $privateBytes.Length)
  $privateKeyPem = $null
  if (Test-Path -LiteralPath $stagingDirectory) {
    Remove-Item -Recurse -Force -LiteralPath $stagingDirectory
  }
  if (Test-Path -LiteralPath $temporaryArchive) {
    Remove-Item -Force -LiteralPath $temporaryArchive
  }
}

Write-Host ""
Write-Host "Signed Aura Admin release package created:"
Write-Host $archivePath
Write-Host ""
Write-Host "Upload this ZIP in Aura Admin -> Firmware & Installers."
Write-Host "Import creates a Draft; publishing remains a separate action."
