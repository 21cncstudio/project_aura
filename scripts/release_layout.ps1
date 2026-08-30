function ConvertTo-AuraFlashOffset {
  param(
    [Parameter(Mandatory = $true)][string]$Value,
    [Parameter(Mandatory = $true)][string]$PartitionName
  )

  $trimmed = $Value.Trim()
  try {
    if ($trimmed -match '^0[xX]([0-9A-Fa-f]+)$') {
      return [Convert]::ToInt64($Matches[1], 16)
    }
    if ($trimmed -match '^\d+$') {
      return [long]::Parse($trimmed, [Globalization.CultureInfo]::InvariantCulture)
    }
  } catch {
    throw "Partition '$PartitionName' has an invalid offset '$Value'."
  }

  throw "Partition '$PartitionName' has an invalid offset '$Value'."
}

function Assert-AuraCanonicalFlashOffset {
  param(
    [Parameter(Mandatory = $true)][string]$Value,
    [Parameter(Mandatory = $true)][long]$ExpectedValue,
    [Parameter(Mandatory = $true)][string]$PartitionName
  )

  $actualValue = ConvertTo-AuraFlashOffset -Value $Value -PartitionName $PartitionName
  if ($actualValue -ne $ExpectedValue) {
    throw ("Partition '{0}' has non-canonical offset {1}; expected 0x{2:X}." -f `
      $PartitionName, $Value, $ExpectedValue)
  }

  return ("0x{0:X}" -f $ExpectedValue)
}

function Get-AuraPartitionOffset {
  param(
    [Parameter(Mandatory = $true)][string]$CsvPath,
    [Parameter(Mandatory = $true)][string]$Name
  )

  if (-not (Test-Path -LiteralPath $CsvPath -PathType Leaf)) {
    return $null
  }

  # Do not name this collection `$matches`: PowerShell's case-insensitive
  # automatic `$Matches` variable is replaced by every successful regex match.
  $partitionOffsets = @()
  $lines = Get-Content -LiteralPath $CsvPath | Where-Object { $_ -and $_ -notmatch "^\s*#" }
  foreach ($line in $lines) {
    $parts = $line.Split(",") | ForEach-Object { $_.Trim() }
    if ($parts.Count -ge 4 -and $parts[0] -eq $Name) {
      $partitionOffsets += $parts[3]
    }
  }

  if ($partitionOffsets.Count -gt 1) {
    throw "Partition table contains duplicate '$Name' entries: $CsvPath"
  }
  if ($partitionOffsets.Count -eq 1) {
    return $partitionOffsets[0]
  }
  return $null
}
