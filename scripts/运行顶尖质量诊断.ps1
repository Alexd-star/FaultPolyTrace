param(
    [string]$OfficialDat = "",
    [string]$ResultCsv = "",
    [string]$OutputRoot = "",
    [double]$StrongThreshold = 0.95,
    [double]$WeakThreshold = 0.66,
    [double]$LowThreshold = 0.05,
    [int]$MinReviewCells = 8,
    [double]$MinLineAspect = 3.0
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")

if ([string]::IsNullOrWhiteSpace($OfficialDat)) {
    $candidate = Get-ChildItem -LiteralPath "D:\BaiduNetdiskDownload\BGPCup2025" -Recurse -Filter "test.dat" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -like "*BGPCup2025*" } |
        Select-Object -First 1
    if ($candidate) {
        $OfficialDat = $candidate.FullName
    }
}

if ([string]::IsNullOrWhiteSpace($ResultCsv)) {
    $ResultCsv = Join-Path $repoRoot "examples\outputs\performance_regression\official_root_antimerge.csv"
}

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $OutputRoot = Join-Path $repoRoot "examples\outputs\top_quality_diagnosis\diagnosis_$timestamp"
}

if (!(Test-Path -LiteralPath $OfficialDat)) {
    throw "官方测试数据不存在：$OfficialDat"
}
if (!(Test-Path -LiteralPath $ResultCsv)) {
    throw "结果 CSV 不存在：$ResultCsv。请先运行 .\scripts\性能回归检查.ps1 或传入 -ResultCsv。"
}

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$diagnosisScript = Join-Path $PSScriptRoot "顶尖质量诊断.py"
$runner = @"
import runpy
import sys
sys.path.insert(0, r'$PSScriptRoot')
sys.argv = [
    r'$diagnosisScript',
    '--dat', r'$OfficialDat',
    '--csv', r'$ResultCsv',
    '--out-dir', r'$OutputRoot',
    '--strong-threshold', '$StrongThreshold',
    '--weak-threshold', '$WeakThreshold',
    '--low-threshold', '$LowThreshold',
    '--min-review-cells', '$MinReviewCells',
    '--min-line-aspect', '$MinLineAspect',
]
runpy.run_path(r'$diagnosisScript', run_name='__main__')
"@
$runner | python - | Out-Host

if ($LASTEXITCODE -ne 0) {
    throw "顶尖质量诊断失败。"
}

Write-Host "顶尖质量诊断目录：$OutputRoot"
Write-Host "诊断报告：$(Join-Path $OutputRoot "顶尖质量诊断报告.md")"
