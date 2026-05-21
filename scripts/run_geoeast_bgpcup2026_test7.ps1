param(
    [string]$GeoEastRoot = $env:FPT_GEOEAST_ROOT,
    [string]$GeoEastDataRoot = $env:FPT_GEOEAST_DATA_ROOT,
    [string]$Database = $(if ($env:FPT_GEOEAST_DB) { $env:FPT_GEOEAST_DB } else { "ndp" }),
    [string]$Project = $(if ($env:FPT_GEOEAST_PROJECT) { $env:FPT_GEOEAST_PROJECT } else { "BGPCup2026_Test7" }),
    [string]$User = $(if ($env:FPT_GEOEAST_USER) { $env:FPT_GEOEAST_USER } else { "26312" }),
    [string]$Password = $(if ($null -ne $env:FPT_GEOEAST_PASSWORD) { $env:FPT_GEOEAST_PASSWORD } else { "" }),
    [string]$PgHost = $(if ($env:FPT_GEOEAST_PG_HOST) { $env:FPT_GEOEAST_PG_HOST } else { "127.0.0.1" }),
    [string]$PgPort = $(if ($env:FPT_GEOEAST_PG_PORT) { $env:FPT_GEOEAST_PG_PORT } else { "5433" }),
    [string]$OfficialDat = $env:FPT_OFFICIAL_DAT,
    [string]$Grid = $(if ($env:FPT_GEOEAST_GRID) { $env:FPT_GEOEAST_GRID } else { "test7_fault_attribute_bo" }),
    [string]$Output = $(if ($env:FPT_GEOEAST_OUTPUT) { $env:FPT_GEOEAST_OUTPUT } else { "FaultPolyTrace_Result" }),
    [string]$Runner = $env:FPT_GEOEAST_RUNNER,
    [double]$Percentile = 0.98,
    [double]$MinArea = 200.0,
    [switch]$NoImportDat
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($GeoEastRoot)) {
    $GeoEastRoot = "C:\Users\26312\GeoEast"
}
if ([string]::IsNullOrWhiteSpace($GeoEastDataRoot)) {
    $GeoEastDataRoot = "D:\BaiduNetdiskDownload\BGPCup2025\GeoEast-RC\GeoEastData.DataModel+SQLITE.20230728\GeoEastData"
}
if ([string]::IsNullOrWhiteSpace($OfficialDat)) {
    $OfficialDat = "D:\BaiduNetdiskDownload\BGPCup2025\赛题7-基于平面断层属性的断层多边形自动追踪\test.dat"
}

$env:NGP = Join-Path $GeoEastRoot "NGP"
$env:IECO = $env:NGP
$env:NDPDIR = $env:NGP
$env:NGPDIR = $env:NGP
$env:GEOEAST = Join-Path $GeoEastRoot "GeoEast-RC"
$env:PG_HOME = Join-Path $env:NGP "support\PostgreSQL\product"
$env:NGP_DBDATA_PATH = Join-Path $GeoEastDataRoot "pgdata\$Database"
$env:NGPDATA = Join-Path $GeoEastDataRoot "DATA\data"

$geoEastPath = @(
    (Join-Path $env:PG_HOME "bin"),
    (Join-Path $env:PG_HOME "lib"),
    (Join-Path $env:GEOEAST "bin"),
    (Join-Path $env:GEOEAST "bin\dbus"),
    (Join-Path $env:NGP "libso\nap"),
    (Join-Path $env:NGP "libso\ndp"),
    (Join-Path $env:NGP "libso\common"),
    (Join-Path $env:NGP "libso\geoscene"),
    (Join-Path $env:NGP "Qt\Qt5.15.2\5.15.2\msvc2019_64\bin"),
    (Join-Path $env:NGP "licmng\bin")
)
$env:PATH = ($geoEastPath -join ";") + ";" + $env:PATH

$hostNetworkDir = Join-Path $env:PG_HOME "network"
New-Item -ItemType Directory -Force -Path $hostNetworkDir | Out-Null
Set-Content -LiteralPath (Join-Path $hostNetworkDir "hostname.pg") -Value "$Database=$PgHost`:$PgPort" -Encoding ASCII

if ([string]::IsNullOrWhiteSpace($Runner)) {
    $runnerName = "faultpolytrace_geoeast_runner.exe"
    $candidatePaths = @(
        (Join-Path $PSScriptRoot "..\geoeast_plugin\bo_runner\$runnerName"),
        (Join-Path $PSScriptRoot "..\geoeast_plugin\bo_runner\release\$runnerName"),
        (Join-Path $PSScriptRoot "..\geoeast_plugin\bo_runner\debug\$runnerName")
    )

    foreach ($path in $candidatePaths) {
        $resolved = Resolve-Path -LiteralPath $path -ErrorAction SilentlyContinue
        if ($resolved) {
            $Runner = $resolved.Path
            break
        }
    }
}

if ([string]::IsNullOrWhiteSpace($Runner) -or !(Test-Path -LiteralPath $Runner)) {
    throw "Cannot find faultpolytrace_geoeast_runner.exe. Build geoeast_plugin\bo_runner first, or pass -Runner."
}

$runnerArgs = @(
    "--db", $Database,
    "--project", $Project,
    "--user", $User,
    "--password", $Password
)

if (!$NoImportDat) {
    $runnerArgs += @("--import-geoeast-dat", $OfficialDat, "--overwrite-grid")
}

$runnerArgs += @(
    "--grid", $Grid,
    "--output", $Output,
    "--trace-mode", "ridge-belt",
    "--threshold-mode", "percentile",
    "--percentile", $Percentile.ToString([Globalization.CultureInfo]::InvariantCulture),
    "--close-radius", "1",
    "--belt-radius", "2",
    "--min-branch-length", "6",
    "--max-gap-distance", "8",
    "--max-gap-angle", "45",
    "--min-area", $MinArea.ToString([Globalization.CultureInfo]::InvariantCulture)
)

Write-Host "GeoEast root: $GeoEastRoot"
Write-Host "GeoEastData root: $GeoEastDataRoot"
Write-Host "Database/project/user: $Database / $Project / $User"
Write-Host "Runner: $Runner"
& $Runner @runnerArgs

if ($LASTEXITCODE -ne 0) {
    throw "GeoEast runner failed with exit code $LASTEXITCODE."
}
