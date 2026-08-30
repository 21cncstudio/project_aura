param(
  [string]$Env = "project_aura",
  [string]$Version,
  [string]$BuildId,
  [string]$Repo = "21cncstudio/project_aura",
  [string]$Tag,
  [string]$OutputRoot = "release-assets",
  [switch]$SkipBuild,
  [switch]$SkipWebInstallerSync
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "release_identity.ps1")
. (Join-Path $PSScriptRoot "release_layout.ps1")

function Write-Step {
  param([string]$Message)
  Write-Host ("[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $Message)
}

function Resolve-PlatformioCommand {
  $preferred = Join-Path $env:USERPROFILE ".platformio\\penv\\Scripts\\platformio.exe"
  if (Test-Path $preferred) {
    return $preferred
  }

  $fallback = Get-Command platformio -ErrorAction SilentlyContinue
  if ($fallback) {
    return $fallback.Path
  }

  throw "PlatformIO executable not found. Expected: $preferred"
}

function Invoke-Platformio {
  param(
    [string]$Exe,
    [string[]]$PioArgs
  )

  & $Exe @PioArgs
  if ($LASTEXITCODE -ne 0) {
    throw "PlatformIO failed: $Exe $($PioArgs -join ' ')"
  }
}

function Get-PartitionOffset {
  param(
    [string]$CsvPath,
    [string]$Name
  )

  if (-not (Test-Path $CsvPath)) {
    return $null
  }

  $matches = @()
  $lines = Get-Content $CsvPath | Where-Object { $_ -and $_ -notmatch "^\s*#" }
  foreach ($line in $lines) {
    $parts = $line.Split(",") | ForEach-Object { $_.Trim() }
    if ($parts.Count -ge 4 -and $parts[0] -eq $Name) {
      $matches += $parts[3]
    }
  }

  if ($matches.Count -gt 1) {
    throw "Partition table contains duplicate '$Name' entries: $CsvPath"
  }
  if ($matches.Count -eq 1) {
    return $matches[0]
  }
  return $null
}

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$platformioIni = Join-Path $root "platformio.ini"
$buildDir = Join-Path $root (".pio\\build\\{0}" -f $Env)

$configuredVersion = $null
if (Test-Path $platformioIni) {
  $iniText = Get-Content $platformioIni -Raw
  $match = [regex]::Match($iniText, '-DAPP_VERSION=\\?"?([0-9A-Za-z._-]+)\\?"?')
  if ($match.Success) {
    $configuredVersion = $match.Groups[1].Value
  }
}

if (-not $Version) {
  $Version = $configuredVersion
} elseif ($configuredVersion -and $Version -ne $configuredVersion) {
  throw "Requested version $Version does not match platformio.ini version $configuredVersion."
}

if (-not $Version) {
  throw "Version not found. Pass -Version 1.1.0 or set -DAPP_VERSION in platformio.ini."
}
[void](Get-AuraEffectiveVersion -Version $Version -BuildId "validation")

if (-not $Tag) {
  $Tag = "v$Version"
}

$contract = Get-AuraHardwareContract -Environment $Env
if (-not $SkipWebInstallerSync -and $contract.HardwareTarget -ne "aura-aq-v1") {
  throw "The legacy local web-installer sync is 4.3-inch-only. Use -SkipWebInstallerSync for $($contract.HardwareTarget)."
}
$releaseRoot = Join-Path $root (Join-Path $OutputRoot $Tag)
$outDir = Join-Path $releaseRoot $contract.HardwareTarget
if (Test-Path -LiteralPath $outDir) {
  throw "Target-specific release directory already exists: $outDir"
}

if (-not $SkipBuild) {
  $platformioExe = Resolve-PlatformioCommand
  Write-Step "Building firmware"
  Invoke-Platformio -Exe $platformioExe -PioArgs @("run", "-e", $Env)
  Write-Step "Building filesystem image"
  Invoke-Platformio -Exe $platformioExe -PioArgs @("run", "-e", $Env, "-t", "buildfs")
}

$identityPath = Join-Path $buildDir "generated\build-identity.json"
$identity = Read-AuraBuildIdentity `
  -IdentityPath $identityPath `
  -Environment $Env `
  -RepositoryRoot $root `
  -ExpectedBuildId $BuildId
$displayVersion = Get-AuraEffectiveVersion -Version $Version -BuildId $identity.BuildId

$required = @("bootloader.bin", "partitions.bin", "firmware.bin", "littlefs.bin")
foreach ($name in $required) {
  $path = Join-Path $buildDir $name
  if (-not (Test-Path $path)) {
    throw "Missing build output: $path"
  }
}

$bootApp0 = Join-Path $env:USERPROFILE ".platformio\\packages\\framework-arduinoespressif32\\tools\\partitions\\boot_app0.bin"
if (-not (Test-Path $bootApp0)) {
  throw "Missing boot_app0.bin at $bootApp0"
}

$partitionsCsv = Join-Path $root $identity.PartitionsFile
if (-not (Test-Path -LiteralPath $partitionsCsv)) {
  throw "Selected environment partition file is missing: $partitionsCsv"
}

$app0Offset = Get-PartitionOffset -CsvPath $partitionsCsv -Name "app0"
if (-not $app0Offset) {
  throw "Partition table does not define app0: $partitionsCsv"
}
$app0Offset = Assert-AuraCanonicalFlashOffset `
  -Value $app0Offset `
  -ExpectedValue 0x10000 `
  -PartitionName "app0"

$littlefsOffset = Get-PartitionOffset -CsvPath $partitionsCsv -Name "littlefs"
$spiffsOffset = Get-PartitionOffset -CsvPath $partitionsCsv -Name "spiffs"
if ($littlefsOffset -and $spiffsOffset) {
  throw "Partition table defines both littlefs and spiffs; expected exactly one filesystem partition: $partitionsCsv"
}
$filesystemPartitionName = if ($littlefsOffset) { "littlefs" } else { "spiffs" }
$littlefsOffset = if ($littlefsOffset) { $littlefsOffset } else { $spiffsOffset }
if (-not $littlefsOffset) {
  throw "Partition table does not define littlefs/spiffs: $partitionsCsv"
}
$littlefsOffset = Assert-AuraCanonicalFlashOffset `
  -Value $littlefsOffset `
  -ExpectedValue 0xC90000 `
  -PartitionName $filesystemPartitionName

$artifactInputs = Get-AuraArtifactInputs -BuildDirectory $buildDir -BootApp0Path $bootApp0
$artifactStampPath = Join-Path $buildDir "generated\release-artifacts.json"
if (-not $SkipBuild) {
  Write-AuraReleaseArtifactStamp `
    -StampPath $artifactStampPath `
    -Identity $identity `
    -ArtifactInputs $artifactInputs
}
$artifactStamp = Read-AuraReleaseArtifactStamp `
  -StampPath $artifactStampPath `
  -Identity $identity `
  -ArtifactInputs $artifactInputs

New-Item -ItemType Directory -Path $outDir | Out-Null

Write-Step "Copying release binaries"
Copy-Item -Force (Join-Path $buildDir "bootloader.bin") (Join-Path $outDir "bootloader.bin")
Copy-Item -Force (Join-Path $buildDir "partitions.bin") (Join-Path $outDir "partitions.bin")
Copy-Item -Force (Join-Path $buildDir "firmware.bin") (Join-Path $outDir "firmware.bin")
Copy-Item -Force (Join-Path $buildDir "littlefs.bin") (Join-Path $outDir "littlefs.bin")
Copy-Item -Force $bootApp0 (Join-Path $outDir "boot_app0.bin")
Copy-Item -LiteralPath $artifactStampPath (Join-Path $outDir "release-artifacts.json")

$otaFileName = "project_aura_{0}_{1}_ota_firmware.bin" -f $identity.ArtifactSlug, $displayVersion
Copy-Item -Force (Join-Path $buildDir "firmware.bin") (Join-Path $outDir $otaFileName)

$manifestFull = [ordered]@{
  name = "Project Aura"
  version = $displayVersion
  hardware_target = $identity.HardwareTarget
  hardware_profile = $identity.HardwareProfile
  build_id = $identity.BuildId
  # ESP Web Tools: skip the Improv provisioning wait because Aura uses its own setup flow.
  new_install_improv_wait_time = 0
  builds = @(
    [ordered]@{
      chipFamily = "ESP32-S3"
      parts = @(
        [ordered]@{ path = "bootloader.bin"; offset = "0x0000" }
        [ordered]@{ path = "partitions.bin"; offset = "0x8000" }
        [ordered]@{ path = "boot_app0.bin"; offset = "0xE000" }
        [ordered]@{ path = "firmware.bin"; offset = $app0Offset }
        [ordered]@{ path = "littlefs.bin"; offset = $littlefsOffset }
      )
    }
  )
}
$manifestUpdate = [ordered]@{
  name = "Project Aura"
  version = $displayVersion
  hardware_target = $identity.HardwareTarget
  hardware_profile = $identity.HardwareProfile
  build_id = $identity.BuildId
  # ESP Web Tools: skip the Improv provisioning wait because Aura uses its own setup flow.
  new_install_improv_wait_time = 0
  new_install_prompt_erase = $true
  builds = @(
    [ordered]@{
      chipFamily = "ESP32-S3"
      parts = @(
        [ordered]@{ path = "firmware.bin"; offset = $app0Offset }
      )
    }
  )
}

Write-Step "Writing release manifests"
$manifestFull | ConvertTo-Json -Depth 8 | Set-Content -Encoding Ascii (Join-Path $outDir "manifest.json")
$manifestUpdate | ConvertTo-Json -Depth 8 | Set-Content -Encoding Ascii (Join-Path $outDir "manifest-update.json")
$releaseIdentity = [ordered]@{
  schema = $identity.Schema
  environment = $identity.Environment
  source_commit = $identity.SourceCommit
  build_id = $identity.BuildId
  hardware_profile = $identity.HardwareProfile
  hardware_target = $identity.HardwareTarget
  artifact_slug = $identity.ArtifactSlug
  partitions_file = $identity.PartitionsFile
}
$releaseIdentity | ConvertTo-Json -Depth 4 | Set-Content -Encoding Ascii (Join-Path $outDir "release-identity.json")

$releaseNotes = Join-Path $root ("docs\\releases\\v{0}.md" -f $Version)
if (Test-Path $releaseNotes) {
  Copy-Item -Force $releaseNotes (Join-Path $outDir ("release-notes-v{0}.md" -f $Version))
}

$hashFiles = @(
  "bootloader.bin",
  "partitions.bin",
  "boot_app0.bin",
  "firmware.bin",
  "littlefs.bin",
  "manifest.json",
  "manifest-update.json",
  "release-identity.json",
  "release-artifacts.json",
  $otaFileName
)
$hashLines = foreach ($fileName in $hashFiles) {
  $filePath = Join-Path $outDir $fileName
  $hash = Get-AuraFileSha256 -Path $filePath
  "$hash  $fileName"
}
$hashLines | Set-Content -Encoding Ascii (Join-Path $outDir "sha256sums.txt")

if (-not $SkipWebInstallerSync) {
  $webPrepareScript = Join-Path $root "web-installer\\prepare_web_installer.ps1"
  if (Test-Path $webPrepareScript) {
    Write-Step "Syncing local web-installer files"
    & powershell -ExecutionPolicy Bypass -File $webPrepareScript -Env $Env -Version $Version -SkipBuild
    if ($LASTEXITCODE -ne 0) {
      throw "Failed to sync web-installer files."
    }
  } else {
    Write-Warning "web-installer\\prepare_web_installer.ps1 not found, skipping local web-installer sync."
  }
}

Write-Host ""
Write-Host "Release assets prepared in: $outDir"
Write-Host "Tag: $Tag"
Write-Host "Hardware target: $($identity.HardwareTarget)"
Write-Host "Hardware profile: $($identity.HardwareProfile)"
Write-Host "Build ID: $($identity.BuildId)"
Write-Host "Upload this file to GitHub Release:"
Write-Host " - $otaFileName"
Write-Host "Other files in release-assets are for local installer workflow."
Write-Host ""
if ($SkipWebInstallerSync) {
  Write-Host "Legacy local web-installer sync was skipped."
} else {
  Write-Host "Local website installer files are refreshed in: $root\\web-installer"
}
