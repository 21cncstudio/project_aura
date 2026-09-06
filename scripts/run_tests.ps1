param(
    [string[]]$Environment = @(),
    [string[]]$Filter = @(),
    [string]$CompilerDir = "C:\msys64\mingw64\bin",
    [ValidateRange(0, 3)]
    [int]$Verbosity = 0
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$pythonExe = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\python.exe"

if (-not (Test-Path -LiteralPath $pythonExe -PathType Leaf)) {
    throw "PlatformIO penv Python not found at $pythonExe. This launcher does not fall back to a PATH installation."
}

$launcherArgs = @("-I", (Join-Path $scriptDir "run_native_tests.py"), "--compiler-dir", $CompilerDir)
foreach ($item in $Environment) {
    $launcherArgs += @("--environment", $item)
}
foreach ($item in $Filter) {
    $launcherArgs += @("--filter", $item)
}
if ($Verbosity -gt 0) {
    $launcherArgs += "-" + ("v" * $Verbosity)
}

& $pythonExe @launcherArgs
exit $LASTEXITCODE
