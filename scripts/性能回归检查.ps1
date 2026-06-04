param(
    [string]$OfficialDat = "",
    [int]$MaxElapsedMs = 5000,
    [int]$ExpectedPolygons = 232,
    [int]$ExpectedKept = 214,
    [int]$MaxPolygonIntersections = 0,
    [switch]$SkipCountCheck
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot "examples\outputs\performance_regression"
$output = Join-Path $outputDir "official_root_antimerge.csv"
$exe = Join-Path $repoRoot "build\faultpolytrace.exe"

if ([string]::IsNullOrWhiteSpace($OfficialDat)) {
    $candidate = Get-ChildItem -LiteralPath "D:\BaiduNetdiskDownload\BGPCup2025" -Recurse -Filter "test.dat" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -like "*BGPCup2025*" } |
        Select-Object -First 1
    if ($candidate) {
        $OfficialDat = $candidate.FullName
    }
}

if (!(Test-Path -LiteralPath $OfficialDat)) {
    throw "Official test data not found: $OfficialDat"
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& (Join-Path $repoRoot "scripts\build.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "Build failed."
}

& $exe `
    --input $OfficialDat `
    --input-format geoeast-dat `
    --output $output `
    --trace-mode ridge-belt `
    --threshold-mode percentile `
    --percentile 0.98 `
    --min-area 120 `
    --close-radius 1 `
    --weak-percentile 0.965 `
    --weak-connect-radius 1 `
    --max-weak-component-cells 20000 `
    --min-weak-component-aspect 3 `
    --belt-radius 3 `
    --attribute-constrained-belt `
    --attribute-envelope-radius 1 `
    --post-belt-close-radius 1 `
    --min-branch-length 4 `
    --min-junction-branch-length 2 `
    --min-junction-branch-mean 0.94 `
    --max-gap-distance 12 `
    --max-gap-angle 55 `
    --min-gap-support-ratio 0.25 `
    --no-separate-branch-belts `
    --root-anti-merge `
    --root-anti-merge-support-ratio 0.25 `
    --root-anti-merge-min-part-cells 0 `
    --root-anti-merge-min-part-ratio 0.12 `
    --root-anti-merge-weak-guard-threshold 0.66 `
    --root-anti-merge-weak-guard-radius 0 `
    --residual-percentile 0.955 `
    --residual-fill-radius 4 `
    --residual-edge-fill-radius 2 `
    --residual-edge-fill-min-value-ratio 0.97 `
    --residual-edge-fill-max-patch-span 5 `
    --residual-min-cells 2 `
    --residual-trace-min-length 6 `
    --residual-trace-max-cells 80 `
    --residual-covered-guard-radius 0 `
    --no-residual-trace `
    --simplify-tolerance 2 `
    --smooth-iterations 1 | Out-Host

if ($LASTEXITCODE -ne 0) {
    throw "Official performance regression run failed."
}

$diagnosticsPath = "$output.diagnostics.json"
$diagnostics = Get-Content -LiteralPath $diagnosticsPath -Raw | ConvertFrom-Json

$elapsedMs = [int]$diagnostics.timing.elapsedMs
$polygonCount = [int]$diagnostics.results.polygonCount
$keptCount = [int]$diagnostics.results.keptPolygonCount
$openRings = [int]$diagnostics.topology.openRings
$selfIntersections = [int]$diagnostics.topology.selfIntersectingPolygons
$polygonIntersections = [int]$diagnostics.topology.polygonIntersections

if ($elapsedMs -gt $MaxElapsedMs) {
    throw "Performance regression failed: elapsed ${elapsedMs}ms > ${MaxElapsedMs}ms."
}
if (!$SkipCountCheck) {
    if ($polygonCount -ne $ExpectedPolygons) {
        throw "Result regression failed: polygon count $polygonCount != expected $ExpectedPolygons."
    }
    if ($keptCount -ne $ExpectedKept) {
        throw "Result regression failed: kept polygon count $keptCount != expected $ExpectedKept."
    }
}
if ($openRings -ne 0 -or $selfIntersections -ne 0) {
    throw "Topology regression failed: openRings=$openRings, selfIntersections=$selfIntersections."
}
if ($polygonIntersections -gt $MaxPolygonIntersections) {
    throw "Topology QC failed: polygon contact/intersection diagnostics=$polygonIntersections > $MaxPolygonIntersections."
}

Write-Host "Performance regression passed."
Write-Host "Elapsed: $elapsedMs ms"
Write-Host "Timing: read=$($diagnostics.timing.readMs) ms, trace=$($diagnostics.timing.traceMs) ms, topology=$($diagnostics.timing.topologyMs) ms, write=$($diagnostics.timing.writeMs) ms"
Write-Host "Output: $output"
Write-Host "Diagnostics: $diagnosticsPath"
