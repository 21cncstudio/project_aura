param(
  [Parameter(Mandatory = $true)]
  [string]$Version,
  [ValidateSet("aura-aq-v1", "aura-aq-7-v1")]
  [string]$HardwareTarget = "aura-aq-v1",
  [string]$BuildId,
  [string]$Repo = "21cncstudio/project_aura",
  [string]$Tag,
  [string]$Title,
  [string]$AssetsDir,
  [string]$NotesPath,
  [string]$TargetCommitish,
  [int]$ApiTimeoutSec = 60,
  [int]$UploadTimeoutSec = 300,
  [int]$ConnectTimeoutSec = 15,
  [switch]$Draft,
  [switch]$SkipNotes,
  [switch]$ForceLegacyApi,
  [switch]$SkipReleaseUpdate,
  [switch]$PruneAssetsToList,
  [string[]]$AssetNames
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$script:LastGhExitCode = 0
. (Join-Path $PSScriptRoot "release_identity.ps1")

function Write-Step {
  param([string]$Message)
  Write-Host ("[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $Message)
}

[void](Get-AuraEffectiveVersion -Version $Version -BuildId "validation")
$isPrerelease = -not (Test-AuraStableVersion -Version $Version)
$requestedTag = $Tag
$assetDirectoryTag = "v$Version"
if ($Repo -notmatch '^[A-Za-z0-9.-]+/[A-Za-z0-9._-]+$') {
  throw "Invalid GitHub repository name: $Repo"
}

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$contract = Get-AuraHardwareContract -Environment $(
  if ($HardwareTarget -eq "aura-aq-v1") { "project_aura" } else { "project_aura_7" }
)
if ($contract.HardwareTarget -eq "aura-aq-7-v1") {
  throw "External GitHub publication for aura-aq-7-v1 is disabled until the 7-inch profile is explicitly qualified for release."
}
if (-not $AssetsDir) {
  $AssetsDir = Join-Path $root ("release-assets\{0}\{1}" -f $assetDirectoryTag, $HardwareTarget)
}
if (-not $NotesPath) {
  $NotesPath = Join-Path $root ("docs\releases\v{0}.md" -f $Version)
}

if (-not (Test-Path $AssetsDir)) {
  throw "Assets directory not found: $AssetsDir"
}

$releaseIdentityPath = Join-Path $AssetsDir "release-identity.json"
$releaseArtifactPath = Join-Path $AssetsDir "release-artifacts.json"
$hashListPath = Join-Path $AssetsDir "sha256sums.txt"
foreach ($requiredMetadata in @($releaseIdentityPath, $releaseArtifactPath, $hashListPath)) {
  if (-not (Test-Path -LiteralPath $requiredMetadata -PathType Leaf)) {
    throw "Required release metadata is missing: $requiredMetadata"
  }
}
$releaseIdentity = Get-Content -Raw -LiteralPath $releaseIdentityPath | ConvertFrom-Json
if ($releaseIdentity.schema -ne "project-aura.build-identity.v1" -or
    $releaseIdentity.environment -ne $contract.Environment -or
    $releaseIdentity.hardware_target -ne $contract.HardwareTarget -or
    $releaseIdentity.hardware_profile -ne $contract.HardwareProfile -or
    $releaseIdentity.artifact_slug -ne $contract.ArtifactSlug -or
    [string]$releaseIdentity.source_commit -notmatch '^[a-f0-9]{40}$') {
  throw "Release identity does not match selected hardware target $HardwareTarget."
}
$expectedBuildId = ([string]$releaseIdentity.source_commit).Substring(0, 7).ToLowerInvariant()
if ($contract.BuildIdSuffix) {
  $expectedBuildId = "{0}-{1}" -f $expectedBuildId, $contract.BuildIdSuffix
}
if ($releaseIdentity.build_id -ne $expectedBuildId) {
  throw "Only a clean, commit-bound build identity may be published: expected=$expectedBuildId actual=$($releaseIdentity.build_id)"
}
if ($BuildId -and $BuildId -ne $releaseIdentity.build_id) {
  throw "Requested BuildId does not match release identity: requested=$BuildId actual=$($releaseIdentity.build_id)"
}
if (-not $TargetCommitish) {
  $TargetCommitish = ([string]$releaseIdentity.source_commit).ToLowerInvariant()
} elseif ($TargetCommitish.ToLowerInvariant() -ne ([string]$releaseIdentity.source_commit).ToLowerInvariant()) {
  throw "TargetCommitish must equal the release source commit $($releaseIdentity.source_commit)."
}
$displayVersion = Get-AuraEffectiveVersion -Version $Version -BuildId $releaseIdentity.build_id
$expectedTag = "v$displayVersion"
if (-not $requestedTag) {
  $Tag = $expectedTag
} elseif ($requestedTag -ne $expectedTag) {
  throw "Release tag must match the effective firmware version exactly: expected=$expectedTag actual=$requestedTag"
} else {
  $Tag = $requestedTag
}
if (-not $Title) {
  $Title = "Project Aura v$displayVersion"
}

$manifest = Get-Content -Raw -LiteralPath (Join-Path $AssetsDir "manifest.json") | ConvertFrom-Json
$updateManifest = Get-Content -Raw -LiteralPath (Join-Path $AssetsDir "manifest-update.json") | ConvertFrom-Json
foreach ($releaseManifest in @($manifest, $updateManifest)) {
  if ($releaseManifest.version -ne $displayVersion -or
      $releaseManifest.hardware_target -ne $contract.HardwareTarget -or
      $releaseManifest.hardware_profile -ne $contract.HardwareProfile -or
      $releaseManifest.build_id -ne $releaseIdentity.build_id) {
    throw "Release manifest does not match the target-specific release identity."
  }
}

$releaseArtifacts = Get-Content -Raw -LiteralPath $releaseArtifactPath | ConvertFrom-Json
if ($releaseArtifacts.schema -ne "project-aura.release-artifacts.v1" -or
    $releaseArtifacts.environment -ne $contract.Environment -or
    $releaseArtifacts.source_commit -ne $releaseIdentity.source_commit -or
    $releaseArtifacts.build_id -ne $releaseIdentity.build_id -or
    $releaseArtifacts.hardware_profile -ne $contract.HardwareProfile -or
    $releaseArtifacts.hardware_target -ne $contract.HardwareTarget) {
  throw "Post-build artifact identity does not match the selected release identity."
}
$firmwareStamp = @($releaseArtifacts.files | Where-Object { $_.file_name -eq "firmware.bin" })
if ($firmwareStamp.Count -ne 1) {
  throw "Post-build artifact identity must contain exactly one firmware.bin record."
}

if (-not $AssetNames -or $AssetNames.Count -eq 0) {
  $AssetNames = @(
    ("project_aura_{0}_{1}_ota_firmware.bin" -f $contract.ArtifactSlug, $displayVersion)
  )
}

if ($PruneAssetsToList) {
  throw "-PruneAssetsToList is disabled for target-aware releases because a per-target invocation could delete the other profile's assets."
}

$declaredHashes = @{}
foreach ($line in Get-Content -LiteralPath $hashListPath) {
  if ($line -notmatch '^([a-f0-9]{64})  ([A-Za-z0-9][A-Za-z0-9._-]{0,127})$') {
    throw "Invalid sha256sums.txt entry: $line"
  }
  if ($declaredHashes.ContainsKey($Matches[2])) {
    throw "Duplicate sha256sums.txt entry: $($Matches[2])"
  }
  $declaredHashes[$Matches[2]] = $Matches[1]
}

$requestedAssetNames = @{}
foreach ($name in $AssetNames) {
  if ($name -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$') {
    throw "Unsafe asset file name: $name"
  }
  if ($requestedAssetNames.ContainsKey($name)) {
    throw "Duplicate requested asset file name: $name"
  }
  $requestedAssetNames[$name] = $true
  $path = Join-Path $AssetsDir $name
  if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
    throw "Missing asset file: $path"
  }
  if (-not $declaredHashes.ContainsKey($name)) {
    throw "Asset is not covered by sha256sums.txt: $name"
  }
  $actualHash = Get-AuraFileSha256 -Path $path
  if ($actualHash -ne $declaredHashes[$name]) {
    throw "Asset hash does not match sha256sums.txt: $name"
  }
  if ($name -eq ("project_aura_{0}_{1}_ota_firmware.bin" -f $contract.ArtifactSlug, $displayVersion) -and
      ($actualHash -ne $firmwareStamp[0].sha256 -or
       (Get-Item -LiteralPath $path).Length -ne [long]$firmwareStamp[0].size_bytes)) {
    throw "OTA asset does not match the post-build firmware.bin identity."
  }
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
  throw "git is required but not found."
}

function Get-GitHubCredentials {
  $request = "protocol=https`nhost=github.com`nusername=21cncstudio`n`n"
  $oldGcmInteractive = $env:GCM_INTERACTIVE
  try {
    $env:GCM_INTERACTIVE = "never"
    $raw = $request | git credential fill 2>$null
    $credentialExitCode = $LASTEXITCODE
  } finally {
    if ($null -eq $oldGcmInteractive) {
      Remove-Item Env:\GCM_INTERACTIVE -ErrorAction SilentlyContinue
    } else {
      $env:GCM_INTERACTIVE = $oldGcmInteractive
    }
  }
  if ($credentialExitCode -ne 0 -or [string]::IsNullOrWhiteSpace($raw)) {
    throw "Unable to read GitHub credentials from git credential helper."
  }

  $map = @{}
  foreach ($line in ($raw -split "`n")) {
    if ($line -match "=") {
      $k, $v = $line.Split("=", 2)
      $map[$k.Trim()] = $v.Trim()
    }
  }

  if (-not $map.ContainsKey("username") -or -not $map.ContainsKey("password")) {
    throw "Credential helper returned incomplete GitHub credentials."
  }

  return [pscustomobject]@{
    Username = $map["username"]
    Password = $map["password"]
    BasicAuth = [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes(("{0}:{1}" -f $map["username"], $map["password"])))
  }
}

function Resolve-GhCommand {
  $gh = Get-Command gh -ErrorAction SilentlyContinue
  if ($gh) {
    return $gh.Source
  }

  $candidates = @(
    (Join-Path ${env:ProgramFiles} "GitHub CLI\gh.exe"),
    (Join-Path ${env:LOCALAPPDATA} "Programs\GitHub CLI\gh.exe")
  ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }

  foreach ($candidate in $candidates) {
    if (Test-Path $candidate) {
      return $candidate
    }
  }

  return $null
}

function Invoke-Gh {
  param(
    [string]$GhExe,
    [string]$Token,
    [string[]]$CommandArgs,
    [switch]$AllowFailure
  )

  $oldGhToken = $env:GH_TOKEN
  $oldGhHost = $env:GH_HOST
  $oldErrorActionPreference = $ErrorActionPreference
  try {
    $env:GH_TOKEN = $Token
    $env:GH_HOST = "github.com"
    $ErrorActionPreference = "Continue"
    $output = & $GhExe @CommandArgs 2>&1
    $exitCode = $LASTEXITCODE
  } finally {
    $ErrorActionPreference = $oldErrorActionPreference
    if ($null -eq $oldGhToken) {
      Remove-Item Env:\GH_TOKEN -ErrorAction SilentlyContinue
    } else {
      $env:GH_TOKEN = $oldGhToken
    }

    if ($null -eq $oldGhHost) {
      Remove-Item Env:\GH_HOST -ErrorAction SilentlyContinue
    } else {
      $env:GH_HOST = $oldGhHost
    }
  }

  if ($exitCode -ne 0 -and -not $AllowFailure) {
    $details = (($output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine).Trim()
    if ([string]::IsNullOrWhiteSpace($details)) {
      $details = "gh exited with code $exitCode"
    }
    throw $details
  }

  $script:LastGhExitCode = $exitCode
  return ,$output
}

function Invoke-GhJson {
  param(
    [string]$GhExe,
    [string]$Token,
    [string[]]$CommandArgs,
    [switch]$AllowFailure
  )

  $output = Invoke-Gh -GhExe $GhExe -Token $Token -CommandArgs $CommandArgs -AllowFailure:$AllowFailure
  $text = (($output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine).Trim()
  if ([string]::IsNullOrWhiteSpace($text)) {
    return $null
  }
  $trimmed = $text.TrimStart()
  if (-not ($trimmed.StartsWith("{") -or $trimmed.StartsWith("["))) {
    if ($AllowFailure) {
      return $null
    }
    throw $text
  }
  return ($text | ConvertFrom-Json)
}

function Invoke-GhReleaseApiJson {
  param(
    [string]$GhExe,
    [string]$Token,
    [string]$Method,
    [string]$Path,
    [string]$InputPath,
    [switch]$AllowFailure
  )

  $args = @(
    "api",
    "--method", $Method,
    $Path,
    "-H", "Accept: application/vnd.github+json",
    "-H", "X-GitHub-Api-Version: 2022-11-28"
  )

  if ($InputPath) {
    $args += @("--input", $InputPath)
  }

  return Invoke-GhJson -GhExe $GhExe -Token $Token -CommandArgs $args -AllowFailure:$AllowFailure
}

function Get-ReleaseByTagViaGh {
  param(
    [string]$GhExe,
    [string]$Token,
    [string]$RepoName,
    [string]$ReleaseTag
  )

  $encodedTag = [uri]::EscapeDataString($ReleaseTag)
  $output = Invoke-Gh -GhExe $GhExe -Token $Token -CommandArgs @(
    "api",
    "--method", "GET",
    ("repos/{0}/releases/tags/{1}" -f $RepoName, $encodedTag),
    "-H", "Accept: application/vnd.github+json",
    "-H", "X-GitHub-Api-Version: 2022-11-28"
  ) -AllowFailure

  if ($script:LastGhExitCode -ne 0) {
    $details = (($output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine).Trim()
    if ($details -notmatch '(?i)(HTTP\s+404|Not Found)') {
      throw "Unable to check GitHub release tag '$ReleaseTag': $details"
    }
    $output = Invoke-Gh -GhExe $GhExe -Token $Token -CommandArgs @(
      "api",
      "--method", "GET",
      "--paginate",
      "--slurp",
      ("repos/{0}/releases?per_page=100" -f $RepoName),
      "-H", "Accept: application/vnd.github+json",
      "-H", "X-GitHub-Api-Version: 2022-11-28"
    )
    $listText = (($output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine).Trim()
    if ([string]::IsNullOrWhiteSpace($listText)) {
      throw "GitHub returned an empty release-list response."
    }
    $pages = $listText | ConvertFrom-Json
    $matches = @()
    foreach ($page in @($pages)) {
      $matches += @($page | Where-Object { [string]$_.tag_name -eq $ReleaseTag })
    }
    if ($matches.Count -gt 1) {
      throw "GitHub contains multiple releases or drafts for tag '$ReleaseTag'. Resolve them manually."
    }
    if ($matches.Count -eq 0) {
      return $null
    }
    return $matches[0]
  }

  $text = (($output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine).Trim()
  if ([string]::IsNullOrWhiteSpace($text) -or -not $text.TrimStart().StartsWith("{")) {
    throw "GitHub returned an invalid release response for '$ReleaseTag'."
  }
  $release = $text | ConvertFrom-Json
  if ([string]$release.tag_name -ne $ReleaseTag) {
    throw "GitHub returned a release for an unexpected tag."
  }
  return $release
}

function Get-TagSourceCommitViaGh {
  param(
    [string]$GhExe,
    [string]$Token,
    [string]$RepoName,
    [string]$ReleaseTag
  )

  $encodedTag = [uri]::EscapeDataString($ReleaseTag)
  $output = Invoke-Gh -GhExe $GhExe -Token $Token -CommandArgs @(
    "api",
    "--method", "GET",
    ("repos/{0}/git/ref/tags/{1}" -f $RepoName, $encodedTag),
    "-H", "Accept: application/vnd.github+json",
    "-H", "X-GitHub-Api-Version: 2022-11-28"
  ) -AllowFailure
  $text = (($output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine).Trim()
  if ($script:LastGhExitCode -ne 0) {
    if ($text -match '(?i)(HTTP\s+404|Not Found)') {
      return $null
    }
    throw "Unable to resolve GitHub tag '$ReleaseTag': $text"
  }

  if ([string]::IsNullOrWhiteSpace($text) -or -not $text.TrimStart().StartsWith("{")) {
    throw "GitHub returned an invalid tag response for '$ReleaseTag'."
  }
  $reference = $text | ConvertFrom-Json
  $objectType = [string]$reference.object.type
  $objectSha = ([string]$reference.object.sha).ToLowerInvariant()
  for ($depth = 0; $depth -lt 8 -and $objectType -eq "tag"; $depth++) {
    if ($objectSha -notmatch '^[a-f0-9]{40}$') {
      throw "GitHub tag '$ReleaseTag' contains an invalid object SHA."
    }
    $tagObject = Invoke-GhReleaseApiJson `
      -GhExe $GhExe `
      -Token $Token `
      -Method "GET" `
      -Path ("repos/{0}/git/tags/{1}" -f $RepoName, $objectSha)
    $objectType = [string]$tagObject.object.type
    $objectSha = ([string]$tagObject.object.sha).ToLowerInvariant()
  }

  if ($objectType -ne "commit" -or $objectSha -notmatch '^[a-f0-9]{40}$') {
    throw "GitHub tag '$ReleaseTag' does not resolve to a commit."
  }
  return $objectSha
}

function Get-ReleaseAssetsViaGh {
  param(
    [string]$GhExe,
    [string]$Token,
    [string]$RepoName,
    [int]$ReleaseId
  )

  $output = Invoke-Gh -GhExe $GhExe -Token $Token -CommandArgs @(
    "api",
    "--method", "GET",
    "--paginate",
    "--slurp",
    ("repos/{0}/releases/{1}/assets?per_page=100" -f $RepoName, $ReleaseId),
    "-H", "Accept: application/vnd.github+json",
    "-H", "X-GitHub-Api-Version: 2022-11-28"
  )
  $text = (($output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine).Trim()
  if ([string]::IsNullOrWhiteSpace($text)) {
    throw "GitHub returned an empty asset-list response for release id=$ReleaseId."
  }
  $pages = $text | ConvertFrom-Json
  if ($null -eq $pages) {
    return @()
  }
  $assets = @()
  foreach ($page in @($pages)) {
    $assets += @($page)
  }
  return $assets
}

function Invoke-GhReleaseUpload {
  param(
    [string]$GhExe,
    [string]$Token,
    [string]$RepoName,
    [string]$ReleaseTag,
    [string]$FilePath
  )

  [void](Invoke-Gh -GhExe $GhExe -Token $Token -CommandArgs @(
    "release", "upload", $ReleaseTag, $FilePath,
    "--repo", $RepoName
  ))
}

function Get-AuraPendingAssetNames {
  param(
    [AllowEmptyCollection()][object[]]$ExistingAssets,
    [Parameter(Mandatory = $true)][string[]]$RequestedAssetNames,
    [Parameter(Mandatory = $true)][string]$LocalAssetsDirectory
  )

  $pending = @()
  foreach ($name in $RequestedAssetNames) {
    $path = Join-Path $LocalAssetsDirectory $name
    $sameName = @($ExistingAssets | Where-Object { $_.name -eq $name })
    if ($sameName.Count -gt 1) {
      throw "GitHub release contains duplicate assets named $name. Resolve the release manually."
    }
    if ($sameName.Count -eq 1) {
      Assert-AuraPublishedAssetIsIdentical -Asset $sameName[0] -LocalPath $path -AssetName $name
      Write-Host "Already published and identical: $name"
    } else {
      $pending += $name
    }
  }

  return $pending
}

function Get-ObjectPropertyValue {
  param(
    [object]$InputObject,
    [string]$PropertyName
  )

  if ($null -eq $InputObject) {
    return $null
  }

  $prop = $InputObject.PSObject.Properties[$PropertyName]
  if ($null -eq $prop) {
    return $null
  }

  return $prop.Value
}

function Get-AuraReleaseId {
  param([Parameter(Mandatory = $true)][object]$Release)

  $releaseId = Get-ObjectPropertyValue -InputObject $Release -PropertyName "id"
  if ($null -eq $releaseId) {
    $releaseId = Get-ObjectPropertyValue -InputObject $Release -PropertyName "databaseId"
  }
  if ($null -eq $releaseId) {
    throw "Unable to determine release id for GitHub release operation."
  }
  return [int]$releaseId
}

function Get-AuraReleaseUrl {
  param([Parameter(Mandatory = $true)][object]$Release)

  $url = Get-ObjectPropertyValue -InputObject $Release -PropertyName "html_url"
  if ([string]::IsNullOrWhiteSpace([string]$url)) {
    $url = Get-ObjectPropertyValue -InputObject $Release -PropertyName "url"
  }
  return [string]$url
}

function Get-TagSourceCommitViaRest {
  param(
    [string]$RepoName,
    [string]$ReleaseTag,
    [hashtable]$RequestHeaders,
    [int]$TimeoutSec
  )

  $encodedTag = [uri]::EscapeDataString($ReleaseTag)
  $referenceUrl = "https://api.github.com/repos/$RepoName/git/ref/tags/$encodedTag"
  try {
    $reference = Invoke-RestMethod -Method Get -Uri $referenceUrl -Headers $RequestHeaders -TimeoutSec $TimeoutSec
  } catch {
    $status = $null
    if ($_.Exception.Response) {
      try { $status = [int]$_.Exception.Response.StatusCode } catch {}
    }
    if ($status -eq 404) {
      return $null
    }
    throw
  }

  $referenceObject = Get-ObjectPropertyValue -InputObject $reference -PropertyName "object"
  $objectType = [string](Get-ObjectPropertyValue -InputObject $referenceObject -PropertyName "type")
  $objectSha = ([string](Get-ObjectPropertyValue -InputObject $referenceObject -PropertyName "sha")).ToLowerInvariant()
  for ($depth = 0; $depth -lt 8 -and $objectType -eq "tag"; $depth++) {
    if ($objectSha -notmatch '^[a-f0-9]{40}$') {
      throw "GitHub tag '$ReleaseTag' contains an invalid object SHA."
    }
    $tagObject = Invoke-RestMethod `
      -Method Get `
      -Uri "https://api.github.com/repos/$RepoName/git/tags/$objectSha" `
      -Headers $RequestHeaders `
      -TimeoutSec $TimeoutSec
    $nestedObject = Get-ObjectPropertyValue -InputObject $tagObject -PropertyName "object"
    $objectType = [string](Get-ObjectPropertyValue -InputObject $nestedObject -PropertyName "type")
    $objectSha = ([string](Get-ObjectPropertyValue -InputObject $nestedObject -PropertyName "sha")).ToLowerInvariant()
  }

  if ($objectType -ne "commit" -or $objectSha -notmatch '^[a-f0-9]{40}$') {
    throw "GitHub tag '$ReleaseTag' does not resolve to a commit."
  }
  return $objectSha
}

function Get-ReleaseByTagViaRest {
  param(
    [string]$RepoName,
    [string]$ReleaseTag,
    [hashtable]$RequestHeaders,
    [int]$TimeoutSec
  )

  $encodedTag = [uri]::EscapeDataString($ReleaseTag)
  try {
    return Invoke-RestMethod `
      -Method Get `
      -Uri "https://api.github.com/repos/$RepoName/releases/tags/$encodedTag" `
      -Headers $RequestHeaders `
      -TimeoutSec $TimeoutSec
  } catch {
    $status = $null
    if ($_.Exception.Response) {
      try { $status = [int]$_.Exception.Response.StatusCode } catch {}
    }
    if ($status -ne 404) {
      throw
    }
  }

  $matches = @()
  for ($page = 1; $page -le 100; $page++) {
    $url = "https://api.github.com/repos/$RepoName/releases?per_page=100&page=$page"
    $response = Invoke-RestMethod -Method Get -Uri $url -Headers $RequestHeaders -TimeoutSec $TimeoutSec
    $pageReleases = if ($null -eq $response) { @() } else { @($response) }
    $matches += @($pageReleases | Where-Object { [string]$_.tag_name -eq $ReleaseTag })
    if ($pageReleases.Count -lt 100) {
      break
    }
    if ($page -eq 100) {
      throw "GitHub release lookup exceeded 100 pages."
    }
  }

  if ($matches.Count -gt 1) {
    throw "GitHub contains multiple releases or drafts for tag '$ReleaseTag'. Resolve them manually."
  }
  if ($matches.Count -eq 1) {
    return $matches[0]
  }
  return $null
}

function Get-ReleaseAssetsViaRest {
  param(
    [string]$RepoName,
    [int]$ReleaseId,
    [hashtable]$RequestHeaders,
    [int]$TimeoutSec
  )

  $assets = @()
  for ($page = 1; $page -le 100; $page++) {
    $url = "https://api.github.com/repos/$RepoName/releases/$ReleaseId/assets?per_page=100&page=$page"
    $response = Invoke-RestMethod -Method Get -Uri $url -Headers $RequestHeaders -TimeoutSec $TimeoutSec
    $pageAssets = if ($null -eq $response) { @() } else { @($response) }
    $assets += $pageAssets
    if ($pageAssets.Count -lt 100) {
      return $assets
    }
  }

  throw "GitHub asset preflight exceeded 100 pages for release id=$ReleaseId."
}

function Invoke-CurlJson {
  param(
    [string]$Method,
    [string]$Url,
    [string]$BasicAuth,
    [object]$BodyObj,
    [int]$TimeoutSec = 60
  )

  $curlArgs = @(
    "-sS",
    "--fail-with-body",
    "--http1.1",
    "--connect-timeout", "$ConnectTimeoutSec",
    "--max-time", "$TimeoutSec",
    "-X", $Method,
    $Url,
    "-H", ("Authorization: Basic {0}" -f $BasicAuth),
    "-H", "Accept: application/vnd.github+json",
    "-H", "X-GitHub-Api-Version: 2022-11-28",
    "-H", "User-Agent: project-aura-release-script",
    "-H", "Expect:"
  )

  $tmp = $null
  try {
    if ($null -ne $BodyObj) {
      $tmp = Join-Path $env:TEMP ("aura_release_payload_{0}.json" -f ([Guid]::NewGuid().ToString("N")))
      $json = $BodyObj | ConvertTo-Json -Depth 20
      Set-Content -LiteralPath $tmp -Value $json -Encoding UTF8
      $curlArgs += @("-H", "Content-Type: application/json", "--data-binary", ("@{0}" -f $tmp))
    }

    $response = & curl.exe @curlArgs
    if ($LASTEXITCODE -ne 0) {
      throw "curl failed with exit code $LASTEXITCODE"
    }
  } finally {
    if ($tmp -and (Test-Path -LiteralPath $tmp)) {
      Remove-Item -Force -LiteralPath $tmp -ErrorAction SilentlyContinue
    }
  }

  if ([string]::IsNullOrWhiteSpace($response)) {
    return $null
  }
  return ($response | ConvertFrom-Json)
}

function Invoke-CurlUpload {
  param(
    [string]$Url,
    [string]$FilePath,
    [string]$BasicAuth,
    [int]$TimeoutSec = 300
  )

  $curlArgs = @(
    "-sS",
    "--fail-with-body",
    "--http1.1",
    "--connect-timeout", "$ConnectTimeoutSec",
    "--max-time", "$TimeoutSec",
    "-X", "POST",
    $Url,
    "-H", ("Authorization: Basic {0}" -f $BasicAuth),
    "-H", "Accept: application/vnd.github+json",
    "-H", "X-GitHub-Api-Version: 2022-11-28",
    "-H", "User-Agent: project-aura-release-script",
    "-H", "Content-Type: application/octet-stream",
    "-H", "Expect:",
    "--data-binary", ("@{0}" -f $FilePath)
  )

  $response = & curl.exe @curlArgs
  if ($LASTEXITCODE -ne 0) {
    throw "Upload failed for $FilePath (exit code $LASTEXITCODE)"
  }
  return ($response | ConvertFrom-Json)
}

Write-Step "Resolving GitHub credentials"
$credentials = Get-GitHubCredentials
$basicAuth = $credentials.BasicAuth
$headers = @{
  Authorization = "Basic $basicAuth"
  Accept = "application/vnd.github+json"
  "X-GitHub-Api-Version" = "2022-11-28"
  "User-Agent" = "project-aura-release-script"
}
$ghCommand = $null
if (-not $ForceLegacyApi) {
  $ghCommand = Resolve-GhCommand
}
if (-not $ghCommand -and -not (Get-Command curl.exe -ErrorAction SilentlyContinue)) {
  throw "curl.exe is required for the legacy GitHub API path but was not found."
}

Write-Step "Checking GitHub repository identity: $Repo"
if ($ghCommand) {
  $repository = Invoke-GhReleaseApiJson `
    -GhExe $ghCommand `
    -Token $credentials.Password `
    -Method "GET" `
    -Path ("repos/{0}" -f $Repo)
} else {
  $repository = Invoke-RestMethod `
    -Method Get `
    -Uri "https://api.github.com/repos/$Repo" `
    -Headers $headers `
    -TimeoutSec $ApiTimeoutSec
}
$repositoryFullName = [string](Get-ObjectPropertyValue -InputObject $repository -PropertyName "full_name")
if ($repositoryFullName -ine $Repo) {
  throw "GitHub repository identity mismatch: expected=$Repo actual=$repositoryFullName"
}

$release = $null
$remoteTagCommit = $null
Write-Step "Checking existing release by tag: $Tag"
if ($ghCommand) {
  Write-Step "Using GitHub CLI path: $ghCommand"
  $release = Get-ReleaseByTagViaGh -GhExe $ghCommand -Token $credentials.Password -RepoName $Repo -ReleaseTag $Tag
  Write-Step "Checking source commit for tag: $Tag"
  $remoteTagCommit = Get-TagSourceCommitViaGh -GhExe $ghCommand -Token $credentials.Password -RepoName $Repo -ReleaseTag $Tag
} else {
  $release = Get-ReleaseByTagViaRest `
    -RepoName $Repo `
    -ReleaseTag $Tag `
    -RequestHeaders $headers `
    -TimeoutSec $ApiTimeoutSec
  Write-Step "Checking source commit for tag: $Tag"
  $remoteTagCommit = Get-TagSourceCommitViaRest `
    -RepoName $Repo `
    -ReleaseTag $Tag `
    -RequestHeaders $headers `
    -TimeoutSec $ApiTimeoutSec
}

$existingDraftBoundToSource = $false
if ($release) {
  $releaseTagName = Get-ObjectPropertyValue -InputObject $release -PropertyName "tagName"
  if ([string]::IsNullOrWhiteSpace([string]$releaseTagName)) {
    $releaseTagName = Get-ObjectPropertyValue -InputObject $release -PropertyName "tag_name"
  }
  if ([string]$releaseTagName -ne $Tag) {
    throw "GitHub returned a release for an unexpected tag."
  }
  [void](Get-AuraReleaseId -Release $release)
  if (-not $remoteTagCommit) {
    $draftValue = Get-ObjectPropertyValue -InputObject $release -PropertyName "draft"
    if ($null -eq $draftValue) {
      $draftValue = Get-ObjectPropertyValue -InputObject $release -PropertyName "isDraft"
    }
    $draftTargetCommitish = [string](Get-ObjectPropertyValue -InputObject $release -PropertyName "target_commitish")
    $existingDraftBoundToSource = [bool]$draftValue -and
      $draftTargetCommitish.ToLowerInvariant() -eq ([string]$releaseIdentity.source_commit).ToLowerInvariant()
    if (-not $existingDraftBoundToSource) {
      throw "GitHub release $Tag exists without a resolvable tag and is not a source-bound draft."
    }
  }
}
if ($remoteTagCommit -and $remoteTagCommit -ne ([string]$releaseIdentity.source_commit).ToLowerInvariant()) {
  throw "GitHub tag $Tag points to $remoteTagCommit, not release source commit $($releaseIdentity.source_commit). Published tags are immutable."
}

$notesText = $null
if (-not $SkipReleaseUpdate -and -not $SkipNotes -and (Test-Path $NotesPath)) {
  Write-Step "Loading release notes: $NotesPath"
  $notesText = Get-Content -Raw -Path $NotesPath
}

if ($ghCommand) {
  $payloadTmp = Join-Path $env:TEMP ("aura_release_payload_{0}.json" -f ([Guid]::NewGuid().ToString("N")))
  $isNewRelease = $null -eq $release
  try {
    if ($isNewRelease) {
      $createPayload = @{
        tag_name = $Tag
        target_commitish = $TargetCommitish
        name = $Title
        draft = $true
        prerelease = [bool]$isPrerelease
        generate_release_notes = $false
      }
      if ($notesText) {
        $createPayload["body"] = $notesText
      }
      $createPayload | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $payloadTmp -Encoding UTF8
      Write-Step "Creating draft release $Tag (target: $TargetCommitish)"
      $release = Invoke-GhReleaseApiJson `
        -GhExe $ghCommand `
        -Token $credentials.Password `
        -Method "POST" `
        -Path ("repos/{0}/releases" -f $Repo) `
        -InputPath $payloadTmp
      Write-Host "Created draft release: $(Get-AuraReleaseUrl -Release $release)"
    }

    $releaseId = Get-AuraReleaseId -Release $release
    Write-Step "Preflighting existing release assets"
    $existing = Get-ReleaseAssetsViaGh `
      -GhExe $ghCommand `
      -Token $credentials.Password `
      -RepoName $Repo `
      -ReleaseId $releaseId
    $pendingAssetNames = @(Get-AuraPendingAssetNames `
      -ExistingAssets @($existing) `
      -RequestedAssetNames $AssetNames `
      -LocalAssetsDirectory $AssetsDir)

    $tagCommitBeforeUpload = Get-TagSourceCommitViaGh `
      -GhExe $ghCommand `
      -Token $credentials.Password `
      -RepoName $Repo `
      -ReleaseTag $Tag
    if (-not $isNewRelease -and -not $existingDraftBoundToSource -and -not $tagCommitBeforeUpload) {
      throw "GitHub release $Tag exists, but its tag disappeared before asset upload."
    }
    if ($tagCommitBeforeUpload -and $tagCommitBeforeUpload -ne ([string]$releaseIdentity.source_commit).ToLowerInvariant()) {
      throw "GitHub tag $Tag changed before asset upload. Published tags are immutable."
    }

    foreach ($name in $pendingAssetNames) {
      $path = Join-Path $AssetsDir $name
      Write-Step "Uploading asset: $name"
      Invoke-GhReleaseUpload `
        -GhExe $ghCommand `
        -Token $credentials.Password `
        -RepoName $Repo `
        -ReleaseTag $Tag `
        -FilePath $path
      $fileInfo = Get-Item -LiteralPath $path
      Write-Host ("Uploaded: {0} ({1} bytes)" -f $name, $fileInfo.Length)
    }

    $shouldFinalizeMetadata = $isNewRelease -or -not $SkipReleaseUpdate
    if ($shouldFinalizeMetadata) {
      $tagCommitBeforeFinalize = Get-TagSourceCommitViaGh `
        -GhExe $ghCommand `
        -Token $credentials.Password `
        -RepoName $Repo `
        -ReleaseTag $Tag
      if (-not $isNewRelease -and -not $existingDraftBoundToSource -and -not $tagCommitBeforeFinalize) {
        throw "GitHub release $Tag exists, but its tag disappeared before metadata update."
      }
      if ($tagCommitBeforeFinalize -and $tagCommitBeforeFinalize -ne ([string]$releaseIdentity.source_commit).ToLowerInvariant()) {
        throw "GitHub tag $Tag changed before metadata update. Published tags are immutable."
      }

      $finalPayload = @{
        name = $Title
        draft = [bool]$Draft
        prerelease = [bool]$isPrerelease
      }
      if ($notesText) {
        $finalPayload["body"] = $notesText
      }
      $finalPayload | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $payloadTmp -Encoding UTF8
      Write-Step "Finalizing release metadata id=$releaseId"
      $release = Invoke-GhReleaseApiJson `
        -GhExe $ghCommand `
        -Token $credentials.Password `
        -Method "PATCH" `
        -Path ("repos/{0}/releases/{1}" -f $Repo, $releaseId) `
        -InputPath $payloadTmp
      Write-Host "Finalized release: $(Get-AuraReleaseUrl -Release $release)"
    } else {
      Write-Step "Skipping release metadata update for existing release id=$releaseId"
    }

    Write-Host ""
    Write-Host "Release ready: $(Get-AuraReleaseUrl -Release $release)"
    return
  } finally {
    if (Test-Path -LiteralPath $payloadTmp) {
      Remove-Item -Force -LiteralPath $payloadTmp -ErrorAction SilentlyContinue
    }
  }
}

$isNewRelease = $null -eq $release
if ($isNewRelease) {
  Write-Step "Creating draft release $Tag (target: $TargetCommitish)"
  $createPayload = @{
    tag_name = $Tag
    target_commitish = $TargetCommitish
    name = $Title
    draft = $true
    prerelease = [bool]$isPrerelease
    generate_release_notes = $false
  }
  if ($notesText) {
    $createPayload["body"] = $notesText
  }
  $release = Invoke-CurlJson -Method "POST" -Url ("https://api.github.com/repos/$Repo/releases") -BasicAuth $basicAuth -BodyObj $createPayload -TimeoutSec $ApiTimeoutSec
  Write-Host "Created draft release: $(Get-AuraReleaseUrl -Release $release)"
}

$releaseId = Get-AuraReleaseId -Release $release
Write-Step "Preflighting existing release assets"
$existing = Get-ReleaseAssetsViaRest `
  -RepoName $Repo `
  -ReleaseId $releaseId `
  -RequestHeaders $headers `
  -TimeoutSec $ApiTimeoutSec
$pendingAssetNames = @(Get-AuraPendingAssetNames `
  -ExistingAssets @($existing) `
  -RequestedAssetNames $AssetNames `
  -LocalAssetsDirectory $AssetsDir)

$tagCommitBeforeUpload = Get-TagSourceCommitViaRest `
  -RepoName $Repo `
  -ReleaseTag $Tag `
  -RequestHeaders $headers `
  -TimeoutSec $ApiTimeoutSec
if (-not $isNewRelease -and -not $existingDraftBoundToSource -and -not $tagCommitBeforeUpload) {
  throw "GitHub release $Tag exists, but its tag disappeared before asset upload."
}
if ($tagCommitBeforeUpload -and $tagCommitBeforeUpload -ne ([string]$releaseIdentity.source_commit).ToLowerInvariant()) {
  throw "GitHub tag $Tag changed before asset upload. Published tags are immutable."
}

foreach ($name in $pendingAssetNames) {
  $path = Join-Path $AssetsDir $name
  Write-Step "Uploading asset: $name"
  $uploadUrl = "https://uploads.github.com/repos/$Repo/releases/$releaseId/assets?name=$([uri]::EscapeDataString($name))"
  $uploaded = $null
  $maxRetries = 3
  for ($attempt = 1; $attempt -le $maxRetries; $attempt++) {
    try {
      Write-Step ("Attempt {0}/{1}: {2}" -f $attempt, $maxRetries, $name)
      $uploaded = Invoke-CurlUpload -Url $uploadUrl -FilePath $path -BasicAuth $basicAuth -TimeoutSec $UploadTimeoutSec
      break
    } catch {
      if ($attempt -eq $maxRetries) {
        throw
      }
      Start-Sleep -Seconds (2 * $attempt)
    }
  }
  Write-Host ("Uploaded: {0} ({1} bytes)" -f $uploaded.name, $uploaded.size)
}

$shouldFinalizeMetadata = $isNewRelease -or -not $SkipReleaseUpdate
if ($shouldFinalizeMetadata) {
  $tagCommitBeforeFinalize = Get-TagSourceCommitViaRest `
    -RepoName $Repo `
    -ReleaseTag $Tag `
    -RequestHeaders $headers `
    -TimeoutSec $ApiTimeoutSec
  if (-not $isNewRelease -and -not $existingDraftBoundToSource -and -not $tagCommitBeforeFinalize) {
    throw "GitHub release $Tag exists, but its tag disappeared before metadata update."
  }
  if ($tagCommitBeforeFinalize -and $tagCommitBeforeFinalize -ne ([string]$releaseIdentity.source_commit).ToLowerInvariant()) {
    throw "GitHub tag $Tag changed before metadata update. Published tags are immutable."
  }

  $patchPayload = @{
    name = $Title
    draft = [bool]$Draft
    prerelease = [bool]$isPrerelease
  }
  if ($notesText) {
    $patchPayload["body"] = $notesText
  }
  Write-Step "Finalizing release metadata id=$releaseId"
  $release = Invoke-CurlJson `
    -Method "PATCH" `
    -Url ("https://api.github.com/repos/$Repo/releases/$releaseId") `
    -BasicAuth $basicAuth `
    -BodyObj $patchPayload `
    -TimeoutSec $ApiTimeoutSec
  Write-Host "Finalized release: $(Get-AuraReleaseUrl -Release $release)"
} else {
  Write-Step "Skipping release metadata update for existing release id=$releaseId"
}

Write-Host ""
Write-Host "Release ready: $(Get-AuraReleaseUrl -Release $release)"
