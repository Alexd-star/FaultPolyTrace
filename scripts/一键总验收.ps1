param(
    [string]$GeoEastRoot = $(if ($env:FPT_GEOEAST_ROOT) { $env:FPT_GEOEAST_ROOT } elseif ($env:USERPROFILE -and (Test-Path -LiteralPath (Join-Path $env:USERPROFILE "GeoEast"))) { Join-Path $env:USERPROFILE "GeoEast" } else { "C:\GeoEast" }),
    [string]$OfficialDat = "",
    [string]$PackageVersion = "v1.1.1",
    [int]$MaxElapsedMs = 5000,
    [double]$MinStrongCoverage = 0.990,
    [double]$MinWeakCoverage = 0.955,
    [int]$ExpectedKeptPolygons = 200,
    [int]$MaxKeptPolygonDelta = 15,
    [int]$MaxStrongReviewComponents = 999999,
    [int]$MaxWeakLineReviewComponents = 999999,
    [int]$MaxHighRiskMergePolygons = 999999,
    [int]$MaxMiddleRiskMergePolygons = 999999,
    [int]$MaxLocalShapeCandidates = 999999,
    [int]$MaxHighPriorityLocalShapeCandidates = 999999,
    [switch]$RunParameterStabilityGate,
    [switch]$SkipGeoEastBuild,
    [switch]$SkipPackage
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "AcceptanceGate.ps1")

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$exe = Join-Path $repoRoot "build\faultpolytrace.exe"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$totalSteps = 15
$resultName = "official_root_antimerge"
$acceptanceName = "acceptance_${timestamp}_official_root_antimerge"
$acceptanceDir = Join-Path $repoRoot "examples\outputs\$acceptanceName"
$resultCsv = Join-Path $acceptanceDir "$resultName.csv"
$qualityDir = Join-Path $acceptanceDir "质量评估"
$missQcDir = Join-Path $acceptanceDir "漏检质控"
$topQualityDir = Join-Path $acceptanceDir "顶尖质量诊断"
$autoParamsDir = Join-Path $acceptanceDir "自动参数推荐"
$topReviewDir = Join-Path $acceptanceDir "冲顶局部复核"
$missRootCauseDir = Join-Path $acceptanceDir "漏检根因诊断"
$residualReviewLayerDir = Join-Path $acceptanceDir "残余复核层"
$localShapeQualityDir = Join-Path $acceptanceDir "局部形态质量诊断"
$parameterStabilityDir = Join-Path $acceptanceDir "参数扰动稳定性"
$lowerPerturbationCsv = Join-Path $parameterStabilityDir "stable_plateau_lower.csv"
$upperPerturbationCsv = Join-Path $parameterStabilityDir "stable_plateau_upper.csv"
$stabilityReportPath = Join-Path $parameterStabilityDir "参数扰动稳定性报告.md"
$stabilitySummaryPath = Join-Path $parameterStabilityDir "参数扰动稳定性摘要.json"
$stabilityPlateauPath = Join-Path $parameterStabilityDir "离散分位平台核验.json"
$reportPath = Join-Path $acceptanceDir "本次验收报告.md"
$stabilityGatePassed = $null
$stabilityArtifactsPresent = $false

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

function Get-StablePercentileProbe {
    param(
        [double]$Baseline,
        [double]$LowerInclusive,
        [double]$UpperExclusive,
        [ValidateSet("Lower", "Upper")][string]$Direction
    )
    foreach ($value in @($Baseline, $LowerInclusive, $UpperExclusive)) {
        if ([double]::IsNaN($value) -or [double]::IsInfinity($value)) {
            throw "分位数稳定区间包含非有限值。"
        }
    }
    if ($UpperExclusive -le $LowerInclusive -or
        $Baseline -lt $LowerInclusive -or $Baseline -ge $UpperExclusive) {
        throw "基线分位数不在诊断给出的稳定区间内。"
    }
    if ($Direction -eq "Lower") {
        return $LowerInclusive + 0.5 * ($Baseline - $LowerInclusive)
    }
    return $Baseline + 0.5 * ($UpperExclusive - $Baseline)
}

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

if (!(Test-Path -LiteralPath $OfficialDat)) {
    throw "官方测试数据不存在：$OfficialDat。请通过 -OfficialDat <test.dat> 或环境变量 FPT_OFFICIAL_DAT 指定。"
}

New-Item -ItemType Directory -Force -Path $acceptanceDir | Out-Null

Write-Host "== 1/$totalSteps 构建离线算法 =="
& (Join-Path $PSScriptRoot "build.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "离线算法构建失败。"
}

Write-Host "== 2/$totalSteps 运行基础烟测 =="
& (Join-Path $PSScriptRoot "run_smoke_test.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "基础烟测失败。"
}

Write-Host "== 3/$totalSteps 运行泛化鲁棒性验证 =="
python (Join-Path $PSScriptRoot "泛化鲁棒性验证.py") | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "泛化鲁棒性验证失败。"
}

Write-Host "== 4/$totalSteps 运行官方数据推荐参数 =="
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
    --simplify-tolerance 2 `
    --smooth-iterations 1 | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "官方数据推荐参数运行失败。"
}

if ($RunParameterStabilityGate) {
    Write-Host "== 4a/$totalSteps 运行参数扰动稳定性门禁 =="
    New-Item -ItemType Directory -Force -Path $parameterStabilityDir | Out-Null

    $baselineDiagnosticsPath = "$resultCsv.diagnostics.json"
    $baselineDiagnostics = Get-Content -LiteralPath $baselineDiagnosticsPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $primaryInfo = $baselineDiagnostics.parameters.primaryThresholdInfo
    $weakInfo = $baselineDiagnostics.parameters.weakThresholdInfo
    $baselinePrimaryPercentile = [double]$baselineDiagnostics.parameters.percentile
    $baselineWeakPercentile = [double]$baselineDiagnostics.parameters.weakPercentile
    $lowerPrimaryPercentile = Get-StablePercentileProbe `
        -Baseline $baselinePrimaryPercentile `
        -LowerInclusive ([double]$primaryInfo.stablePercentileLowerInclusive) `
        -UpperExclusive ([double]$primaryInfo.stablePercentileUpperExclusive) `
        -Direction Lower
    $upperPrimaryPercentile = Get-StablePercentileProbe `
        -Baseline $baselinePrimaryPercentile `
        -LowerInclusive ([double]$primaryInfo.stablePercentileLowerInclusive) `
        -UpperExclusive ([double]$primaryInfo.stablePercentileUpperExclusive) `
        -Direction Upper
    $lowerWeakPercentile = Get-StablePercentileProbe `
        -Baseline $baselineWeakPercentile `
        -LowerInclusive ([double]$weakInfo.stablePercentileLowerInclusive) `
        -UpperExclusive ([double]$weakInfo.stablePercentileUpperExclusive) `
        -Direction Lower
    $upperWeakPercentile = Get-StablePercentileProbe `
        -Baseline $baselineWeakPercentile `
        -LowerInclusive ([double]$weakInfo.stablePercentileLowerInclusive) `
        -UpperExclusive ([double]$weakInfo.stablePercentileUpperExclusive) `
        -Direction Upper
    $invariantCulture = [System.Globalization.CultureInfo]::InvariantCulture
    $lowerPrimaryArg = $lowerPrimaryPercentile.ToString("G17", $invariantCulture)
    $upperPrimaryArg = $upperPrimaryPercentile.ToString("G17", $invariantCulture)
    $lowerWeakArg = $lowerWeakPercentile.ToString("G17", $invariantCulture)
    $upperWeakArg = $upperWeakPercentile.ToString("G17", $invariantCulture)

    & $exe `
        --input $OfficialDat `
        --input-format geoeast-dat `
        --output $lowerPerturbationCsv `
        --trace-mode ridge-belt `
        --threshold-mode percentile `
        --percentile $lowerPrimaryArg `
        --min-area 120 `
        --close-radius 1 `
        --weak-percentile $lowerWeakArg `
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
        --simplify-tolerance 2 `
        --smooth-iterations 1 | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "较低分位参数扰动运行失败。"
    }

    & $exe `
        --input $OfficialDat `
        --input-format geoeast-dat `
        --output $upperPerturbationCsv `
        --trace-mode ridge-belt `
        --threshold-mode percentile `
        --percentile $upperPrimaryArg `
        --min-area 120 `
        --close-radius 1 `
        --weak-percentile $upperWeakArg `
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
        --simplify-tolerance 2 `
        --smooth-iterations 1 | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "较高分位参数扰动运行失败。"
    }

    python (Join-Path $PSScriptRoot "参数扰动稳定性验证.py") `
        --baseline "balanced=$resultCsv" `
        --candidate "stable_plateau_lower=$lowerPerturbationCsv" `
        --candidate "stable_plateau_upper=$upperPerturbationCsv" `
        --conservative $upperPerturbationCsv `
        --high-recall $lowerPerturbationCsv `
        --out-dir $parameterStabilityDir | Out-Host
    $comparisonExitCode = $LASTEXITCODE
    $stabilityArtifactsPresent = (
        (Test-Path -LiteralPath $stabilityReportPath) -and
        (Test-Path -LiteralPath $stabilitySummaryPath)
    )
    $baselineHash = (Get-FileHash -LiteralPath $resultCsv -Algorithm SHA256).Hash
    $lowerHash = (Get-FileHash -LiteralPath $lowerPerturbationCsv -Algorithm SHA256).Hash
    $upperHash = (Get-FileHash -LiteralPath $upperPerturbationCsv -Algorithm SHA256).Hash
    $plateauHashStable = ($baselineHash -eq $lowerHash) -and ($baselineHash -eq $upperHash)
    [ordered]@{
        schemaVersion = 1
        baseline = [ordered]@{
            primaryPercentile = $baselinePrimaryPercentile
            weakPercentile = $baselineWeakPercentile
            resultSha256 = $baselineHash
        }
        primaryStableInterval = [ordered]@{
            lowerInclusive = [double]$primaryInfo.stablePercentileLowerInclusive
            upperExclusive = [double]$primaryInfo.stablePercentileUpperExclusive
            effectiveThreshold = [double]$primaryInfo.threshold
        }
        weakStableInterval = [ordered]@{
            lowerInclusive = [double]$weakInfo.stablePercentileLowerInclusive
            upperExclusive = [double]$weakInfo.stablePercentileUpperExclusive
            effectiveThreshold = [double]$weakInfo.threshold
        }
        lowerProbe = [ordered]@{
            primaryPercentile = $lowerPrimaryPercentile
            weakPercentile = $lowerWeakPercentile
            resultSha256 = $lowerHash
        }
        upperProbe = [ordered]@{
            primaryPercentile = $upperPrimaryPercentile
            weakPercentile = $upperWeakPercentile
            resultSha256 = $upperHash
        }
        exactHashStable = $plateauHashStable
        note = "Cross-plateau percentile changes alter the effective attribute threshold and are sensitivity stress cases, not same-threshold stability probes."
    } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $stabilityPlateauPath -Encoding UTF8
    $stabilityGatePassed = ($comparisonExitCode -eq 0) -and $stabilityArtifactsPresent -and $plateauHashStable
} else {
    Write-Host "== 4a/$totalSteps 跳过可选参数扰动稳定性门禁 =="
}

Write-Host "== 5/$totalSteps 生成白底质量评估图 =="
python (Join-Path $PSScriptRoot "生成质量评估图.py") `
    --dat $OfficialDat `
    --runs-dir $acceptanceDir `
    --out-dir $qualityDir `
    --include-regex "^$([regex]::Escape($resultName))$" | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "质量评估图生成失败。"
}

Write-Host "== 6/$totalSteps 生成漏检质控图 =="
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

Write-Host "== 7/$totalSteps 生成顶尖质量诊断 =="
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

Write-Host "== 8/$totalSteps 生成自动参数推荐 =="
python (Join-Path $PSScriptRoot "自动参数推荐.py") `
    --dat $OfficialDat `
    --out-dir $autoParamsDir | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "自动参数推荐生成失败。"
}

Write-Host "== 9/$totalSteps 生成冲顶局部复核包 =="
python (Join-Path $PSScriptRoot "生成冲顶复核包.py") `
    --dat $OfficialDat `
    --csv $resultCsv `
    --top-quality-dir $topQualityDir `
    --out-dir $topReviewDir | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "冲顶局部复核包生成失败。"
}

Write-Host "== 10/$totalSteps 生成漏检根因诊断 =="
python (Join-Path $PSScriptRoot "漏检根因诊断.py") `
    --dat $OfficialDat `
    --csv $resultCsv `
    --out-dir $missRootCauseDir `
    --strong-threshold 0.95 `
    --weak-threshold 0.66 `
    --min-review-cells 8 `
    --min-line-aspect 3 | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "漏检根因诊断生成失败。"
}

Write-Host "== 11/$totalSteps 生成残余复核层 =="
python (Join-Path $PSScriptRoot "生成残余复核层.py") `
    --dat $OfficialDat `
    --csv $resultCsv `
    --out-dir $residualReviewLayerDir `
    --strong-threshold 0.95 `
    --weak-threshold 0.66 `
    --min-review-cells 8 `
    --min-line-aspect 3 `
    --max-local-figures 12 | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "残余复核层生成失败。"
}

Write-Host "== 12/$totalSteps 生成局部形态质量诊断 =="
python (Join-Path $PSScriptRoot "局部形态质量诊断.py") `
    --dat $OfficialDat `
    --csv $resultCsv `
    --out-dir $localShapeQualityDir `
    --width-ratio 2.35 `
    --min-extra-cells 24 `
    --window 11 `
    --max-figures 30 | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "局部形态质量诊断生成失败。"
}

Write-Host "== 13/$totalSteps 读取验收指标并生成报告 =="
$diagnosticsPath = "$resultCsv.diagnostics.json"
$diagnostics = Get-Content -LiteralPath $diagnosticsPath -Raw -Encoding UTF8 | ConvertFrom-Json
$qualityCsv = Join-Path $qualityDir "metrics\质量评估汇总.csv"
$qualityRows = Import-Csv -LiteralPath $qualityCsv
$quality = $qualityRows | Where-Object { $_.name -eq $resultName } | Select-Object -First 1
if (!$quality) {
    $availableNames = ($qualityRows | ForEach-Object { $_.name } | Where-Object { $_ }) -join ", "
    if ([string]::IsNullOrWhiteSpace($availableNames)) {
        $availableNames = "无"
    }
    throw "质量评估汇总未包含正式结果 '$resultName'。已有结果：$availableNames。请检查 include-regex 或结果 CSV 是否有效：$qualityCsv"
}
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
$residualReviewSummaryPath = Join-Path $residualReviewLayerDir "残余复核层摘要.json"
$residualReviewSummary = Get-Content -LiteralPath $residualReviewSummaryPath -Raw -Encoding UTF8 | ConvertFrom-Json
$residualReviewCandidateCount = [int]$residualReviewSummary.candidate_count
$localShapeSummaryPath = Join-Path $localShapeQualityDir "局部形态质量摘要.json"
$localShapeSummary = Get-Content -LiteralPath $localShapeSummaryPath -Raw -Encoding UTF8 | ConvertFrom-Json
$localShapeCandidateCount = [int]$localShapeSummary.candidate_count
$highPriorityLocalShapeCandidateCount = [int]$localShapeSummary.high_priority_count

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
$checks += [pscustomobject]@{ Item = "局部形态复核候选"; Pass = ($localShapeCandidateCount -le $MaxLocalShapeCandidates); Detail = "$localShapeCandidateCount 个，阈值 $MaxLocalShapeCandidates 个。该项用于定位局部鼓包、内环和长段宽带复核对象" }
$checks += [pscustomobject]@{ Item = "高优先级局部形态候选"; Pass = ($highPriorityLocalShapeCandidateCount -le $MaxHighPriorityLocalShapeCandidates); Detail = "$highPriorityLocalShapeCandidateCount 个，阈值 $MaxHighPriorityLocalShapeCandidates 个。该项用于跟踪黄框类局部分支/包络风险" }
if ($RunParameterStabilityGate) {
    $checks += [pscustomobject]@{
        Item = "参数扰动稳定性门禁"
        Pass = $stabilityGatePassed
        Detail = "在诊断给出的离散属性阈值稳定区间内分别向上、向下扰动，并要求结果 SHA256 完全一致；报告位于 $parameterStabilityDir"
    }
}

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
$report.Add("| 残余复核层候选 | $residualReviewCandidateCount |")
$report.Add("| 局部形态复核候选 | $localShapeCandidateCount |")
$report.Add("| 高优先级局部形态候选 | $highPriorityLocalShapeCandidateCount |")
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
$report.Add("root-anti-merge = true")
$report.Add("root-anti-merge-support-ratio = 0.25")
$report.Add("root-anti-merge-min-part-cells = 0")
$report.Add("root-anti-merge-min-part-ratio = 0.12")
$report.Add("residual-percentile = 0.955")
$report.Add("residual-fill-radius = 4")
$report.Add("residual-edge-fill-radius = 2")
$report.Add("residual-edge-fill-min-value-ratio = 0.97")
$report.Add("residual-edge-fill-max-patch-span = 5")
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
$report.Add("- 自动参数推荐报告：$autoParamsDir\自动参数推荐报告.md")
$report.Add("- 冲顶局部复核报告：$topReviewDir\冲顶局部复核报告.md")
$report.Add("- 冲顶局部复核图：$topReviewDir\局部复核图")
$report.Add("- 漏检根因诊断报告：$missRootCauseDir\漏检根因诊断报告.md")
$report.Add("- 漏检根因诊断明细：$missRootCauseDir\漏检根因诊断明细.csv")
$report.Add("- 残余复核层报告：$residualReviewLayerDir\残余复核层报告.md")
$report.Add("- 残余复核层 CSV：$residualReviewLayerDir\残余复核层.csv")
$report.Add("- 残余复核层 GeoJSON：$residualReviewLayerDir\残余复核层.geojson")
$report.Add("- 残余复核层图件：$residualReviewLayerDir\残余复核层总览.png")
$report.Add("- 局部形态质量报告：$localShapeQualityDir\局部形态质量诊断报告.md")
$report.Add("- 局部形态质量明细：$localShapeQualityDir\局部形态质量诊断.csv")
$report.Add("- 局部形态质量摘要：$localShapeQualityDir\局部形态质量摘要.json")
$report.Add("- 局部形态复核图：$localShapeQualityDir\局部形态复核图")
if ($RunParameterStabilityGate) {
    if ($stabilityArtifactsPresent) {
        $report.Add("- 参数扰动稳定性报告：$stabilityReportPath")
        $report.Add("- 参数扰动稳定性摘要：$stabilitySummaryPath")
        $report.Add("- 离散分位平台核验：$stabilityPlateauPath")
    } else {
        $report.Add("- 参数扰动稳定性报告：生成失败，未形成可引用证据文件")
    }
}
$report.Add("")
$report.Add("## 6. 说明")
$report.Add("")
$report.Add("本验收报告采用覆盖率、拓扑、耗时、漏检连通域、疑似误合并候选和局部形态候选共同评价结果质量。强响应覆盖率用于检查主要断层是否被追踪；弱响应覆盖率用于检查细弱线性响应的召回能力；拓扑指标用于确认输出是否满足闭合、无自相交、无异常交叉；疑似误合并候选用于检查相邻断层是否被错误合并；局部形态候选用于定位黄框复核中发现的局部鼓包、内环、长段宽带和疑似分支归属风险。局部形态候选不直接等同于确定错误，但它是后续局部瘦身、候选分裂和人工复核的入口。参数扰动稳定性只证明同一数据在邻近参数下的结果一致性，不使用专家标准答案，也不能单独证明地质解释正确。")

Set-Content -LiteralPath $reportPath -Value $report -Encoding UTF8
Write-Host "验收报告: $reportPath"

$deployAction = {
    if (!$SkipGeoEastBuild) {
        Write-Host "== 14/$totalSteps 构建并部署 GeoEast 组件 =="
        & (Join-Path $PSScriptRoot "build_geoeast_bo_runner.ps1") -GeoEastRoot $GeoEastRoot -UseVisualStudioProject
        & (Join-Path $PSScriptRoot "build_qt_launcher.ps1") -GeoEastRoot $GeoEastRoot
        & (Join-Path $PSScriptRoot "build_geoeast_mainframe_plugin.ps1") -GeoEastRoot $GeoEastRoot
        & (Join-Path $PSScriptRoot "部署到GeoEast.ps1") -GeoEastRoot $GeoEastRoot -SkipBuild
    } else {
        Write-Host "== 14/$totalSteps 跳过 GeoEast 构建部署 =="
    }
}.GetNewClosure()

$packageAction = {
    if (!$SkipPackage) {
        Write-Host "== 15/$totalSteps 生成发布包 =="
        & (Join-Path $PSScriptRoot "打包发布包.ps1") -Version $PackageVersion -GeoEastRoot $GeoEastRoot -SkipBuild
    } else {
        Write-Host "== 15/$totalSteps 跳过发布包生成 =="
    }
}.GetNewClosure()

Invoke-AcceptedDelivery `
    -Checks $checks `
    -ReportPath $reportPath `
    -DeployAction $deployAction `
    -PackageAction $packageAction

Write-Host "一键总验收通过。"
Write-Host "验收目录：$acceptanceDir"
Write-Host "验收报告：$reportPath"
if (!$SkipPackage) {
    Write-Host "发布包：$(Join-Path $repoRoot "release\FaultPolyTrace_$PackageVersion")"
    Write-Host "发布 ZIP：$(Join-Path $repoRoot "release\FaultPolyTrace_$PackageVersion.zip")"
}
