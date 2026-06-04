param(
    [string]$GeoEastRoot = $env:FPT_GEOEAST_ROOT,
    [string]$GeoEastDataRoot = $env:FPT_GEOEAST_DATA_ROOT,
    [string]$Database = $(if ($env:FPT_GEOEAST_DB) { $env:FPT_GEOEAST_DB } else { "ndp" }),
    [string]$Project = $env:FPT_GEOEAST_PROJECT,
    [string]$User = $(if ($env:FPT_GEOEAST_USER) { $env:FPT_GEOEAST_USER } else { $env:USERNAME }),
    [string]$Password = $(if ($null -ne $env:FPT_GEOEAST_PASSWORD) { $env:FPT_GEOEAST_PASSWORD } else { "" }),
    [string]$PgHost = $(if ($env:FPT_GEOEAST_PG_HOST) { $env:FPT_GEOEAST_PG_HOST } else { "127.0.0.1" }),
    [string]$PgPort = $(if ($env:FPT_GEOEAST_PG_PORT) { $env:FPT_GEOEAST_PG_PORT } else { "5433" }),
    [string]$OfficialDat = $env:FPT_OFFICIAL_DAT,
    [string]$Grid = $env:FPT_GEOEAST_GRID,
    [string]$Output = $(if ($env:FPT_GEOEAST_OUTPUT) { $env:FPT_GEOEAST_OUTPUT } else { "FaultPolyTrace_Result" }),
    [string]$Runner = $env:FPT_GEOEAST_RUNNER,
    [double]$Percentile = 0.98,
    [double]$MinArea = 120.0,
    [switch]$NoImportDat
)

$ErrorActionPreference = "Stop"

function Find-GeoEastRoot {
    if ($env:FPT_GEOEAST_ROOT) {
        return $env:FPT_GEOEAST_ROOT
    }
    if ($env:NGP) {
        $candidate = Split-Path -Parent $env:NGP
        if (Test-Path -LiteralPath (Join-Path $candidate "GeoEast-RC")) {
            return $candidate
        }
    }
    if ($env:USERPROFILE) {
        $candidate = Join-Path $env:USERPROFILE "GeoEast"
        if (Test-Path -LiteralPath (Join-Path $candidate "GeoEast-RC")) {
            return $candidate
        }
    }
    if (Test-Path -LiteralPath "C:\GeoEast\GeoEast-RC") {
        return "C:\GeoEast"
    }
    return $null
}

function Find-GeoEastDataRoot {
    param([string]$Root)
    if ($env:FPT_GEOEAST_DATA_ROOT) {
        return $env:FPT_GEOEAST_DATA_ROOT
    }
    if ($env:NGP_DBDATA_PATH) {
        $pgData = Split-Path -Parent $env:NGP_DBDATA_PATH
        return Split-Path -Parent $pgData
    }
    if ([string]::IsNullOrWhiteSpace($Root)) {
        return $null
    }
    foreach ($candidate in @(
        (Join-Path $Root "GeoEast-RC\GeoEastData"),
        (Join-Path $Root "GeoEastData")
    )) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    $rc = Join-Path $Root "GeoEast-RC"
    if (Test-Path -LiteralPath $rc) {
        $match = Get-ChildItem -LiteralPath $rc -Directory -Filter "GeoEastData*" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($match) {
            $nested = Join-Path $match.FullName "GeoEastData"
            return $(if (Test-Path -LiteralPath $nested) { $nested } else { $match.FullName })
        }
    }
    return $null
}

function Find-OfficialDat {
    if ($env:FPT_OFFICIAL_DAT -and (Test-Path -LiteralPath $env:FPT_OFFICIAL_DAT)) {
        return $env:FPT_OFFICIAL_DAT
    }
    $roots = @()
    if ($env:FPT_OFFICIAL_DATA_ROOT) { $roots += $env:FPT_OFFICIAL_DATA_ROOT }
    if ($env:USERPROFILE) { $roots += (Join-Path $env:USERPROFILE "Documents") }
    $roots += (Join-Path (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")) "examples")
    foreach ($root in $roots) {
        if (!(Test-Path -LiteralPath $root)) {
            continue
        }
        $match = Get-ChildItem -LiteralPath $root -Recurse -Filter "test.dat" -ErrorAction SilentlyContinue |
            Sort-Object Length -Descending |
            Select-Object -First 1
        if ($match) {
            return $match.FullName
        }
    }
    return $null
}

if ([string]::IsNullOrWhiteSpace($GeoEastRoot)) {
    $GeoEastRoot = Find-GeoEastRoot
}
if ([string]::IsNullOrWhiteSpace($GeoEastDataRoot)) {
    $GeoEastDataRoot = Find-GeoEastDataRoot -Root $GeoEastRoot
}
if ([string]::IsNullOrWhiteSpace($OfficialDat)) {
    $OfficialDat = Find-OfficialDat
}
if ([string]::IsNullOrWhiteSpace($GeoEastRoot) -or !(Test-Path -LiteralPath (Join-Path $GeoEastRoot "GeoEast-RC"))) {
    throw "Cannot locate GeoEast root. Pass -GeoEastRoot or set FPT_GEOEAST_ROOT."
}
if ([string]::IsNullOrWhiteSpace($GeoEastDataRoot) -or !(Test-Path -LiteralPath $GeoEastDataRoot)) {
    throw "Cannot locate GeoEastData root. Pass -GeoEastDataRoot or set FPT_GEOEAST_DATA_ROOT."
}
if ([string]::IsNullOrWhiteSpace($Project)) {
    throw "Missing GeoEast project. Pass -Project or set FPT_GEOEAST_PROJECT."
}
if ([string]::IsNullOrWhiteSpace($User)) {
    throw "Missing GeoEast user. Pass -User or set FPT_GEOEAST_USER."
}
if ([string]::IsNullOrWhiteSpace($Grid)) {
    throw "Missing input BOMapGrid. Pass -Grid or set FPT_GEOEAST_GRID."
}
if (!$NoImportDat -and ([string]::IsNullOrWhiteSpace($OfficialDat) -or !(Test-Path -LiteralPath $OfficialDat))) {
    throw "Cannot locate official test.dat. Pass -OfficialDat or set FPT_OFFICIAL_DAT."
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
    "--weak-percentile", "0.965",
    "--weak-connect-radius", "1",
    "--max-weak-component-cells", "20000",
    "--min-weak-component-aspect", "3",
    "--belt-radius", "3",
    "--attribute-constrained-belt",
    "--attribute-envelope-radius", "1",
    "--post-belt-close-radius", "1",
    "--min-branch-length", "4",
    "--min-junction-branch-length", "2",
    "--min-junction-branch-mean", "0.94",
    "--max-gap-distance", "12",
    "--max-gap-angle", "55",
    "--min-gap-support-ratio", "0.25",
    "--no-separate-branch-belts",
    "--root-anti-merge",
    "--root-anti-merge-support-ratio", "0.25",
    "--root-anti-merge-min-part-cells", "0",
    "--root-anti-merge-min-part-ratio", "0.12",
    "--root-anti-merge-weak-guard-threshold", "0.66",
    "--root-anti-merge-weak-guard-radius", "0",
    "--residual-percentile", "0.955",
    "--residual-fill-radius", "4",
    "--residual-edge-fill-radius", "2",
    "--residual-edge-fill-min-value-ratio", "0.97",
    "--residual-edge-fill-max-patch-span", "5",
    "--residual-min-cells", "2",
    "--residual-trace-min-length", "6",
    "--residual-trace-max-cells", "80",
    "--residual-covered-guard-radius", "0",
    "--no-residual-trace",
    "--simplify-tolerance", "2",
    "--smooth-iterations", "1",
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
