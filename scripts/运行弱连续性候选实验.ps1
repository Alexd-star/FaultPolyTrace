param(
    [string]$OfficialDat = $(if ($env:FPT_OFFICIAL_DAT) { $env:FPT_OFFICIAL_DAT } else { "D:\BaiduNetdiskDownload\BGPCup2025\test.dat" }),
    [string]$OutputDirectory = "",
    [switch]$SkipBuild,
    [switch]$SkipReviewFigures
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $OutputDirectory = Join-Path $repoRoot "outputs\evidence\weak_continuity_candidate_$timestamp"
}
if (!(Test-Path -LiteralPath $OfficialDat)) {
    throw "Official DAT not found: $OfficialDat"
}
if (!$SkipBuild) {
    & (Join-Path $PSScriptRoot "build.ps1")
}
$exe = Join-Path $repoRoot "build\faultpolytrace.exe"
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$common = @(
    "--input", $OfficialDat,
    "--input-format", "geoeast-dat",
    "--trace-mode", "ridge-belt",
    "--threshold-mode", "percentile",
    "--percentile", "0.98",
    "--min-area", "120",
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
    "--smooth-iterations", "1"
)

$baseline = Join-Path $OutputDirectory "baseline_214.csv"
$shadow = Join-Path $OutputDirectory "weak_continuity_shadow.csv"
$candidate = Join-Path $OutputDirectory "weak_continuity_candidate.csv"
$strictCandidate = Join-Path $OutputDirectory "weak_continuity_strict_candidate.csv"
$multistageCandidate = Join-Path $OutputDirectory "weak_continuity_multistage_candidate.csv"

& $exe @common --output $baseline
if ($LASTEXITCODE -ne 0) { throw "Baseline run failed." }
$expectedHash = "A2A9644692B1E6635B152823CAE192C9C6EF19320AB5EF4273D5B2DB90638ECF"
$actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $baseline).Hash
if ($actualHash -ne $expectedHash) {
    throw "Baseline hash changed: $actualHash"
}

& $exe @common --output $shadow --weak-continuity-shadow --weak-tail-completion-shadow
if ($LASTEXITCODE -ne 0) { throw "Weak continuity shadow run failed." }
& $exe @common --output $multistageCandidate --weak-continuity --weak-continuity-second-pass `
    --weak-continuity-final-object-pass --weak-tail-completion `
    --preserve-observed-skeleton-envelope
if ($LASTEXITCODE -ne 0) { throw "Multistage weak continuity candidate run failed." }

& $exe @common --output $strictCandidate --weak-continuity-final-object-pass
if ($LASTEXITCODE -ne 0) { throw "Strict final-object weak continuity candidate run failed." }

& $exe @common --output $candidate --weak-tail-completion `
    --weak-continuity-final-object-pass --preserve-observed-skeleton-envelope
if ($LASTEXITCODE -ne 0) { throw "Weak-tail plus strict final-object candidate run failed." }

python (Join-Path $PSScriptRoot "诊断弱连续性落地率.py") `
    --dat $OfficialDat --baseline $baseline --candidate $candidate `
    --candidates "$candidate.weak-continuity.csv" `
    --output-dir (Join-Path $OutputDirectory "review")
if ($LASTEXITCODE -ne 0) { throw "Weak continuity landing diagnostics failed." }

if ($SkipReviewFigures) {
    Write-Host "Weak continuity core experiment generated: $OutputDirectory"
    return
}

$review = Join-Path $OutputDirectory "review"
python (Join-Path $PSScriptRoot "生成质量评估图.py") `
    --dat $OfficialDat --runs-dir $OutputDirectory `
    --out-dir (Join-Path $OutputDirectory "quality") `
    --include-regex "baseline_214|weak_continuity_candidate$"
if ($LASTEXITCODE -ne 0) { throw "Quality figure generation failed." }
python (Join-Path $PSScriptRoot "生成弱连续性复核包.py") `
    --dat $OfficialDat --baseline $baseline --candidate $candidate `
    --candidate-evidence "$candidate.weak-continuity.csv" --out-dir $review
if ($LASTEXITCODE -ne 0) { throw "Local review figure generation failed." }
python (Join-Path $PSScriptRoot "生成弱尾链复核包.py") `
    --dat $OfficialDat --baseline $baseline --candidate $candidate `
    --tail-evidence "$candidate.weak-tails.csv" `
    --out-dir (Join-Path $review "弱尾链复核")
if ($LASTEXITCODE -ne 0) { throw "Weak-tail review figure generation failed." }
python (Join-Path $PSScriptRoot "全图分块视觉复核.py") `
    --dat $OfficialDat --csv $baseline --out-dir (Join-Path $review "全图分块_基线") `
    --cols 6 --rows 4
if ($LASTEXITCODE -ne 0) { throw "Baseline tile review generation failed." }
python (Join-Path $PSScriptRoot "全图分块视觉复核.py") `
    --dat $OfficialDat --csv $candidate --out-dir (Join-Path $review "全图分块_候选") `
    --cols 6 --rows 4
if ($LASTEXITCODE -ne 0) { throw "Candidate tile review generation failed." }

[ordered]@{
    status = "PENDING_HUMAN_REVIEW"
    baselineSha256 = $actualHash
    baseline = $baseline
    shadow = $shadow
    candidate = $candidate
    strictCandidate = $strictCandidate
    multistageCandidate = $multistageCandidate
    review = $review
    generatedAt = (Get-Date).ToString("o")
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputDirectory "实验清单.json") -Encoding UTF8

Write-Host "Weak continuity experiment generated: $OutputDirectory"
