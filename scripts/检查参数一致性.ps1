[CmdletBinding()]
param(
    [switch]$StrictFixedParameters
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$reportDir = Join-Path $repoRoot "examples\outputs\parameter_consistency_$timestamp"
$reportPath = Join-Path $reportDir "参数一致性检查报告.md"

function Read-TextFile {
    param([string]$RelativePath)
    $path = Join-Path $repoRoot $RelativePath
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "缺少待检查文件：$path"
    }
    return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
}

function Test-TextContains {
    param(
        [string]$Text,
        [string]$Needle
    )
    return $Text.Contains($Needle)
}

function Add-Check {
    param(
        [System.Collections.Generic.List[object]]$Rows,
        [string]$Group,
        [string]$Name,
        [string]$Requirement,
        [bool]$Passed,
        [string]$Detail,
        [bool]$Required = $true
    )
    $Rows.Add([pscustomobject]@{
        Group = $Group
        Name = $Name
        Requirement = $Requirement
        Status = if ($Passed) { "通过" } elseif ($Required) { "失败" } else { "警告" }
        Detail = $Detail
        Required = $Required
    }) | Out-Null
}

function Add-ContainsCheck {
    param(
        [System.Collections.Generic.List[object]]$Rows,
        [string]$Group,
        [string]$Name,
        [string]$Requirement,
        [string]$Text,
        [string]$Needle,
        [string]$Detail,
        [bool]$Required = $true
    )
    Add-Check -Rows $Rows -Group $Group -Name $Name -Requirement $Requirement `
        -Passed (Test-TextContains -Text $Text -Needle $Needle) -Detail $Detail -Required $Required
}

function Escape-Md {
    param([string]$Text)
    return ($Text -replace "\|", "\|" -replace "`r?`n", " ")
}

$standalonePath = "standalone_app\qt_desktop\FaultPolyTraceWindow.cpp"
$geoEastPath = "geoeast_plugin\qt_launcher\FaultTraceDialog.cpp"
$coreCliPath = "src\app\main.cpp"
$boRunnerPath = "geoeast_plugin\bo_runner\main.cpp"
$sharedParameterPath = "src\ui\TraceParameterModel.cpp"
$packageScriptPath = "scripts\打包发布包.ps1"

$standalone = Read-TextFile $standalonePath
$geoEast = Read-TextFile $geoEastPath
$coreCli = Read-TextFile $coreCliPath
$boRunner = Read-TextFile $boRunnerPath
$sharedParameterModel = Read-TextFile $sharedParameterPath
$packageScript = Read-TextFile $packageScriptPath

$rows = [System.Collections.Generic.List[object]]::new()

Add-ContainsCheck -Rows $rows -Group "共享参数架构" -Name "独立软件参数委托" -Requirement "独立软件必须调用共享序列化器" -Text $standalone -Needle "fpt_ui::traceArguments(currentTraceParameters())" -Detail "$standalonePath 委托 TraceParameterModel"
Add-ContainsCheck -Rows $rows -Group "共享参数架构" -Name "GeoEast 控件参数委托" -Requirement "GeoEast 控件必须调用共享序列化器" -Text $geoEast -Needle "fpt_ui::traceArguments(currentTraceParameters())" -Detail "$geoEastPath 委托 TraceParameterModel"

$algorithmParams = @(
    @{ Key="traceMode"; Label="追踪模式"; Cli="--trace-mode"; Control="traceModeCombo_"; Type="value" },
    @{ Key="thresholdMode"; Label="阈值模式"; Cli="--threshold-mode"; Control="thresholdModeCombo_"; Type="value" },
    @{ Key="threshold"; Label="固定阈值"; Cli="--threshold"; Control="thresholdSpin_"; Type="value" },
    @{ Key="percentile"; Label="分位数"; Cli="--percentile"; Control="percentileSpin_"; Type="value" },
    @{ Key="minArea"; Label="最小面积"; Cli="--min-area"; Control="minAreaSpin_"; Type="value" },
    @{ Key="closeRadius"; Label="闭运算半径"; Cli="--close-radius"; Control="closeRadiusSpin_"; Type="value" },
    @{ Key="weakPercentile"; Label="弱延拓分位"; Cli="--weak-percentile"; Control="weakPercentileSpin_"; Type="value" },
    @{ Key="weakConnectRadius"; Label="弱延拓半径"; Cli="--weak-connect-radius"; Control="weakConnectRadiusSpin_"; Type="value" },
    @{ Key="beltRadius"; Label="带宽半径"; Cli="--belt-radius"; Control="beltRadiusSpin_"; Type="value" },
    @{ Key="postBeltCloseRadius"; Label="带后闭合"; Cli="--post-belt-close-radius"; Control="postBeltCloseRadiusSpin_"; Type="value" },
    @{ Key="attributeConstrainedBelt"; Label="属性约束断层带"; Cli="--attribute-constrained-belt"; NoCli="--no-attribute-constrained-belt"; Control="attributeConstrainedBeltCheck_"; Type="toggle" },
    @{ Key="attributeEnvelopeRadius"; Label="属性包络半径"; Cli="--attribute-envelope-radius"; Control="attributeEnvelopeRadiusSpin_"; Type="value" },
    @{ Key="minBranchLength"; Label="最短分支"; Cli="--min-branch-length"; Control="minBranchLengthSpin_"; Type="value" },
    @{ Key="minJunctionBranchLength"; Label="交切短枝"; Cli="--min-junction-branch-length"; Control="minJunctionBranchLengthSpin_"; Type="value" },
    @{ Key="minJunctionBranchMean"; Label="交切短枝均值"; Cli="--min-junction-branch-mean"; Control="minJunctionBranchMeanSpin_"; Type="value" },
    @{ Key="maxGapDistance"; Label="最大断距"; Cli="--max-gap-distance"; Control="maxGapDistanceSpin_"; Type="value" },
    @{ Key="maxGapAngle"; Label="最大夹角"; Cli="--max-gap-angle"; Control="maxGapAngleSpin_"; Type="value" },
    @{ Key="rootAntiMerge"; Label="根系防黏连"; Cli="--root-anti-merge"; NoCli="--no-root-anti-merge"; Control="rootAntiMergeCheck_"; Type="toggle" },
    @{ Key="rootAntiMergeRadius"; Label="根系半径"; Cli="--root-anti-merge-radius"; Control="rootAntiMergeRadiusSpin_"; Type="value" },
    @{ Key="rootAntiMergeSupportRatio"; Label="根系支撑比例"; Cli="--root-anti-merge-support-ratio"; Control="rootAntiMergeSupportRatioSpin_"; Type="value" },
    @{ Key="rootAntiMergeMinPartCells"; Label="根系主体点数"; Cli="--root-anti-merge-min-part-cells"; Control="rootAntiMergeMinPartCellsSpin_"; Type="value" },
    @{ Key="rootAntiMergeMinPartRatio"; Label="根系主体比例"; Cli="--root-anti-merge-min-part-ratio"; Control="rootAntiMergeMinPartRatioSpin_"; Type="value" },
    @{ Key="rootAntiMergeWeakGuardThreshold"; Label="弱保护阈值"; Cli="--root-anti-merge-weak-guard-threshold"; Control="rootAntiMergeWeakGuardThresholdSpin_"; Type="value" },
    @{ Key="rootAntiMergeWeakGuardRadius"; Label="弱保护半径"; Cli="--root-anti-merge-weak-guard-radius"; Control="rootAntiMergeWeakGuardRadiusSpin_"; Type="value" },
    @{ Key="localAntiMerge"; Label="局部防黏连"; Cli="--local-anti-merge"; NoCli="--no-local-anti-merge"; Control="localAntiMergeCheck_"; Type="toggle" },
    @{ Key="localAntiMergeLowThreshold"; Label="局部低值阈值"; Cli="--local-anti-merge-low-threshold"; Control="localAntiMergeLowThresholdSpin_"; Type="value" },
    @{ Key="localAntiMergeBridgeRadius"; Label="局部桥接半径"; Cli="--local-anti-merge-bridge-radius"; Control="localAntiMergeBridgeRadiusSpin_"; Type="value" },
    @{ Key="localAntiMergeMinPartCells"; Label="局部主体点数"; Cli="--local-anti-merge-min-part-cells"; Control="localAntiMergeMinPartCellsSpin_"; Type="value" },
    @{ Key="localAntiMergeMinPartRatio"; Label="局部主体比例"; Cli="--local-anti-merge-min-part-ratio"; Control="localAntiMergeMinPartRatioSpin_"; Type="value" },
    @{ Key="localAntiMergeMinBridgeCells"; Label="局部低桥点数"; Cli="--local-anti-merge-min-bridge-cells"; Control="localAntiMergeMinBridgeCellsSpin_"; Type="value" },
    @{ Key="localAntiMergeMinLowRatio"; Label="局部低值比例"; Cli="--local-anti-merge-min-low-ratio"; Control="localAntiMergeMinLowRatioSpin_"; Type="value" },
    @{ Key="residualTrace"; Label="残余二次追踪"; Cli="--residual-trace"; NoCli="--no-residual-trace"; Control="residualTraceCheck_"; Type="toggle" },
    @{ Key="residualTraceMinLength"; Label="补追最短"; Cli="--residual-trace-min-length"; Control="residualTraceMinLengthSpin_"; Type="value" },
    @{ Key="residualTraceMaxCells"; Label="补追上限"; Cli="--residual-trace-max-cells"; Control="residualTraceMaxCellsSpin_"; Type="value" },
    @{ Key="simplifyTolerance"; Label="简化容差"; Cli="--simplify-tolerance"; Control="simplifyToleranceSpin_"; Type="value" },
    @{ Key="smoothIterations"; Label="平滑次数"; Cli="--smooth-iterations"; Control="smoothIterationsSpin_"; Type="value" }
)

foreach ($paramSpec in $algorithmParams) {
    $key = [string]$paramSpec.Key
    $label = [string]$paramSpec.Label
    $cli = [string]$paramSpec.Cli
    $control = [string]$paramSpec.Control
    $noCli = if ($paramSpec.ContainsKey("NoCli")) { [string]$paramSpec.NoCli } else { "" }

    Add-ContainsCheck -Rows $rows -Group "独立软件参数" -Name $label -Requirement "界面控件存在" -Text $standalone -Needle $control -Detail "$standalonePath 包含 $control"
    Add-ContainsCheck -Rows $rows -Group "独立软件参数" -Name $label -Requirement "配置读取" -Text $standalone -Needle "settings.value(`"$key`"" -Detail "$standalonePath 读取 $key"
    Add-ContainsCheck -Rows $rows -Group "独立软件参数" -Name $label -Requirement "配置保存" -Text $standalone -Needle "settings.setValue(`"$key`"" -Detail "$standalonePath 保存 $key"
    Add-ContainsCheck -Rows $rows -Group "独立软件参数" -Name $label -Requirement "模板保存" -Text $standalone -Needle "obj[`"$key`"]" -Detail "$standalonePath 模板写入 $key"
    Add-ContainsCheck -Rows $rows -Group "独立软件参数" -Name $label -Requirement "模板读取" -Text $standalone -Needle "obj.value(`"$key`")" -Detail "$standalonePath 模板读取 $key"
    Add-ContainsCheck -Rows $rows -Group "共享参数序列化" -Name $label -Requirement "共享模型传入核心 CLI" -Text $sharedParameterModel -Needle $cli -Detail "$sharedParameterPath traceArguments 包含 $cli"

    if (![string]::IsNullOrWhiteSpace($noCli)) {
        Add-ContainsCheck -Rows $rows -Group "共享参数序列化" -Name $label -Requirement "共享模型传入关闭开关" -Text $sharedParameterModel -Needle $noCli -Detail "$sharedParameterPath traceArguments 包含 $noCli"
    }

    Add-ContainsCheck -Rows $rows -Group "GeoEast 控件参数" -Name $label -Requirement "界面控件存在" -Text $geoEast -Needle $control -Detail "$geoEastPath 包含 $control"
    Add-ContainsCheck -Rows $rows -Group "GeoEast 控件参数" -Name $label -Requirement "配置读取" -Text $geoEast -Needle "settings.value(`"$key`"" -Detail "$geoEastPath 读取 $key"
    Add-ContainsCheck -Rows $rows -Group "GeoEast 控件参数" -Name $label -Requirement "配置保存" -Text $geoEast -Needle "settings.setValue(`"$key`"" -Detail "$geoEastPath 保存 $key"
    Add-ContainsCheck -Rows $rows -Group "核心 CLI 解析" -Name $label -Requirement "离线核心解析参数" -Text $coreCli -Needle "arg == `"$cli`"" -Detail "$coreCliPath 解析 $cli"
    Add-ContainsCheck -Rows $rows -Group "BO Runner 解析" -Name $label -Requirement "GeoEast Runner 解析参数" -Text $boRunner -Needle "arg == `"$cli`"" -Detail "$boRunnerPath 解析 $cli"

    if (![string]::IsNullOrWhiteSpace($noCli)) {
        Add-ContainsCheck -Rows $rows -Group "核心 CLI 解析" -Name $label -Requirement "离线核心解析关闭开关" -Text $coreCli -Needle "arg == `"$noCli`"" -Detail "$coreCliPath 解析 $noCli"
        Add-ContainsCheck -Rows $rows -Group "BO Runner 解析" -Name $label -Requirement "GeoEast Runner 解析关闭开关" -Text $boRunner -Needle "arg == `"$noCli`"" -Detail "$boRunnerPath 解析 $noCli"
    }
}

$fixedParams = @(
    @{ Label="间隙支撑比例"; Cli="--min-gap-support-ratio" },
    @{ Label="禁止分支单独成带"; Cli="--no-separate-branch-belts" },
    @{ Label="根系背景桥最大值"; Cli="--root-anti-merge-max-bridge-value" },
    @{ Label="残余分位"; Cli="--residual-percentile" },
    @{ Label="残余填充半径"; Cli="--residual-fill-radius" },
    @{ Label="边界微补半径"; Cli="--residual-edge-fill-radius" },
    @{ Label="边界微补值比"; Cli="--residual-edge-fill-min-value-ratio" },
    @{ Label="边界微补跨度"; Cli="--residual-edge-fill-max-patch-span" },
    @{ Label="残余最少点数"; Cli="--residual-min-cells" },
    @{ Label="残余覆盖保护半径"; Cli="--residual-covered-guard-radius" }
)

foreach ($fixed in $fixedParams) {
    $label = [string]$fixed.Label
    $cli = [string]$fixed.Cli
    $required = [bool]$StrictFixedParameters
    Add-ContainsCheck -Rows $rows -Group "固定策略参数" -Name $label -Requirement "共享模型传参" -Text $sharedParameterModel -Needle $cli -Detail "$sharedParameterPath 固定传参 $cli" -Required $required
    Add-ContainsCheck -Rows $rows -Group "固定策略参数" -Name $label -Requirement "离线核心解析" -Text $coreCli -Needle "arg == `"$cli`"" -Detail "$coreCliPath 解析 $cli" -Required $required
    Add-ContainsCheck -Rows $rows -Group "固定策略参数" -Name $label -Requirement "BO Runner 解析" -Text $boRunner -Needle "arg == `"$cli`"" -Detail "$boRunnerPath 解析 $cli" -Required $required
}

$displayParams = @(
    @{ Key="color"; Label="独立软件结果颜色"; Control="colorEdit_"; Text=$standalone; Path=$standalonePath },
    @{ Key="lineWidth"; Label="独立软件结果线宽"; Control="lineWidthSpin_"; Text=$standalone; Path=$standalonePath },
    @{ Key="alpha"; Label="独立软件结果透明度"; Control="alphaSpin_"; Text=$standalone; Path=$standalonePath },
    @{ Key="polygonColor"; Label="GeoEast 多边形颜色"; Control="polygonColorEdit_"; Text=$geoEast; Path=$geoEastPath; Cli="--polygon-color" },
    @{ Key="polygonLineWidth"; Label="GeoEast 多边形线宽"; Control="polygonLineWidthSpin_"; Text=$geoEast; Path=$geoEastPath; Cli="--polygon-linewidth" }
)

foreach ($display in $displayParams) {
    $key = [string]$display.Key
    $label = [string]$display.Label
    $text = [string]$display.Text
    $control = [string]$display.Control
    $path = [string]$display.Path
    Add-ContainsCheck -Rows $rows -Group "显示与样式参数" -Name $label -Requirement "界面控件存在" -Text $text -Needle $control -Detail "$path 包含 $control"
    Add-ContainsCheck -Rows $rows -Group "显示与样式参数" -Name $label -Requirement "配置读取" -Text $text -Needle "settings.value(`"$key`"" -Detail "$path 读取 $key"
    Add-ContainsCheck -Rows $rows -Group "显示与样式参数" -Name $label -Requirement "配置保存" -Text $text -Needle "settings.setValue(`"$key`"" -Detail "$path 保存 $key"
    if ($display.ContainsKey("Cli")) {
        Add-ContainsCheck -Rows $rows -Group "显示与样式参数" -Name $label -Requirement "传给 GeoEast Runner" -Text $text -Needle ([string]$display.Cli) -Detail "$path traceArgs 包含 $($display.Cli)"
    }
}

Add-ContainsCheck -Rows $rows -Group "发布链路" -Name "参数一致性脚本" -Requirement "发布包携带检查脚本" -Text $packageScript -Needle "检查参数一致性.ps1" -Detail "$packageScriptPath 脚本清单包含 检查参数一致性.ps1"

$failed = @($rows | Where-Object { $_.Status -eq "失败" })
$warnings = @($rows | Where-Object { $_.Status -eq "警告" })
$passed = @($rows | Where-Object { $_.Status -eq "通过" })

New-Item -ItemType Directory -Force -Path $reportDir | Out-Null

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# 参数一致性检查报告") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("生成时间：$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## 检查目的") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("本报告用于核对独立桌面软件、GeoEast 集成控件、离线核心 CLI、GeoEast BO Runner 和发布包脚本之间的参数链路是否一致。它对应《独立软件与 GeoEast 插件全流程开发规划》的阶段 1：参数一致性治理。") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## 汇总") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| 项目 | 数量 |") | Out-Null
$lines.Add("|---|---:|") | Out-Null
$lines.Add("| 通过 | $($passed.Count) |") | Out-Null
$lines.Add("| 警告 | $($warnings.Count) |") | Out-Null
$lines.Add("| 失败 | $($failed.Count) |") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("说明：固定策略参数是算法内部策略常量，默认作为警告项展示；如需把它们也作为强制门禁，可使用 `-StrictFixedParameters`。") | Out-Null
$lines.Add("") | Out-Null

foreach ($group in ($rows | Select-Object -ExpandProperty Group -Unique)) {
    $lines.Add("## $group") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("| 参数 | 要求 | 状态 | 细节 |") | Out-Null
    $lines.Add("|---|---|---|---|") | Out-Null
    foreach ($row in ($rows | Where-Object { $_.Group -eq $group })) {
        $lines.Add("| $(Escape-Md $row.Name) | $(Escape-Md $row.Requirement) | $($row.Status) | $(Escape-Md $row.Detail) |") | Out-Null
    }
    $lines.Add("") | Out-Null
}

if ($failed.Count -gt 0) {
    $lines.Add("## 失败项处理建议") | Out-Null
    $lines.Add("") | Out-Null
    foreach ($row in $failed) {
        $lines.Add("- $($row.Group) / $($row.Name) / $($row.Requirement)：$($row.Detail)") | Out-Null
    }
    $lines.Add("") | Out-Null
}

[System.IO.File]::WriteAllText($reportPath, ($lines -join [Environment]::NewLine), [System.Text.Encoding]::UTF8)

Write-Host "参数一致性检查报告：$reportPath"
Write-Host "通过：$($passed.Count)  警告：$($warnings.Count)  失败：$($failed.Count)"

if ($failed.Count -gt 0) {
    exit 1
}
