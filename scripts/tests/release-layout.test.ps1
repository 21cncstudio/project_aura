$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. (Join-Path (Split-Path -Parent $PSScriptRoot) "release_layout.ps1")

$tempCsv = Join-Path ([System.IO.Path]::GetTempPath()) ("aura-partitions-" + [guid]::NewGuid() + ".csv")
try {
  @"
# Name, Type, SubType, Offset, Size
nvs, data, nvs, 0x9000, 0x5000
app0, app, ota_0, 0x10000, 0x680000
littlefs, data, spiffs, 0xC90000, 0x360000
"@ | Set-Content -LiteralPath $tempCsv -Encoding Ascii

  if ((Get-AuraPartitionOffset -CsvPath $tempCsv -Name "app0") -ne "0x10000") {
    throw "app0 offset was not read from the partition table."
  }
  if ((Get-AuraPartitionOffset -CsvPath $tempCsv -Name "littlefs") -ne "0xC90000") {
    throw "littlefs offset was not read after regex filtering."
  }
  if ($null -ne (Get-AuraPartitionOffset -CsvPath $tempCsv -Name "missing")) {
    throw "A missing partition must return null."
  }

  Add-Content -LiteralPath $tempCsv -Encoding Ascii -Value "app0, app, ota_0, 0x20000, 0x680000"
  try {
    Get-AuraPartitionOffset -CsvPath $tempCsv -Name "app0" | Out-Null
    throw "A duplicate partition name must be rejected."
  } catch {
    if ($_.Exception.Message -notmatch "duplicate 'app0'") {
      throw
    }
  }

  Write-Output "release-layout tests: 4 passed"
} finally {
  Remove-Item -LiteralPath $tempCsv -Force -ErrorAction SilentlyContinue
}
