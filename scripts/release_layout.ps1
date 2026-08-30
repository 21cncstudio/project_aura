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
