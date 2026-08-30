function Get-AuraHardwareContract {
  param([Parameter(Mandatory = $true)][string]$Environment)

  switch ($Environment) {
    "project_aura" {
      return [pscustomobject]@{
        Environment = "project_aura"
        HardwareTarget = "aura-aq-v1"
        HardwareProfile = "4_3"
        BuildIdSuffix = ""
        ArtifactSlug = "4_3"
      }
    }
    "project_aura_7" {
      return [pscustomobject]@{
        Environment = "project_aura_7"
        HardwareTarget = "aura-aq-7-v1"
        HardwareProfile = "7_dual_i2c_scl6"
        BuildIdSuffix = "7_dual_i2c_scl6"
        ArtifactSlug = "7"
      }
    }
    default {
      throw "Unsupported production environment: $Environment"
    }
  }
}

function Test-AuraStableVersion {
  param([string]$Version)

  return -not [string]::IsNullOrWhiteSpace($Version) -and
         $Version -match '^\d+(?:\.\d+)+$'
}

function Get-AuraEffectiveVersion {
  param(
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$BuildId
  )

  if ($Version -notmatch '^\d+\.\d+\.\d+(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?$') {
    throw "Invalid release version: $Version. Expected X.Y.Z or a safe prerelease suffix."
  }
  if (Test-AuraStableVersion -Version $Version) {
    return $Version
  }
  return "{0}-{1}" -f $Version, $BuildId
}

function Assert-AuraReleaseChannelVersion {
  param(
    [Parameter(Mandatory = $true)][string]$Channel,
    [Parameter(Mandatory = $true)][string]$Version
  )

  [void](Get-AuraEffectiveVersion -Version $Version -BuildId "validation")
  switch ($Channel) {
    "stable" {
      if (-not (Test-AuraStableVersion -Version $Version)) {
        throw "Stable releases require an exact X.Y.Z version without a prerelease suffix."
      }
    }
    "beta" {
      if (Test-AuraStableVersion -Version $Version) {
        throw "Beta releases require an explicit prerelease suffix, for example X.Y.Z-beta."
      }
    }
    default {
      throw "Unsupported release channel: $Channel"
    }
  }
}

function Get-AuraFileSha256 {
  param([Parameter(Mandatory = $true)][string]$Path)

  $stream = [System.IO.File]::OpenRead($Path)
  $sha256 = [System.Security.Cryptography.SHA256]::Create()
  try {
    return ([BitConverter]::ToString($sha256.ComputeHash($stream))).Replace("-", "").ToLowerInvariant()
  } finally {
    $sha256.Dispose()
    $stream.Dispose()
  }
}

function Assert-AuraPublishedAssetIsIdentical {
  param(
    [Parameter(Mandatory = $true)][object]$Asset,
    [Parameter(Mandatory = $true)][string]$LocalPath,
    [Parameter(Mandatory = $true)][string]$AssetName
  )

  $expectedSize = (Get-Item -LiteralPath $LocalPath).Length
  $expectedDigest = "sha256:" + (Get-AuraFileSha256 -Path $LocalPath)
  $sizeProperty = $Asset.PSObject.Properties["size"]
  $digestProperty = $Asset.PSObject.Properties["digest"]
  $actualSize = if ($null -eq $sizeProperty) { -1 } else { [long]$sizeProperty.Value }
  $actualDigest = if ($null -eq $digestProperty) { "" } else { [string]$digestProperty.Value }
  if ($actualSize -eq [long]$expectedSize -and
      $actualDigest.ToLowerInvariant() -eq $expectedDigest) {
    return
  }
  throw "GitHub release asset already exists and cannot be proven identical: $AssetName. Use a new version/tag; published assets are immutable."
}

function Read-AuraBuildIdentity {
  param(
    [Parameter(Mandatory = $true)][string]$IdentityPath,
    [Parameter(Mandatory = $true)][string]$Environment,
    [Parameter(Mandatory = $true)][string]$RepositoryRoot,
    [string]$ExpectedBuildId
  )

  if (-not (Test-Path -LiteralPath $IdentityPath)) {
    throw "Generated build identity is missing: $IdentityPath"
  }

  $contract = Get-AuraHardwareContract -Environment $Environment
  $identity = Get-Content -Raw -LiteralPath $IdentityPath | ConvertFrom-Json
  $requiredProperties = @(
    "schema",
    "environment",
    "source_commit",
    "build_id",
    "hardware_profile",
    "hardware_target",
    "partitions_file"
  )
  foreach ($propertyName in $requiredProperties) {
    $property = $identity.PSObject.Properties[$propertyName]
    if ($null -eq $property -or
        $null -eq $property.Value -or
        [string]::IsNullOrWhiteSpace([string]$property.Value)) {
      throw "Build identity is missing required property '$propertyName': $IdentityPath"
    }
  }
  if ($identity.schema -ne "project-aura.build-identity.v1") {
    throw "Unsupported build identity schema in $IdentityPath"
  }
  if ($identity.environment -ne $contract.Environment -or
      $identity.hardware_profile -ne $contract.HardwareProfile -or
      $identity.hardware_target -ne $contract.HardwareTarget) {
    throw "Build identity does not match the selected production environment $Environment."
  }
  if ([string]::IsNullOrWhiteSpace($identity.partitions_file) -or
      [System.IO.Path]::IsPathRooted($identity.partitions_file) -or
      $identity.partitions_file -match '(^|[\\/])\.\.([\\/]|$)') {
    throw "Build identity contains an unsafe partition file path."
  }

  Push-Location $RepositoryRoot
  try {
    $commit = (& git rev-parse HEAD 2>$null).Trim().ToLowerInvariant()
    if ($LASTEXITCODE -ne 0 -or $commit -notmatch "^[a-f0-9]{40}$") {
      throw "Could not resolve the source Git commit."
    }
    $statusLines = @(& git status --porcelain)
    $statusExitCode = $LASTEXITCODE
    if ($statusExitCode -ne 0) {
      throw "Could not inspect Git working tree state."
    }
    $dirty = -not [string]::IsNullOrWhiteSpace(($statusLines -join "`n"))
  } finally {
    Pop-Location
  }

  $identityCommit = ([string]$identity.source_commit).ToLowerInvariant()
  if ($identityCommit -notmatch "^[a-f0-9]{40}$") {
    throw "Build identity contains an invalid source commit."
  }
  if ($identityCommit -ne $commit) {
    throw "Build identity is stale: source commit does not match HEAD."
  }
  $expected = $commit.Substring(0, 7)
  if ($contract.BuildIdSuffix) {
    $expected = "{0}-{1}" -f $expected, $contract.BuildIdSuffix
  }
  if ($dirty) {
    $expected = "$expected-dirty"
  }
  if ($identity.build_id -ne $expected) {
    throw "Build identity is stale or belongs to different source inputs: expected=$expected actual=$($identity.build_id)"
  }
  if ($ExpectedBuildId -and $identity.build_id -ne $ExpectedBuildId) {
    throw "Requested BuildId does not match generated identity: requested=$ExpectedBuildId actual=$($identity.build_id)"
  }

  return [pscustomobject]@{
    Schema = $identity.schema
    Environment = $identity.environment
    SourceCommit = $identityCommit
    BuildId = $identity.build_id
    HardwareProfile = $identity.hardware_profile
    HardwareTarget = $identity.hardware_target
    ArtifactSlug = $contract.ArtifactSlug
    PartitionsFile = $identity.partitions_file
  }
}

function Get-AuraArtifactInputs {
  param(
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [Parameter(Mandatory = $true)][string]$BootApp0Path
  )

  return [ordered]@{
    "bootloader.bin" = Join-Path $BuildDirectory "bootloader.bin"
    "partitions.bin" = Join-Path $BuildDirectory "partitions.bin"
    "boot_app0.bin" = $BootApp0Path
    "firmware.bin" = Join-Path $BuildDirectory "firmware.bin"
    "littlefs.bin" = Join-Path $BuildDirectory "littlefs.bin"
  }
}

function Write-AuraReleaseArtifactStamp {
  param(
    [Parameter(Mandatory = $true)][string]$StampPath,
    [Parameter(Mandatory = $true)][object]$Identity,
    [Parameter(Mandatory = $true)][System.Collections.IDictionary]$ArtifactInputs
  )

  $files = @()
  foreach ($entry in $ArtifactInputs.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $entry.Value -PathType Leaf)) {
      throw "Missing release artifact: $($entry.Value)"
    }
    $item = Get-Item -LiteralPath $entry.Value
    if ($item.Length -lt 1) {
      throw "Release artifact is empty: $($entry.Value)"
    }
    $files += [ordered]@{
      file_name = [string]$entry.Key
      size_bytes = [long]$item.Length
      sha256 = Get-AuraFileSha256 -Path $entry.Value
    }
  }

  $stamp = [ordered]@{
    schema = "project-aura.release-artifacts.v1"
    environment = $Identity.Environment
    source_commit = $Identity.SourceCommit
    build_id = $Identity.BuildId
    hardware_profile = $Identity.HardwareProfile
    hardware_target = $Identity.HardwareTarget
    files = $files
  }
  $directory = Split-Path -Parent $StampPath
  New-Item -ItemType Directory -Force -Path $directory | Out-Null
  $stamp | ConvertTo-Json -Depth 6 | Set-Content -Encoding Ascii -LiteralPath $StampPath
}

function Read-AuraReleaseArtifactStamp {
  param(
    [Parameter(Mandatory = $true)][string]$StampPath,
    [Parameter(Mandatory = $true)][object]$Identity,
    [Parameter(Mandatory = $true)][System.Collections.IDictionary]$ArtifactInputs
  )

  if (-not (Test-Path -LiteralPath $StampPath -PathType Leaf)) {
    throw "Post-build release artifact stamp is missing: $StampPath"
  }
  $stamp = Get-Content -Raw -LiteralPath $StampPath | ConvertFrom-Json
  if ($stamp.schema -ne "project-aura.release-artifacts.v1" -or
      $stamp.environment -ne $Identity.Environment -or
      $stamp.source_commit -ne $Identity.SourceCommit -or
      $stamp.build_id -ne $Identity.BuildId -or
      $stamp.hardware_profile -ne $Identity.HardwareProfile -or
      $stamp.hardware_target -ne $Identity.HardwareTarget) {
    throw "Post-build release artifact stamp does not match the generated build identity."
  }
  if (-not ($stamp.files -is [System.Array]) -or
      $stamp.files.Count -ne $ArtifactInputs.Count) {
    throw "Post-build release artifact stamp has an unexpected file set."
  }

  $seen = @{}
  foreach ($file in $stamp.files) {
    $fileName = [string]$file.file_name
    if (-not $ArtifactInputs.Contains($fileName) -or $seen.ContainsKey($fileName)) {
      throw "Post-build release artifact stamp contains an unexpected or duplicate file: $fileName"
    }
    $seen[$fileName] = $true
    $path = [string]$ArtifactInputs[$fileName]
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
      throw "Stamped release artifact is missing: $path"
    }
    $item = Get-Item -LiteralPath $path
    $actualHash = Get-AuraFileSha256 -Path $path
    if ([long]$file.size_bytes -ne [long]$item.Length -or
        [string]$file.sha256 -ne $actualHash) {
      throw "Stamped release artifact changed after the successful build: $fileName"
    }
  }
  return $stamp
}
