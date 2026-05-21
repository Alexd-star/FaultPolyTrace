$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot "generate_synthetic.ps1")
& (Join-Path $PSScriptRoot "build.ps1")

$outDir = Join-Path $repoRoot "examples\outputs"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$exe = Join-Path $repoRoot "build\faultpolytrace.exe"
$input = Join-Path $repoRoot "examples\synthetic\cross_fault_grid.txt"
$output = Join-Path $outDir "cross_fault_polygons.csv"

& $exe --input $input --output $output --trace-mode region --threshold-mode fixed --threshold 0.45 --min-area 20 --min-cells 3

if (!(Test-Path $output)) {
    throw "Expected output was not created: $output"
}

$lines = Get-Content -LiteralPath $output
if ($lines.Count -lt 2) {
    throw "Smoke test failed: output CSV has no polygon rows."
}

$lInput = Join-Path $repoRoot "examples\synthetic\l_shape_grid.txt"
$lOutput = Join-Path $outDir "l_shape_polygons.csv"
& $exe --input $lInput --output $lOutput --trace-mode region --threshold-mode fixed --threshold 0.5 --min-area 1 --min-cells 1 --simplify-tolerance 0 --smooth-iterations 0

$lRows = @(Import-Csv -LiteralPath $lOutput)
if ($lRows.Count -ne 1) {
    throw "L-shape test failed: expected one polygon, got $($lRows.Count)."
}

$lArea = [double]$lRows[0].area
if ([Math]::Abs($lArea - 36.0) -gt 0.0001) {
    throw "L-shape test failed: expected boundary area 36, got $lArea."
}

$pOutput = Join-Path $outDir "percentile_noisy_blobs_polygons.csv"
$pInput = Join-Path $repoRoot "examples\synthetic\noisy_blobs_grid.txt"
& $exe --input $pInput --output $pOutput --trace-mode region --threshold-mode percentile --percentile 0.88 --min-area 10 --min-cells 3 --open-radius 1 --close-radius 1

$pRows = @(Import-Csv -LiteralPath $pOutput)
if ($pRows.Count -lt 1) {
    throw "Percentile segmentation test failed: expected at least one polygon."
}

$rInput = Join-Path $repoRoot "examples\synthetic\ridge_fault_grid.txt"
$rOutput = Join-Path $outDir "ridge_belt_polygons.csv"
& $exe --input $rInput --output $rOutput --trace-mode ridge-belt --threshold-mode percentile --percentile 0.94 --min-area 10 --min-cells 3 --close-radius 1 --belt-radius 2 --min-branch-length 8

$rRows = @(Import-Csv -LiteralPath $rOutput)
if ($rRows.Count -lt 1) {
    throw "Ridge-belt test failed: expected at least one polygon."
}
if (!(Test-Path "$rOutput.diagnostics.json")) {
    throw "Ridge-belt test failed: diagnostics file missing."
}

$bInput = Join-Path $repoRoot "examples\synthetic\broken_ridge_fault_grid.txt"
$bNoConnectOutput = Join-Path $outDir "broken_ridge_no_connect.csv"
$bConnectOutput = Join-Path $outDir "broken_ridge_connected.csv"
& $exe --input $bInput --output $bNoConnectOutput --trace-mode ridge-belt --threshold-mode percentile --percentile 0.90 --min-area 8 --min-cells 3 --belt-radius 2 --min-branch-length 3
& $exe --input $bInput --output $bConnectOutput --trace-mode ridge-belt --threshold-mode percentile --percentile 0.90 --min-area 8 --min-cells 3 --belt-radius 2 --min-branch-length 3 --max-gap-distance 12 --max-gap-angle 50

$bNoRows = @(Import-Csv -LiteralPath $bNoConnectOutput)
$bRows = @(Import-Csv -LiteralPath $bConnectOutput)
if ($bNoRows.Count -lt 2) {
    throw "Broken ridge setup failed: expected multiple polygons without gap connection."
}
if ($bRows.Count -gt $bNoRows.Count) {
    throw "Gap connection test failed: connected result should not increase polygon count."
}
$bDiagnostics = Get-Content -LiteralPath "$bConnectOutput.diagnostics.json" -Raw
if ($bDiagnostics -notmatch '"connectedGaps"') {
    throw "Gap connection test failed: diagnostics missing connectedGaps."
}
if ($bDiagnostics -notmatch '"topology"') {
    throw "Gap connection test failed: diagnostics missing topology section."
}

Write-Host "Smoke test passed: $output"
Write-Host "Geometry test passed: $lOutput"
Write-Host "Percentile segmentation test passed: $pOutput"
Write-Host "Ridge-belt test passed: $rOutput"
Write-Host "Gap connection test passed: $bConnectOutput"
