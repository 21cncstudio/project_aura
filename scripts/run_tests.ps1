$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir
Set-Location $repoRoot

$pioCmd = Get-Command pio -ErrorAction SilentlyContinue
if ($pioCmd) {
    $pioExe = $pioCmd.Source
} else {
    $pioExe = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\platformio.exe"
    if (-not (Test-Path $pioExe)) {
        Write-Error "PlatformIO not found. Install it or add 'pio' to PATH."
        exit 1
    }
}

if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    Write-Error "gcc not found. Install MSYS2 mingw-w64-x86_64-toolchain and add C:\\msys64\\mingw64\\bin to PATH."
    exit 1
}

& $pioExe test -e native_test
exit $LASTEXITCODE
