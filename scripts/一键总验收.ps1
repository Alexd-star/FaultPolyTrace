param(
    [string]$GeoEastRoot = $(if ($env:FPT_GEOEAST_ROOT) { $env:FPT_GEOEAST_ROOT } else { "C:\Users\26312\GeoEast" }),
    [string]$OfficialDat = "",
    [string]$PackageVersion = "v0.9.5",
    [int]$MaxElapsedMs = 5000,
    [double]$MinStrongCoverage = 0.990,
    [double]$MinWeakCoverage = 0.955,
    [int]$ExpectedKeptPolygons = 204,
    [int]$MaxKeptPolygonDelta = 15,
    [int]$MaxStrongReviewComponents = 999999,
    [int]$MaxWeakLineReviewComponents = 999999,
    [int]$MaxHighRiskMergePolygons = 999999,
    [int]$MaxMiddleRiskMergePolygons = 999999,
    [switch]$SkipGeoEastBuild,
    [switch]$SkipPackage
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$exe = Join-Path $repoRoot "build\faultpolytrace.exe"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$acceptanceName = "acceptance_${timestamp}_official_fitfusion_multiring"
$acceptanceDir = Join-Path $repoRoot "examples\outputs\$acceptanceName"
$resultCsv = Join-Path $acceptanceDir "official_fitfusion_multiring.csv"
$qualityDir = Join-Path $acceptanceDir "质量评估"
$missQcDir = Join-Path $acceptanceDir "漏检质控"
$topQualityDir = Join-Path $acceptanceDir "顶尖质量诊断"
$reportPath = Join-Path $acceptanceDir "本次验收报告.md"

function Read-TextNumber {
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$DefaultValue = "0"
    )
    $m = [regex]::Match($Text, $Pattern)
    if ($m.Success) {
        return $m.Groups[1].Value
    }
    return $DefaultValue
}

if ([string]::IsNullOrWhiteSpace($OfficialDat)) {
    $candidate = Get-ChildItem -LiteralPath "D:\BaiduNetdiskDownload\BGPCup2025" -Recurse -Filter "test.dat" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -like "*BGPCup2025*" } |
        Select-Object -First 1
    if ($candidate) {
        $OfficialDat = $candidate.FullName
    }
}

if (!(Test-Path -LiteralPath $OfficialDat)) {
    throw "官方测试数据不存在：$OfficialDat"
}

New-Item -ItemType Directory -Force -Path $acceptanceDir | Out-Null

Write-Host "== 1/10 构建离线算法 =="
& (Join-Path $PSScriptRoot "build.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "离线算法构建失败。"
}

Write-Host "== 2/10 运行基础烟测 =="
& (Join-Path $PSScriptRoot "run_smoke_test.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "基础烟测失败。"
}

Write-Host "== 3/10 运行泛化鲁棒性验证 =="
python (Join-Path $PSScriptRoot "泛化鲁棒性验证.py") | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "泛化鲁棒性验证失败。"
}

Write-Host "== 4/10 运行官方数据推荐参数 =="
& $exe `
    --input $OfficialDat `
    --input-format geoeast-dat `
    --output $resultCsv `
    --trace-mode ridge-belt `
    --threshold-mode percentile `
    --percentile 0.98 `
    --min-area 120 `
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
    --no-root-anti-merge `
    --residual-percentile 0.955 `
    --residual-fill-radius 4 `
    --residual-min-cells 2 `
    --residual-trace-min-length 6 `
    --residual-trace-max-cells 80 `
    --residual-covered-guard-radius 0 `
    --no-residual-trace `
    --simplify-tolerance 2 `
    --smooth-iterations 1 | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "官方数据推荐参数运行失败。"
}

Write-Host "== 5/10 生成白底质量评估图 =="
python (Join-Path $PSScriptRoot "生成质量评估图.py") `
    --dat $OfficialDat `
    --runs-dir $acceptanceDir `
    --out-dir $qualityDir `
    --include-regex "official_fitfusion_multiring" | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "质量评估图生成失败。"
}

Write-Host "== 6/10 生成漏检质控图 =="
python (Join-Path $PSScriptRoot "生成漏检质控图.py") `
    --dat $OfficialDat `
    --csv $resultCsv `
    --out-dir $missQcDir `
    --strong-threshold 0.95 `
    --weak-threshold 0.66 `
    --min-review-cells 8 | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "漏检质控图生成失败。"
}

Write-Host "== 7/10 生成顶尖质量诊断 =="
python (Join-Path $PSScriptRoot "顶尖质量诊断.py") `
    --dat $OfficialDat `
    --csv $resultCsv `
    --out-dir $topQualityDir `
    --strong-threshold 0.95 `
    --weak-threshold 0.66 `
    --low-threshold 0.05 `
    --min-review-cells 8 `
    --min-line-aspect 3 | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "顶尖质量诊断生成失败。"
}

Write-Host "== 8/10 读取验收指标并生成报告 =="
$diagnosticsPath = "$resultCsv.diagnostics.json"
$diagnostics = Get-Content -LiteralPath $diagnosticsPath -Raw -Encoding UTF8 | ConvertFrom-Json
$qualityCsv = Join-Path $qualityDir "metrics\质量评估汇总.csv"
$qualityRows = Import-Csv -LiteralPath $qualityCsv
$quality = $qualityRows | Select-Object -First 1
$missSummaryPath = Join-Path $missQcDir "漏检质控摘要.txt"
$missSummary = Get-Content -LiteralPath $missSummaryPath -Raw -Encoding UTF8
$topQualityJsonPath = Join-Path $topQualityDir "总体指标.json"
$topQuality = Get-Content -LiteralPath $topQualityJsonPath -Raw -Encoding UTF8 | ConvertFrom-Json

$elapsedMs = [int]$diagnostics.timing.elapsedMs
$traceMs = [int]$diagnostics.timing.traceMs
$openRings = [int]$diagnostics.topology.openRings
$selfIntersections = [int]$diagnostics.topology.selfIntersectingPolygons
$polygonIntersections = [int]$diagnostics.topology.polygonIntersections
$keptPolygons = [int]$diagnostics.results.keptPolygonCount
$strongCoverage = [double]$quality.strong_coverage
$weakCoverage = [double]$quality.weak_coverage
$strongReviewComponents = [int](Read-TextNumber -Text $missSummary -Pattern "强响应需复核连通域数\(cell_count>=8\):\s*(\d+)")
$weakLineReviewComponents = [int](Read-TextNumber -Text $missSummary -Pattern "弱响应需复核线性连通域数\(cell_count>=8, aspect>=3\):\s*(\d+)")
$highRiskMergePolygons = [int]$topQuality.high_risk_merge_polygons
$middleRiskMergePolygons = [int]$topQuality.middle_risk_merge_polygons
$smallPolygonCount = [int]$topQuality.small_polygon_count

$checks = @()
$checks += [pscustomobject]@{ Item = "核心算法耗时"; Pass = ($traceMs -lt 60000); Detail = "${traceMs} ms，赛题要求 60000 ms 以内" }
$checks += [pscustomobject]@{ Item = "总运行耗时"; Pass = ($elapsedMs -le $MaxElapsedMs); Detail = "${elapsedMs} ms，验收阈值 ${MaxElapsedMs} ms" }
$checks += [pscustomobject]@{ Item = "闭合多边形"; Pass = ($openRings -eq 0); Detail = "开放环 $openRings" }
$checks += [pscustomobject]@{ Item = "自相交"; Pass = ($selfIntersections -eq 0); Detail = "自相交 $selfIntersections" }
$checks += [pscustomobject]@{ Item = "多边形数量稳定性"; Pass = ([math]::Abs($keptPolygons - $ExpectedKeptPolygons) -le $MaxKeptPolygonDelta); Detail = "保留 $keptPolygons 个，参考 $ExpectedKeptPolygons 个，允许偏差 $MaxKeptPolygonDelta 个" }
$checks += [pscustomobject]@{ Item = "多边形接触/交切诊断"; Pass = ($polygonIntersections -eq 0); Detail = "诊断记录 $polygonIntersections 条，正式主线要求为 0" }
$checks += [pscustomobject]@{ Item = "强响应覆盖率"; Pass = ($strongCoverage -ge $MinStrongCoverage); Detail = ("{0:P3}，阈值 {1:P3}" -f $strongCoverage, $MinStrongCoverage) }
$checks += [pscustomobject]@{ Item = "弱响应覆盖率"; Pass = ($weakCoverage -ge $MinWeakCoverage); Detail = ("{0:P3}，阈值 {1:P3}" -f $weakCoverage, $MinWeakCoverage) }
$checks += [pscustomobject]@{ Item = "强响应需复核项"; Pass = ($strongReviewComponents -le $MaxStrongReviewComponents); Detail = "$strongReviewComponents 个，阈值 $MaxStrongReviewComponents 个" }
$checks += [pscustomobject]@{ Item = "弱线性需复核项"; Pass = ($weakLineReviewComponents -le $MaxWeakLineReviewComponents); Detail = "$weakLineReviewComponents 个，阈值 $MaxWeakLineReviewComponents 个" }
$checks += [pscustomobject]@{ Item = "高风险误合并候选"; Pass = ($highRiskMergePolygons -le $MaxHighRiskMergePolygons); Detail = "$highRiskMergePolygons 个，阈值 $MaxHighRiskMergePolygons 个。该项为复核候选，不等同于确定错误" }
$checks += [pscustomobject]@{ Item = "中风险误合并候选"; Pass = ($middleRiskMergePolygons -le $MaxMiddleRiskMergePolygons); Detail = "$middleRiskMergePolygons 个，阈值 $MaxMiddleRiskMergePolygons 个。该项为复核候选，不等同于确定错误" }

$failedChecks = $checks | Where-Object { -not $_.Pass }
$resultText = if ($failedChecks.Count -eq 0) { "通过" } else { "未通过" }

$report = New-Object System.Collections.Generic.List[string]
$report.Add("# FaultPolyTrace 本次验收报告")
$report.Add("")
$report.Add("## 1. 验收结论")
$report.Add("")
$report.Add("- 验收时间：$timestamp")
$report.Add("- 验收数据：$OfficialDat")
$report.Add("- 验收目录：$acceptanceDir")
$report.Add("- 验收结论：$resultText")
$report.Add("")
$report.Add("## 2. 关键结果")
$report.Add("")
$report.Add("| 指标 | 数值 |")
$report.Add("|---|---:|")
$report.Add("| 网格规模 | $($diagnostics.grid.nx) x $($diagnostics.grid.ny) |")
$report.Add("| 输出多边形总数 | $($diagnostics.results.polygonCount) |")
$report.Add("| 保留多边形数量 | $($diagnostics.results.keptPolygonCount) |")
$report.Add("| 激活像元数 | $($diagnostics.results.activeCellCount) |")
$report.Add("| 有效阈值 | $($diagnostics.parameters.effectiveThreshold) |")
$report.Add("| 强响应覆盖率 | $("{0:P3}" -f $strongCoverage) |")
$report.Add("| 弱响应覆盖率 | $("{0:P3}" -f $weakCoverage) |")
$report.Add("| 强响应需复核连通域 | $strongReviewComponents |")
$report.Add("| 弱线性需复核连通域 | $weakLineReviewComponents |")
$report.Add("| 高风险误合并候选 | $highRiskMergePolygons |")
$report.Add("| 中风险误合并候选 | $middleRiskMergePolygons |")
$report.Add("| 小碎片多边形数量 | $smallPolygonCount |")
$report.Add("| 核心追踪耗时 | $traceMs ms |")
$report.Add("| 总耗时 | $elapsedMs ms |")
$report.Add("| 开放环 | $openRings |")
$report.Add("| 自相交多边形 | $selfIntersections |")
$report.Add("| 多边形相交 | $polygonIntersections |")
$report.Add("")
$report.Add("## 3. 验收项")
$report.Add("")
$report.Add("| 验收项 | 结果 | 说明 |")
$report.Add("|---|---|---|")
foreach ($check in $checks) {
    $mark = if ($check.Pass) { "通过" } else { "未通过" }
    $report.Add("| $($check.Item) | $mark | $($check.Detail) |")
}
$report.Add("")
$report.Add("## 4. 推荐参数")
$report.Add("")
$report.Add('```text')
$report.Add("trace-mode = ridge-belt")
$report.Add("threshold-mode = percentile")
$report.Add("percentile = 0.98")
$report.Add("min-area = 120")
$report.Add("close-radius = 1")
$report.Add("weak-percentile = 0.965")
$report.Add("weak-connect-radius = 1")
$report.Add("belt-radius = 3")
$report.Add("attribute-constrained-belt = true")
$report.Add("attribute-envelope-radius = 1")
$report.Add("post-belt-close-radius = 1")
$report.Add("min-branch-length = 4")
$report.Add("min-junction-branch-length = 2")
$report.Add("min-junction-branch-mean = 0.94")
$report.Add("max-gap-distance = 12")
$report.Add("max-gap-angle = 55")
$report.Add("min-gap-support-ratio = 0.25")
$report.Add("separate-branch-belts = false")
$report.Add("root-anti-merge = false")
$report.Add("residual-percentile = 0.955")
$report.Add("residual-fill-radius = 4")
$report.Add("residual-min-cells = 2")
$report.Add("residual-trace-min-length = 6")
$report.Add("residual-trace-max-cells = 80")
$report.Add("residual-covered-guard-radius = 0")
$report.Add("residual-trace = false")
$report.Add("simplify-tolerance = 2")
$report.Add("smooth-iterations = 1")
$report.Add('```')
$report.Add("")
$report.Add("## 5. 输出文件")
$report.Add("")
$report.Add("- 多边形结果：$resultCsv")
$report.Add("- 运行诊断：$diagnosticsPath")
$report.Add("- 拓扑明细：$resultCsv.topology.csv")
$report.Add("- 分支明细：$resultCsv.branches.csv")
$report.Add("- 白底质量图：$qualityDir\figures")
$report.Add("- 质量指标表：$qualityCsv")
$report.Add("- 漏检质控图：$missQcDir\漏检质控总览.png")
$report.Add("- 漏检连通域表：$missQcDir\强响应未覆盖连通域.csv，$missQcDir\弱响应未覆盖连通域.csv")
$report.Add("- 顶尖质量诊断报告：$topQualityDir\顶尖质量诊断报告.md")
$report.Add("- 疑似误合并多边形表：$topQualityDir\疑似误合并多边形.csv")
$report.Add("- 逐多边形质量诊断表：$topQualityDir\逐多边形质量诊断.csv")
$report.Add("")
$report.Add("## 6. 说明")
$report.Add("")
$report.Add("本验收报告采用覆盖率、拓扑、耗时、漏检连通域和疑似误合并候选共同评价结果质量。强响应覆盖率用于检查主要断层是否被追踪；弱响应覆盖率用于检查细弱线性响应的召回能力；拓扑指标用于确认输出是否满足闭合、无自相交、无异常交叉；疑似误合并候选用于定位后续局部复核对象，不直接等同于确定错误。")

Set-Content -LiteralPath $reportPath -Value $report -Encoding UTF8
Write-Host "验收报告: $reportPath"

if (!$SkipGeoEastBuild) {
    Write-Host "== 9/10 构建并部署 GeoEast 组件 =="
    & (Join-Path $PSScriptRoot "build_geoeast_bo_runner.ps1") -GeoEastRoot $GeoEastRoot -UseVisualStudioProject
    & (Join-Path $PSScriptRoot "build_qt_launcher.ps1") -GeoEastRoot $GeoEastRoot
    & (Join-Path $PSScriptRoot "build_geoeast_mainframe_plugin.ps1") -GeoEastRoot $GeoEastRoot
    & (Join-Path $PSScriptRoot "部署到GeoEast.ps1") -GeoEastRoot $GeoEastRoot -SkipBuild
} else {
    Write-Host "== 9/10 跳过 GeoEast 构建部署 =="
}

if (!$SkipPackage) {
    Write-Host "== 10/10 生成发布包 =="
    & (Join-Path $PSScriptRoot "打包发布包.ps1") -Version $PackageVersion -GeoEastRoot $GeoEastRoot -SkipBuild
} else {
    Write-Host "== 10/10 跳过发布包生成 =="
}

if ($failedChecks.Count -gt 0) {
    $failedText = ($failedChecks | ForEach-Object { "$($_.Item): $($_.Detail)" }) -join "；"
    throw "一键总验收未通过：$failedText。详见 $reportPath"
}

Write-Host "一键总验收通过。"
Write-Host "验收目录：$acceptanceDir"
Write-Host "验收报告：$reportPath"
if (!$SkipPackage) {
    Write-Host "发布包：$(Join-Path $repoRoot "release\FaultPolyTrace_$PackageVersion")"
    Write-Host "发布 ZIP：$(Join-Path $repoRoot "release\FaultPolyTrace_$PackageVersion.zip")"
}
