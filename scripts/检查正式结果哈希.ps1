param(
    [string]$OfficialDat = $(if ($env:FPT_OFFICIAL_DAT) { $env:FPT_OFFICIAL_DAT } else { "" }),
    [string]$Executable = "",
    [string]$OutputDirectory = "",
    [switch]$SkipGeosValidation
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($Executable)) {
    $Executable = Join-Path $repoRoot "build\faultpolytrace.exe"
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot "outputs\regression\official_baseline_hash"
}
if (!(Test-Path -LiteralPath $OfficialDat)) {
    throw "Official test data not found: $OfficialDat"
}
if (!(Test-Path -LiteralPath $Executable)) {
    throw "FaultPolyTrace CLI not found: $Executable"
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$output = Join-Path $OutputDirectory "official_final_200.csv"
& $Executable `
    --input $OfficialDat --input-format geoeast-dat --output $output `
    --trace-mode ridge-belt --threshold-mode percentile --percentile 0.98 `
    --close-radius 1 --weak-percentile 0.965 --weak-connect-radius 1 `
    --max-weak-component-cells 20000 --min-weak-component-aspect 3 `
    --belt-radius 3 --attribute-constrained-belt --attribute-envelope-radius 1 `
    --post-belt-close-radius 1 --min-branch-length 4 `
    --min-junction-branch-length 2 --min-junction-branch-mean 0.94 `
    --max-gap-distance 12 --max-gap-angle 55 --min-gap-support-ratio 0.25 `
    --no-weak-continuity --no-weak-continuity-second-pass `
    --weak-continuity-final-object-pass --weak-tail-completion `
    --preserve-observed-skeleton-envelope `
    --no-separate-branch-belts --root-anti-merge `
    --root-anti-merge-support-ratio 0.25 --root-anti-merge-min-part-cells 0 `
    --root-anti-merge-min-part-ratio 0.12 --root-anti-merge-weak-guard-threshold 0.66 `
    --root-anti-merge-weak-guard-radius 0 --residual-percentile 0.955 `
    --residual-fill-radius 4 --residual-edge-fill-radius 2 `
    --residual-edge-fill-min-value-ratio 0.97 --residual-edge-fill-max-patch-span 5 `
    --residual-min-cells 2 --residual-trace-min-length 6 `
    --residual-trace-max-cells 80 --residual-covered-guard-radius 0 `
    --no-residual-trace --min-area 120 --simplify-tolerance 2 --smooth-iterations 1
if ($LASTEXITCODE -ne 0) {
    throw "Official baseline run failed with exit code $LASTEXITCODE."
}

$expected = "2911D696F880E1173F6633E6B8A4587316FDC7DFA19109CE8EB28083A076ED3A"
$actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $output).Hash
if ($actual -ne $expected) {
    throw "Official result hash regressed. Expected $expected, actual $actual."
}
$diagnostics = Get-Content -LiteralPath "$output.diagnostics.json" -Raw -Encoding UTF8 | ConvertFrom-Json
if ([int]$diagnostics.results.keptPolygonCount -ne 200) {
    throw "Official kept-polygon count regressed: $($diagnostics.results.keptPolygonCount)."
}
if ([int]$diagnostics.topology.openRings -ne 0 -or
    [int]$diagnostics.topology.selfIntersectingPolygons -ne 0 -or
    [int]$diagnostics.topology.polygonIntersections -ne 0 -or
    [int]$diagnostics.topology.invalidPolygons -ne 0 -or
    [int]$diagnostics.topology.polygonContainments -ne 0 -or
    [int]$diagnostics.topology.polygonInteriorOverlaps -ne 0) {
    throw "Official topology baseline regressed."
}

$geosReport = Join-Path $OutputDirectory "geos_geometry_validation.json"
if (!$SkipGeosValidation) {
    & python (Join-Path $repoRoot "scripts\validate_geometry_geos.py") `
        --input $output --report $geosReport --expected-count 200
    if ($LASTEXITCODE -ne 0) {
        throw "GEOS release geometry validation failed with exit code $LASTEXITCODE."
    }
}

[ordered]@{
    status = "PASS"
    input = (Resolve-Path -LiteralPath $OfficialDat).Path
    executable = (Resolve-Path -LiteralPath $Executable).Path
    output = (Resolve-Path -LiteralPath $output).Path
    keptPolygonCount = 200
    sha256 = $actual
    expectedSha256 = $expected
    topology = [ordered]@{
        openRings = 0
        selfIntersections = 0
        polygonIntersections = 0
        invalidPolygons = 0
        polygonContainments = 0
        polygonInteriorOverlaps = 0
    }
    geosValidation = $(if ($SkipGeosValidation) { "SKIPPED" } else { "PASS" })
    geosReport = $(if ($SkipGeosValidation) { $null } else { (Resolve-Path -LiteralPath $geosReport).Path })
    checkedAt = (Get-Date).ToString("o")
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDirectory "正式结果哈希回归.json") -Encoding UTF8

Write-Host "Official baseline hash regression: PASS ($actual)"
