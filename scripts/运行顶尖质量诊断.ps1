param(
    [string]$OfficialDat = "",
    [string]$ResultCsv = "",
    [string]$OutputRoot = "",
    [double]$StrongThreshold = 0.95,
    [double]$WeakThreshold = 0.66,
    [double]$LowThreshold = 0.05,
    [int]$MinReviewCells = 8,
    [double]$MinLineAspect = 3.0,
    [double]$LocalShapeWidthRatio = 2.35,
    [int]$LocalShapeMinExtraCells = 24,
    [int]$LocalShapeWindow = 11
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")

if ([string]::IsNullOrWhiteSpace($OfficialDat)) {
    if ($env:FPT_OFFICIAL_DAT) {
        $OfficialDat = $env:FPT_OFFICIAL_DAT
    } elseif ($env:FPT_OFFICIAL_DATA_DIR -and (Test-Path -LiteralPath $env:FPT_OFFICIAL_DATA_DIR)) {
        $candidate = Get-ChildItem -LiteralPath $env:FPT_OFFICIAL_DATA_DIR -Recurse -Filter "test.dat" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($candidate) {
            $OfficialDat = $candidate.FullName
        }
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
    throw "官方测试数据不存在：$OfficialDat。请通过 -OfficialDat <test.dat> 或环境变量 FPT_OFFICIAL_DAT 指定。"
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

$localShapeOutputRoot = Join-Path $OutputRoot "局部形态质量诊断"
python (Join-Path $PSScriptRoot "局部形态质量诊断.py") `
    --dat $OfficialDat `
    --csv $ResultCsv `
    --out-dir $localShapeOutputRoot `
    --width-ratio $LocalShapeWidthRatio `
    --min-extra-cells $LocalShapeMinExtraCells `
    --window $LocalShapeWindow `
    --max-figures 30 | Out-Host

if ($LASTEXITCODE -ne 0) {
    throw "局部形态质量诊断失败。"
}

Write-Host "顶尖质量诊断目录：$OutputRoot"
Write-Host "诊断报告：$(Join-Path $OutputRoot "顶尖质量诊断报告.md")"
Write-Host "局部形态质量诊断报告：$(Join-Path $localShapeOutputRoot "局部形态质量诊断报告.md")"
