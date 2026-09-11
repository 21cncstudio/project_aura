[CmdletBinding()]
param(
    [ValidateSet('4_3', '7_dual_i2c')]
    [string]$Profile = '4_3',
    [string]$IdfPath = $env:IDF_PATH,
    [string]$ToolsPath = $env:IDF_TOOLS_PATH,
    [ValidateRange(1, 32)]
    [int]$Jobs = 4,
    [ValidateSet('build', 'reconfigure')]
    [string]$Action = 'build'
)

$ErrorActionPreference = 'Stop'
if (-not $IdfPath) { throw 'Set IDF_PATH or pass -IdfPath to an ESP-IDF 6.1 checkout.' }
$idfRoot = (Resolve-Path -LiteralPath $IdfPath).Path
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if ($ToolsPath) { $env:IDF_TOOLS_PATH = (Resolve-Path -LiteralPath $ToolsPath).Path }
$env:IDF_PATH = $idfRoot
$env:IDF_PY_BUILD_JOBS = [string]$Jobs
. (Join-Path $idfRoot 'export.ps1')
if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF environment activation failed.' }
$pythonExe = Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe'
$buildDir = Join-Path $projectRoot ('build-idf-' + $Profile)
Push-Location -LiteralPath $projectRoot
try {
    # Refresh identity before Ninja evaluates header dependencies after a commit.
    & $pythonExe (Join-Path $projectRoot 'scripts\idf_prepare.py') --profile $Profile --build-dir $buildDir
    if ($LASTEXITCODE -ne 0) { throw 'Aura native build preparation failed.' }
    & $pythonExe (Join-Path $idfRoot 'tools\idf.py') -B $buildDir `
        -D ('AURA_PROFILE=' + $Profile) -D ('Python3_EXECUTABLE=' + $pythonExe) $Action
    if ($LASTEXITCODE -ne 0) { throw "ESP-IDF $Action failed with exit code $LASTEXITCODE" }
} finally {
    Pop-Location
}
