param(
    [string]$OfficialDat = $(if ($env:FPT_OFFICIAL_DAT) { $env:FPT_OFFICIAL_DAT } else { "" }),
    [string]$OutputRoot = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repoRoot "build\faultpolytrace.exe"
if (!(Test-Path -LiteralPath $OfficialDat)) { throw "Official test data not found: $OfficialDat" }
if (!(Test-Path -LiteralPath $exe)) { throw "FaultPolyTrace CLI not found: $exe" }
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $repoRoot "outputs\development_20260730\adaptive_boundary_promotion"
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$common = @(
    "--input", $OfficialDat, "--input-format", "geoeast-dat",
    "--trace-mode", "ridge-belt", "--threshold-mode", "percentile", "--percentile", "0.98",
    "--close-radius", "1", "--weak-percentile", "0.965", "--weak-connect-radius", "1",
    "--max-weak-component-cells", "20000", "--min-weak-component-aspect", "3",
    "--belt-radius", "3", "--attribute-constrained-belt", "--attribute-envelope-radius", "1",
    "--post-belt-close-radius", "1", "--min-branch-length", "4",
    "--min-junction-branch-length", "2", "--min-junction-branch-mean", "0.94",
    "--max-gap-distance", "12", "--max-gap-angle", "55", "--min-gap-support-ratio", "0.25",
    "--no-separate-branch-belts", "--root-anti-merge",
    "--root-anti-merge-support-ratio", "0.25", "--root-anti-merge-min-part-cells", "0",
    "--root-anti-merge-min-part-ratio", "0.12", "--root-anti-merge-weak-guard-threshold", "0.66",
    "--root-anti-merge-weak-guard-radius", "0", "--residual-percentile", "0.955",
    "--residual-fill-radius", "4", "--residual-edge-fill-radius", "2",
    "--residual-edge-fill-min-value-ratio", "0.97", "--residual-edge-fill-max-patch-span", "5",
    "--residual-min-cells", "2", "--residual-trace-min-length", "6",
    "--residual-trace-max-cells", "80", "--residual-covered-guard-radius", "0",
    "--no-residual-trace", "--min-area", "120", "--simplify-tolerance", "2", "--smooth-iterations", "1",
    "--no-branch-attribution-shadow", "--no-weak-path-gate"
)

$baseline = Join-Path $OutputRoot "baseline.csv"
$candidate = Join-Path $OutputRoot "adaptive_boundary_candidate.csv"
& $exe @common "--no-local-polygon-quality-shadow" "--no-local-polygon-quality" "--output" $baseline
if ($LASTEXITCODE -ne 0) { throw "Adaptive-boundary baseline failed." }
& $exe @common "--no-local-polygon-quality-shadow" "--local-polygon-quality" "--output" $candidate
if ($LASTEXITCODE -ne 0) { throw "Adaptive-boundary candidate failed." }

$gate = Join-Path $OutputRoot "promotion_gate"
python (Join-Path $repoRoot "scripts\冲顶实验晋级验收.py") `
    --dat $OfficialDat --baseline-csv $baseline --candidate-csv $candidate --out-dir $gate
if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 2) {
    throw "Adaptive-boundary promotion comparison failed."
}

$baseDiag = Get-Content -LiteralPath "$baseline.diagnostics.json" -Raw -Encoding UTF8 | ConvertFrom-Json
$candidateDiag = Get-Content -LiteralPath "$candidate.diagnostics.json" -Raw -Encoding UTF8 | ConvertFrom-Json
$gateResult = Get-Content -LiteralPath (Join-Path $gate "冲顶实验晋级验收摘要.json") -Raw -Encoding UTF8 | ConvertFrom-Json
[ordered]@{
    status = "PASS"
    baseline = [ordered]@{
        keptPolygons = $baseDiag.results.keptPolygonCount
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $baseline).Hash
    }
    candidate = [ordered]@{
        keptPolygons = $candidateDiag.results.keptPolygonCount
        detected = $candidateDiag.localPolygonQuality.detected
        accepted = $candidateDiag.localPolygonQuality.accepted
        review = $candidateDiag.localPolygonQuality.review
        rejected = $candidateDiag.localPolygonQuality.rejected
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $candidate).Hash
    }
    promoted = [bool]$gateResult.decision.pass
    decision = $gateResult.decision
    checkedAt = (Get-Date).ToString("o")
} | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $OutputRoot "adaptive_boundary_summary.json") -Encoding UTF8

Write-Host "Adaptive-boundary promotion test completed: $OutputRoot"
