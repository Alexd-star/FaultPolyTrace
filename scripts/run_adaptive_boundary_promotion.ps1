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
    $OutputRoot = Join-Path $repoRoot "outputs\development\adaptive_boundary_promotion"
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$common = @(
    "--input", $OfficialDat,
    "--input-format", "geoeast-dat"
)

$baseline = Join-Path $OutputRoot "baseline.csv"
$candidate = Join-Path $OutputRoot "adaptive_boundary_candidate.csv"
& $exe @common "--no-adaptive-boundary-apply" "--output" $baseline
if ($LASTEXITCODE -ne 0) { throw "Adaptive-boundary baseline failed." }
& $exe @common "--adaptive-boundary-apply" "--output" $candidate
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
        evaluated = $candidateDiag.adaptiveBoundaryShadow.polygonsEvaluated
        accepted = $candidateDiag.adaptiveBoundaryShadow.acceptedCandidates
        committed = $candidateDiag.adaptiveBoundaryShadow.commitCommittedCandidates
        rejected = $candidateDiag.adaptiveBoundaryShadow.rejectedCandidates
        rolledBack = $candidateDiag.adaptiveBoundaryShadow.commitBatchRolledBack
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $candidate).Hash
    }
    promoted = [bool]$gateResult.decision.pass
    decision = $gateResult.decision
    checkedAt = (Get-Date).ToString("o")
} | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $OutputRoot "adaptive_boundary_summary.json") -Encoding UTF8

Write-Host "Adaptive-boundary promotion test completed: $OutputRoot"
