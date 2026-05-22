param(
    [string]$OfficialDat = "",
    [string]$OutputRoot = "",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$exe = Join-Path $repoRoot "build\faultpolytrace.exe"

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

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $OutputRoot = Join-Path $repoRoot "examples\outputs\quality_compare\compare_$timestamp"
}
$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

if (!$SkipBuild) {
    & (Join-Path $repoRoot "scripts\build.ps1")
    if ($LASTEXITCODE -ne 0) {
        throw "构建失败。"
    }
}

$baseArgs = @(
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
    "--residual-percentile", "0.955",
    "--residual-fill-radius", "4",
    "--residual-edge-fill-radius", "2",
    "--residual-min-cells", "2",
    "--residual-trace-min-length", "6",
    "--residual-trace-max-cells", "80",
    "--residual-covered-guard-radius", "0",
    "--no-residual-trace",
    "--simplify-tolerance", "2",
    "--smooth-iterations", "1"
)

$plans = @(
    @{
        Name = "01_balanced"
        Label = "均衡主线"
        Description = "覆盖、拓扑、粒度最稳定的正式基线；不把多边形数量作为目标，仅作为对照锚点。"
        Extra = @("--no-root-anti-merge", "--no-local-anti-merge")
    },
    @{
        Name = "02_root_antimerge"
        Label = "根系防黏连"
        Description = "沿同一根系才允许归并，重点减少近邻不同脉络黏连。"
        Extra = @("--root-anti-merge", "--root-anti-merge-support-ratio", "0.25", "--root-anti-merge-min-part-ratio", "0.12", "--no-local-anti-merge")
    },
    @{
        Name = "03_local_bridge"
        Label = "局部窄桥防黏连"
        Description = "在掩膜层切除低属性窄桥，仅用于验证局部桥接是否是主要根因。"
        Extra = @("--no-root-anti-merge", "--local-anti-merge")
    },
    @{
        Name = "04_root_plus_local"
        Label = "根系加局部窄桥"
        Description = "组合实验，检查两类防黏连是否互补或互相干扰。"
        Extra = @("--root-anti-merge", "--root-anti-merge-support-ratio", "0.25", "--root-anti-merge-min-part-ratio", "0.12", "--local-anti-merge")
    }
)

$summaryRows = @()
$baselineCsv = ""
foreach ($plan in $plans) {
    $planDir = Join-Path $OutputRoot $plan.Name
    New-Item -ItemType Directory -Force -Path $planDir | Out-Null
    $csv = Join-Path $planDir "result.csv"

    Write-Host "运行方案：$($plan.Label)"
    & $exe @baseArgs @($plan.Extra) "--output" $csv | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "方案运行失败：$($plan.Name)"
    }

    $diagnosisDir = Join-Path $planDir "quality_diagnosis"
    & (Join-Path $repoRoot "scripts\运行顶尖质量诊断.ps1") `
        -OfficialDat $OfficialDat `
        -ResultCsv $csv `
        -OutputRoot $diagnosisDir | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "质量诊断失败：$($plan.Name)"
    }

    $splitSummary = ""
    if ($plan.Name -eq "01_balanced") {
        $baselineCsv = $csv
    } elseif (![string]::IsNullOrWhiteSpace($baselineCsv)) {
        $diffDir = Join-Path $planDir "diff_from_balanced"
        python (Join-Path $repoRoot "scripts\防黏连差异诊断.py") `
            --dat $OfficialDat `
            --baseline-csv $baselineCsv `
            --candidate-csv $csv `
            --out-dir $diffDir `
            --max-figures 12 | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "防黏连差异诊断失败：$($plan.Name)"
        }
        $splitSummary = Join-Path $diffDir "防黏连差异摘要.json"
    }

    $localCandidateDir = Join-Path $planDir "local_candidate_check"
    $suspectCsv = Join-Path $diagnosisDir "疑似误合并多边形.csv"
    python (Join-Path $repoRoot "scripts\局部防黏连候选诊断.py") `
        --dat $OfficialDat `
        --csv $csv `
        --suspect-csv $suspectCsv `
        --out-dir $localCandidateDir `
        --max-figures 8 | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "局部防黏连候选诊断失败：$($plan.Name)"
    }
    $localSummary = Join-Path $localCandidateDir "局部防黏连候选摘要.json"

    $metricsPath = Join-Path $diagnosisDir "总体指标.json"
    $scoreJson = Join-Path $planDir "综合质量评分.json"
    $scoreReport = Join-Path $planDir "综合质量评分报告.md"
    $scoreArgs = @(
        (Join-Path $repoRoot "scripts\综合质量评分.py"),
        "--metrics", $metricsPath,
        "--csv", $csv,
        "--out-json", $scoreJson,
        "--out-report", $scoreReport
    )
    if (![string]::IsNullOrWhiteSpace($splitSummary)) {
        $scoreArgs += @("--split-summary", $splitSummary)
    }
    if (Test-Path -LiteralPath $localSummary) {
        $scoreArgs += @("--local-summary", $localSummary)
    }
    python @scoreArgs | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "综合评分失败：$($plan.Name)"
    }

    $score = Get-Content -LiteralPath $scoreJson -Raw -Encoding UTF8 | ConvertFrom-Json
    $summaryRows += [PSCustomObject]@{
        方案 = $plan.Label
        目录名 = $plan.Name
        说明 = $plan.Description
        综合评分 = [double]$score.total_score
        等级 = $score.level
        门禁通过 = $score.gates_passed
        多边形 = [int]$score.metrics.polygon_count
        强覆盖 = [double]$score.metrics.strong_coverage
        弱覆盖 = [double]$score.metrics.weak_coverage
        强复核 = [int]$score.metrics.strong_review_components
        弱复核 = [int]$score.metrics.weak_line_review_components
        高风险黏连 = [int]$score.metrics.high_risk_merge_polygons
        中风险黏连 = [int]$score.metrics.middle_risk_merge_polygons
        安全拆分修正 = [double]$score.subscores.split_benefit
        可行动桥风险 = [double]$score.subscores.local_candidate_risk
        小碎片 = [int]$score.metrics.small_polygon_count
        远背景包络 = [double]$score.metrics.boundary_background_ratio
        高远背景多边形 = [int]$score.metrics.large_background_polygon_count
        拓扑 = "$($score.metrics.open_rings)/$($score.metrics.self_intersections)/$($score.metrics.polygon_intersections)"
        核心耗时ms = [double]$score.metrics.trace_ms
        总耗时ms = [double]$score.metrics.elapsed_ms
        目录 = $planDir
    }
}

$summaryCsv = Join-Path $OutputRoot "方案质量对比.csv"
$summaryRows | Sort-Object -Property 综合评分 -Descending | Export-Csv -LiteralPath $summaryCsv -NoTypeInformation -Encoding UTF8

$report = Join-Path $OutputRoot "方案质量对比报告.md"
$lines = @(
    "# 方案质量对比报告",
    "",
    "## 1. 说明",
    "",
    "本报告不把固定多边形数量作为唯一标准，而是用覆盖率、漏检复核、疑似黏连、边界贴合、碎片粒度、拓扑和性能共同评价结果。多边形数量可以变化，但必须服务于解释质量。",
    "",
    "## 2. 对比结果",
    "",
    "| 方案 | 综合评分 | 等级 | 门禁 | 多边形 | 强覆盖 | 弱覆盖 | 高风险黏连 | 中风险黏连 | 安全拆分修正 | 可行动桥风险 | 小碎片 | 远背景包络 | 高远背景 | 拓扑 | 核心耗时 |",
    "|---|---:|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|"
)
foreach ($row in ($summaryRows | Sort-Object -Property 综合评分 -Descending)) {
    $gateText = if ($row.门禁通过) { "通过" } else { "未通过" }
    $lines += "| $($row.方案) | $('{0:N2}' -f $row.综合评分) | $($row.等级) | $gateText | $($row.多边形) | $('{0:P4}' -f $row.强覆盖) | $('{0:P4}' -f $row.弱覆盖) | $($row.高风险黏连) | $($row.中风险黏连) | $('{0:+0.00;-0.00;0.00}' -f $row.安全拆分修正) | $('{0:+0.00;-0.00;0.00}' -f $row.可行动桥风险) | $($row.小碎片) | $('{0:P2}' -f $row.远背景包络) | $($row.高远背景多边形) | $($row.拓扑) | $('{0:N0}' -f $row.核心耗时ms) ms |"
}
$lines += @(
    "",
    "## 3. 输出文件",
    "",
    "- 对比表：``$summaryCsv``",
    "- 输出根目录：``$OutputRoot``"
)
Set-Content -LiteralPath $report -Value ($lines -join "`n") -Encoding UTF8

Write-Host "方案质量对比目录：$OutputRoot"
Write-Host "对比表：$summaryCsv"
Write-Host "对比报告：$report"
