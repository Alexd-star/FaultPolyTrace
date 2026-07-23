param(
    [string]$OfficialDat = $(if ($env:FPT_OFFICIAL_DAT) { $env:FPT_OFFICIAL_DAT } else { "" })
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot "examples\outputs\official"
$output = Join-Path $outputDir "official_root_antimerge_benchmark.csv"

if ([string]::IsNullOrWhiteSpace($OfficialDat)) {
    throw "Official test data not specified. Pass -OfficialDat <test.dat> or set FPT_OFFICIAL_DAT."
}

if (!(Test-Path -LiteralPath $OfficialDat)) {
    throw "Official test data not found: $OfficialDat"
}

New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

& (Join-Path $repoRoot "scripts\build.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "Build failed."
}

& (Join-Path $repoRoot "build\faultpolytrace.exe") `
    --input $OfficialDat `
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
    --min-area 120 `
    --simplify-tolerance 2 `
    --smooth-iterations 1

if ($LASTEXITCODE -ne 0) {
    throw "Official benchmark failed."
}

Write-Host "Official benchmark output: $output"
Write-Host "Diagnostics: $output.diagnostics.json"
