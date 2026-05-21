$ErrorActionPreference = "Stop"

$required = @("IECO", "NGP", "NDPDIR", "NGPDIR", "PG_HOME", "GEOEAST")
$optional = @("GEOEAST", "GGI_MAKE_PATH", "BASE_MAKE_PATH")

Write-Host "GeoEast development environment check"
Write-Host "Current shell: $($PSVersionTable.PSEdition) PowerShell $($PSVersionTable.PSVersion)"
Write-Host ""

$missing = @()
foreach ($name in $required) {
    $value = [Environment]::GetEnvironmentVariable($name)
    if ([string]::IsNullOrWhiteSpace($value)) {
        $missing += $name
        Write-Host "MISSING $name"
    } else {
        $exists = Test-Path -LiteralPath $value
        Write-Host "OK      $name = $value  exists=$exists"
    }
}

foreach ($name in $optional) {
    $value = [Environment]::GetEnvironmentVariable($name)
    if ([string]::IsNullOrWhiteSpace($value)) {
        Write-Host "OPTION  $name is not set"
    } else {
        $exists = Test-Path -LiteralPath $value
        Write-Host "OK      $name = $value  exists=$exists"
    }
}

Write-Host ""
if ($missing.Count -gt 0) {
    Write-Host "Result: GeoEast SDK variables are not available in this shell."
    Write-Host "Open the GeoEast/Qt/Visual Studio development environment, then run this script again."
    exit 1
}

$checks = @(
    (Join-Path $env:IECO "include\ndp\bo"),
    (Join-Path $env:IECO "libso\ndp"),
    (Join-Path $env:IECO "support\support_config.pri"),
    (Join-Path $env:IECO "support\pg_config.pri")
)

foreach ($path in $checks) {
    if (Test-Path -LiteralPath $path) {
        Write-Host "OK      $path"
    } else {
        Write-Host "MISSING $path"
        $missing += $path
    }
}

if ($missing.Count -gt 0) {
    Write-Host ""
    Write-Host "Result: variables exist, but one or more expected SDK paths are missing."
    exit 1
}

Write-Host ""
Write-Host "Result: GeoEast SDK environment looks ready for the bo_runner qmake project."
