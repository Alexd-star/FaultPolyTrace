param(
    [string]$Dat = $(if ($env:FPT_OFFICIAL_DAT) { $env:FPT_OFFICIAL_DAT } else { "" }),
    [string]$BaselineCsv = "",
    [string]$OutDir = "",
    [string]$CoreExe = "",
    [string]$Python = "python",
    [int]$MaxPolygonDelta = 25,
    [int]$Rows = 4,
    [int]$Cols = 6,
    [switch]$BuildCore,
    [switch]$SkipCore,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$entryScript = Join-Path $PSScriptRoot "冲顶候选集成入口.py"
$boundary = "candidate_not_default_geoeast_or_cpp"

if ([string]::IsNullOrWhiteSpace($Dat)) {
    throw "未指定输入数据。请传入 -Dat <数据文件.dat>，或设置环境变量 FPT_OFFICIAL_DAT。"
}
if (!(Test-Path -LiteralPath $Dat)) {
    throw "输入数据不存在：$Dat"
}
if (!(Test-Path -LiteralPath $entryScript)) {
    throw "缺少受控集成入口脚本：$entryScript"
}

if ([string]::IsNullOrWhiteSpace($CoreExe)) {
    $CoreExe = Join-Path $repoRoot "build\faultpolytrace.exe"
}

if ($BuildCore) {
    & (Join-Path $PSScriptRoot "build.ps1")
    if ($LASTEXITCODE -ne 0) {
        throw "正式执行程序构建失败。"
    }
}

if (!$SkipCore -and !(Test-Path -LiteralPath $CoreExe)) {
    throw "正式执行程序不存在：$CoreExe。请先运行 -BuildCore，或传入 -CoreExe。"
}

if ($SkipCore -and [string]::IsNullOrWhiteSpace($BaselineCsv)) {
    throw "使用 -SkipCore 时必须传入 -BaselineCsv，确保候选线从明确的正式基线出发。"
}
if (![string]::IsNullOrWhiteSpace($BaselineCsv) -and !(Test-Path -LiteralPath $BaselineCsv)) {
    throw "正式基线 CSV 不存在：$BaselineCsv"
}

if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $OutDir = Join-Path $repoRoot "examples\outputs\peak_candidate_integration_$stamp"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$env:PYTHONIOENCODING = "utf-8"
$argsList = @(
    $entryScript,
    "--dat", $Dat,
    "--out-dir", $OutDir,
    "--core-exe", $CoreExe,
    "--max-polygon-delta", [string]$MaxPolygonDelta,
    "--rows", [string]$Rows,
    "--cols", [string]$Cols
)

if (![string]::IsNullOrWhiteSpace($BaselineCsv)) {
    $argsList += @("--baseline-csv", $BaselineCsv)
}
if ($SkipCore) {
    $argsList += "--skip-core"
}
if ($DryRun) {
    $argsList += "--dry-run"
}

Write-Host "FaultPolyTrace 冲顶候选受控集成入口"
Write-Host "边界：" $boundary
Write-Host "可选参数：-BuildCore -SkipCore -DryRun"
Write-Host "输入数据：" $Dat
Write-Host "输出目录：" $OutDir
Write-Host "正式执行程序：" $CoreExe
if (![string]::IsNullOrWhiteSpace($BaselineCsv)) {
    Write-Host "复用正式基线：" $BaselineCsv
}

& $Python $argsList
if ($LASTEXITCODE -ne 0) {
    throw "冲顶候选受控集成入口运行失败。"
}

$manifest = Join-Path $OutDir "冲顶候选集成入口清单.json"
if (!(Test-Path -LiteralPath $manifest)) {
    throw "运行结束但未找到集成清单：$manifest"
}

Write-Host "集成清单：" $manifest
