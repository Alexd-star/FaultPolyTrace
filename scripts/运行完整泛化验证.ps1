param(
    [string]$Python = "python",
    [string]$Executable = "",
    [int]$ControlledCaseCount = 80,
    [switch]$DownloadCracks,
    [switch]$SkipCracks,
    [switch]$WithPerturbations,
    [string]$ThebeSeismic = "",
    [string]$ThebeLabel = "",
    [int[]]$ThebeSamples = @(),
    [string]$ThebeStagingDir = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($Executable)) {
    $Executable = Join-Path $repoRoot "build\faultpolytrace.exe"
}
$Executable = [IO.Path]::GetFullPath($Executable)
if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "未找到 FaultPolyTrace 执行程序：$Executable。请先运行 scripts\build.ps1。"
}

$generatedRoot = Join-Path $repoRoot "data\generated"
$processedRoot = Join-Path $repoRoot "data\processed"
$externalRoot = Join-Path $repoRoot "data\external"
$outputRoot = Join-Path $repoRoot "examples\outputs"
$controlledData = Join-Path $generatedRoot "planar_benchmark_$ControlledCaseCount"
$controlledOutput = Join-Path $outputRoot "generalization_planar_$ControlledCaseCount"
$runner = Join-Path $PSScriptRoot "运行泛化验证.py"
$dataManager = Join-Path $PSScriptRoot "管理公开泛化数据.py"
$volumeConverter = Join-Path $PSScriptRoot "转换真实地震为平面属性.py"

New-Item -ItemType Directory -Force -Path $generatedRoot, $processedRoot, $externalRoot, $outputRoot | Out-Null

Write-Host "[1/3] 生成 $ControlledCaseCount 个平面可控真值案例。"
& $Python $runner generate --output $controlledData --count $ControlledCaseCount --width 256 --height 192 --seed 20260723
if ($LASTEXITCODE -ne 0) { throw "平面真值案例生成失败。" }

Write-Host "[2/3] 使用固定正式参数运行平面泛化验证。"
$controlledArgs = @(
    $runner, "run", "--dataset", $controlledData, "--exe", $Executable,
    "--output", $controlledOutput, "--boundary-tolerance", "2",
    "--centerline-tolerance", "3"
)
if ($WithPerturbations) { $controlledArgs += "--with-perturbations" }
& $Python @controlledArgs
if ($LASTEXITCODE -ne 0) { throw "平面泛化验证失败。" }

$cracksSource = Join-Path $externalRoot "cracks"
$cracksData = Join-Path $processedRoot "cracks_auxiliary"
$cracksOutput = Join-Path $outputRoot "generalization_cracks_expert_40"
$cracksRan = $false
if ($DownloadCracks) {
    Write-Host "正在下载并校验 CRACKS 公开数据。"
    & $Python $dataManager download --dataset cracks --output $cracksSource
    if ($LASTEXITCODE -ne 0) { throw "CRACKS 下载或校验失败。" }
}

if (-not $SkipCracks) {
    $imagesZip = Join-Path $cracksSource "images.zip"
    $labelsZip = Join-Path $cracksSource "Fault segmentations.zip"
    if ((Test-Path -LiteralPath $imagesZip) -and (Test-Path -LiteralPath $labelsZip)) {
        Write-Host "[3/3] 运行 CRACKS/F3 纵向剖面辅助压力测试。"
        & $Python $dataManager prepare-cracks --source $cracksSource --output $cracksData --annotator expert --limit 40
        if ($LASTEXITCODE -ne 0) { throw "CRACKS 辅助基准准备失败。" }
        & $Python $runner run --dataset $cracksData --exe $Executable --output $cracksOutput --boundary-tolerance 3 --centerline-tolerance 3
        if ($LASTEXITCODE -ne 0) { throw "CRACKS 辅助压力测试失败。" }
        $cracksRan = $true
    } else {
        Write-Warning "未发现 CRACKS 本地数据，已跳过公开真实剖面辅助测试。需要时添加 -DownloadCracks。"
    }
} else {
    Write-Host "[3/3] 已按参数跳过 CRACKS 辅助压力测试。"
}

$thebeRequested = (-not [string]::IsNullOrWhiteSpace($ThebeSeismic)) -or (-not [string]::IsNullOrWhiteSpace($ThebeLabel)) -or $ThebeSamples.Count -gt 0
if ($thebeRequested) {
    if ([string]::IsNullOrWhiteSpace($ThebeSeismic) -or [string]::IsNullOrWhiteSpace($ThebeLabel) -or $ThebeSamples.Count -eq 0) {
        throw "Thebe 验证必须同时提供 -ThebeSeismic、-ThebeLabel 和 -ThebeSamples。"
    }
    $resolvedSeismic = (Resolve-Path -LiteralPath $ThebeSeismic).Path
    $resolvedLabel = (Resolve-Path -LiteralPath $ThebeLabel).Path
    if ([string]::IsNullOrWhiteSpace($ThebeStagingDir)) {
        $ThebeStagingDir = Join-Path (Split-Path -Parent $resolvedSeismic) "faultpolytrace_staging"
    }
    $sourceName = [IO.Path]::GetFileNameWithoutExtension($resolvedSeismic)
    foreach ($sample in $ThebeSamples) {
        $caseName = "${sourceName}_sample${sample}_lag2"
        $caseData = Join-Path $processedRoot $caseName
        $caseOutput = Join-Path $outputRoot "generalization_$caseName"
        Write-Host "[Thebe] 转换并验证 sample $sample。"
        & $Python $volumeConverter --seismic $resolvedSeismic --seismic-key arr_0 --axis-order xys --sample-index $sample --half-window 5 --stepout 1 --max-vertical-lag 2 --label $resolvedLabel --label-key arr_0 --label-axis-order xys --staging-dir $ThebeStagingDir --output $caseData
        if ($LASTEXITCODE -ne 0) { throw "Thebe sample $sample 属性转换失败。" }
        & $Python $runner run --dataset $caseData --exe $Executable --output $caseOutput --boundary-tolerance 3 --centerline-tolerance 3
        if ($LASTEXITCODE -ne 0) { throw "Thebe sample $sample 泛化验证失败。" }
        Write-Host "Thebe 平面报告：$(Join-Path $caseOutput '泛化验证报告.md')"
    }
}
Write-Host "完整泛化验证结束。"
Write-Host "平面精确真值报告：$(Join-Path $controlledOutput '泛化验证报告.md')"
if ($cracksRan -and (Test-Path -LiteralPath (Join-Path $cracksOutput "泛化验证报告.md"))) {
    Write-Host "公开真实剖面辅助报告：$(Join-Path $cracksOutput '泛化验证报告.md')"
}
Write-Host "注意：公开纵向剖面不是平面赛题输入；Thebe 只覆盖一个真实工区，跨工区结论仍需第二套独立平面属性与专家解释验证。"
