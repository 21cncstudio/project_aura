param(
  [string]$KeyFile = (Join-Path $env:USERPROFILE ".aura-aq\installer-release-key.dpapi.json"),
  [string]$PublicKeyFile = (Join-Path $env:USERPROFILE ".aura-aq\installer-release-public.pem")
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
  throw "Node.js is required to generate an installer release signing key."
}

if ($env:OS -ne "Windows_NT") {
  throw "This key setup uses Windows DPAPI and must run on the trusted Windows release computer."
}
if (Test-Path -LiteralPath $KeyFile) {
  throw "A private installer release key already exists at $KeyFile. Move it explicitly before creating a replacement key."
}

$node = Resolve-NodeCommand
$generator = Join-Path $PSScriptRoot "generate_installer_release_key.mjs"
$generated = (& $node $generator | Out-String) | ConvertFrom-Json

if ($LASTEXITCODE -ne 0 -or -not $generated.private_key_pem -or -not $generated.public_key_pem) {
  throw "Ed25519 key generation failed."
}

$privateBytes = [System.Text.Encoding]::UTF8.GetBytes([string]$generated.private_key_pem)
$protectedBytes = [System.Security.Cryptography.ProtectedData]::Protect(
  $privateBytes,
  $null,
  [System.Security.Cryptography.DataProtectionScope]::CurrentUser
)
$keyDocument = [ordered]@{
  schema = "aura-installer-release-dpapi-key-v1"
  key_id = [string]$generated.key_id
  protected_private_key = [Convert]::ToBase64String($protectedBytes)
}

$keyDirectory = Split-Path -Parent $KeyFile
$publicDirectory = Split-Path -Parent $PublicKeyFile
New-Item -ItemType Directory -Force -Path $keyDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $publicDirectory | Out-Null

$keyDocument | ConvertTo-Json | Set-Content -Encoding Ascii -Path $KeyFile
[string]$generated.public_key_pem | Set-Content -Encoding Ascii -NoNewline -Path $PublicKeyFile

$escapedPublicKey = ([string]$generated.public_key_pem).Replace("`r", "").Replace("`n", "\n")
$vercelValue = [ordered]@{
  ([string]$generated.key_id) = $escapedPublicKey
} | ConvertTo-Json -Compress

Write-Host ""
Write-Host "Installer release signing key created."
Write-Host "Private DPAPI key: $KeyFile"
Write-Host "Public key: $PublicKeyFile"
Write-Host "Key ID: $($generated.key_id)"
Write-Host ""
Write-Host "Add this value to AURA_FIRMWARE_SIGNING_PUBLIC_KEYS in Aura Link:"
Write-Host $vercelValue
Write-Host ""
Write-Host "The plaintext private key was not written to disk."
