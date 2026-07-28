param(
  [string]$KeyDirectory = (Join-Path $env:USERPROFILE ".project_aura\signing"),
  [string]$NodePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Security

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
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

if (Test-Path $privatePath) {
  throw "A protected installer release key already exists at $privatePath"
}

New-Item -ItemType Directory -Force -Path $KeyDirectory | Out-Null
$tempPrivate = Join-Path ([System.IO.Path]::GetTempPath()) ("aura-release-" + [guid]::NewGuid() + ".pem")
$tempPublic = Join-Path ([System.IO.Path]::GetTempPath()) ("aura-release-" + [guid]::NewGuid() + ".pub.pem")

try {
  & $node (Join-Path $PSScriptRoot "generate_installer_release_key.mjs") --private-out $tempPrivate --public-out $tempPublic
  if ($LASTEXITCODE -ne 0) {
    throw "Node.js failed to generate the Ed25519 key pair."
  }

  $privateBytes = [System.IO.File]::ReadAllBytes($tempPrivate)
  $protected = [System.Security.Cryptography.ProtectedData]::Protect(
    $privateBytes,
    $null,
    [System.Security.Cryptography.DataProtectionScope]::CurrentUser
  )
  [System.IO.File]::WriteAllText($privatePath, [Convert]::ToBase64String($protected))

  Copy-Item -LiteralPath $tempPublic -Destination $publicPath
  $publicDer = & $node -e "const {createPublicKey,createHash}=require('node:crypto');const fs=require('node:fs');const k=createPublicKey(fs.readFileSync(process.argv[1]));process.stdout.write(createHash('sha256').update(k.export({type:'spki',format:'der'})).digest('hex').slice(0,16))" $publicPath
  if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($publicDer)) {
    throw "Could not derive the public key ID."
  }
  $keyId = "aura-installer-ed25519-" + $publicDer.Trim()
  [ordered]@{
    key_id = $keyId
    algorithm = "ed25519"
    created_at = [DateTime]::UtcNow.ToString("o")
    public_key_path = $publicPath
  } | ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath $metadataPath

  Write-Host "Installer release signing key created."
  Write-Host "Key ID: $keyId"
  Write-Host "Protected private key: $privatePath"
  Write-Host "Public key to add to AURA_FIRMWARE_SIGNING_PUBLIC_KEYS: $publicPath"
} finally {
  Remove-Item -LiteralPath $tempPrivate -Force -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath $tempPublic -Force -ErrorAction SilentlyContinue
}
