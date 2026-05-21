$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$officialDat = "D:\BaiduNetdiskDownload\BGPCup2025\赛题7-基于平面断层属性的断层多边形自动追踪\test.dat"
$outputDir = Join-Path $repoRoot "examples\outputs\official"
$output = Join-Path $outputDir "test_p98_ridge_belt_benchmark.csv"

if (!(Test-Path -LiteralPath $officialDat)) {
    throw "Official test data not found: $officialDat"
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& (Join-Path $repoRoot "scripts\build.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "Build failed."
}

& (Join-Path $repoRoot "build\faultpolytrace.exe") `
    --input $officialDat `
    --input-format geoeast-dat `
    --output $output `
    --trace-mode ridge-belt `
    --threshold-mode percentile `
    --percentile 0.98 `
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
    --max-gap-distance 20 `
    --max-gap-angle 55 `
    --min-gap-support-ratio 0.25 `
    --residual-percentile 0.955 `
    --residual-fill-radius 4 `
    --residual-min-cells 2 `
    --residual-trace-min-length 6 `
    --residual-trace-max-cells 80 `
    --residual-covered-guard-radius 0 `
    --min-area 120 `
    --simplify-tolerance 2 `
    --smooth-iterations 1

if ($LASTEXITCODE -ne 0) {
    throw "Official benchmark failed."
}

Write-Host "Official benchmark output: $output"
Write-Host "Diagnostics: $output.diagnostics.json"
