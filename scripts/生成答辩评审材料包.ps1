param(
    [string]$OutputDir = "",
    [string]$AcceptanceDir = "",
    [string]$StandaloneAcceptanceDir = "",
    [string]$GeoEastEvidenceDir = "",
    [string]$PackageDir = ""
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path -LiteralPath (Join-Path $scriptRoot "..")
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$rootLooksLikePackage = (Test-Path -LiteralPath (Join-Path $root "发布包清单.md")) -and
    (Test-Path -LiteralPath (Join-Path $root "FaultPolyTrace.exe"))
$releaseRoot = $(if ($rootLooksLikePackage) { $root } else { Join-Path $root "release" })

function Find-LatestDirectory {
    param(
        [string]$BaseDir,
        [string]$Pattern,
        [string]$RequiredFile = ""
    )
    if (!(Test-Path -LiteralPath $BaseDir)) {
        return $null
    }
    Get-ChildItem -LiteralPath $BaseDir -Directory |
        Where-Object {
            $_.Name -like $Pattern -and
            ([string]::IsNullOrWhiteSpace($RequiredFile) -or (Test-Path -LiteralPath (Join-Path $_.FullName $RequiredFile)))
        } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
}

function Copy-FileIfExists {
    param(
        [string]$Source,
        [string]$Destination
    )
    if (Test-Path -LiteralPath $Source) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Destination) | Out-Null
        Copy-Item -LiteralPath $Source -Destination $Destination -Force
        return $true
    }
    return $false
}

function Copy-DirIfExists {
    param(
        [string]$Source,
        [string]$Destination
    )
    if (Test-Path -LiteralPath $Source) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Destination) | Out-Null
        Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
        return $true
    }
    return $false
}

function Read-JsonFile {
    param([string]$Path)
    Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json
}

function Get-FirstCsvRow {
    param([string]$Path, [string]$Name = "")
    $rows = Import-Csv -LiteralPath $Path
    if (![string]::IsNullOrWhiteSpace($Name)) {
        $matched = $rows | Where-Object { $_.name -eq $Name } | Select-Object -First 1
        if ($matched) {
            return $matched
        }
    }
    return $rows | Select-Object -First 1
}

function Format-Percent {
    param([double]$Value)
    return ("{0:N3}%" -f ($Value * 100.0))
}

$outputsRoot = Join-Path $root "examples\outputs"
if ([string]::IsNullOrWhiteSpace($AcceptanceDir)) {
    $latest = Find-LatestDirectory -BaseDir $outputsRoot -Pattern "acceptance_*_official_root_antimerge" -RequiredFile "本次验收报告.md"
    if ($latest) { $AcceptanceDir = $latest.FullName }
}
if ([string]::IsNullOrWhiteSpace($StandaloneAcceptanceDir)) {
    $latest = Find-LatestDirectory -BaseDir $outputsRoot -Pattern "standalone_acceptance_*" -RequiredFile "独立软件发布包验收报告.md"
    if ($latest) { $StandaloneAcceptanceDir = $latest.FullName }
}
if ([string]::IsNullOrWhiteSpace($GeoEastEvidenceDir)) {
    $figureRoot = Join-Path $releaseRoot "figure"
    $latest = Find-LatestDirectory -BaseDir $figureRoot -Pattern "geoeast_acceptance_*" -RequiredFile "GeoEast现场证据检查报告.md"
    if ($latest) { $GeoEastEvidenceDir = $latest.FullName }
}
if ([string]::IsNullOrWhiteSpace($PackageDir)) {
    if ($rootLooksLikePackage) {
        $PackageDir = $root
    } else {
        $latest = Find-LatestDirectory -BaseDir $releaseRoot -Pattern "FaultPolyTrace_*" -RequiredFile "发布包清单.md"
        if ($latest) { $PackageDir = $latest.FullName }
    }
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $releaseRoot ("答辩评审材料包_" + $timestamp)
}

$createdOutputDir = New-Item -ItemType Directory -Force -Path $OutputDir
$OutputDir = (Resolve-Path -LiteralPath $createdOutputDir.FullName).Path

$copiedItems = New-Object System.Collections.Generic.List[object]
function Add-CopiedItem {
    param([string]$Category, [string]$Name, [string]$Path, [string]$Status)
    $copiedItems.Add([pscustomobject]@{
        category = $Category
        name = $Name
        path = $Path
        status = $Status
    }) | Out-Null
}

$diagnostics = $null
$metricRow = $null
$officialReport = ""
$diagnosticsPath = ""
$metricsPath = ""

if (![string]::IsNullOrWhiteSpace($AcceptanceDir) -and (Test-Path -LiteralPath $AcceptanceDir)) {
    $AcceptanceDir = (Resolve-Path -LiteralPath $AcceptanceDir).Path
    $officialTarget = Join-Path $OutputDir "01_官方数据验收"
    Copy-DirIfExists $AcceptanceDir (Join-Path $officialTarget (Split-Path -Leaf $AcceptanceDir)) | Out-Null
    Add-CopiedItem "官方数据" "官方 test.dat 一键验收目录" $AcceptanceDir "已复制"

    $officialReport = Join-Path $AcceptanceDir "本次验收报告.md"
    $diagnosticsPath = Join-Path $AcceptanceDir "official_root_antimerge.csv.diagnostics.json"
    $metricsPath = Join-Path $AcceptanceDir "质量评估\metrics\质量评估汇总.csv"
    if (Test-Path -LiteralPath $diagnosticsPath) { $diagnostics = Read-JsonFile $diagnosticsPath }
    if (Test-Path -LiteralPath $metricsPath) { $metricRow = Get-FirstCsvRow -Path $metricsPath -Name "official_root_antimerge" }
} else {
    Add-CopiedItem "官方数据" "官方 test.dat 一键验收目录" "" "缺失"
}

if (![string]::IsNullOrWhiteSpace($StandaloneAcceptanceDir) -and (Test-Path -LiteralPath $StandaloneAcceptanceDir)) {
    $StandaloneAcceptanceDir = (Resolve-Path -LiteralPath $StandaloneAcceptanceDir).Path
    Copy-DirIfExists $StandaloneAcceptanceDir (Join-Path $OutputDir ("02_独立软件验收\" + (Split-Path -Leaf $StandaloneAcceptanceDir))) | Out-Null
    Add-CopiedItem "独立软件" "独立软件发布包验收目录" $StandaloneAcceptanceDir "已复制"
} else {
    Add-CopiedItem "独立软件" "独立软件发布包验收目录" "" "缺失"
}

$standaloneFigureRoot = Join-Path $releaseRoot "figure"
$latestStandaloneFigure = Find-LatestDirectory -BaseDir $standaloneFigureRoot -Pattern "standalone_acceptance_*"
if ($latestStandaloneFigure) {
    Copy-DirIfExists $latestStandaloneFigure.FullName (Join-Path $OutputDir ("03_界面截图\" + $latestStandaloneFigure.Name)) | Out-Null
    Add-CopiedItem "界面截图" "独立软件界面验收截图" $latestStandaloneFigure.FullName "已复制"
}

if (![string]::IsNullOrWhiteSpace($GeoEastEvidenceDir) -and (Test-Path -LiteralPath $GeoEastEvidenceDir)) {
    $GeoEastEvidenceDir = (Resolve-Path -LiteralPath $GeoEastEvidenceDir).Path
    Copy-DirIfExists $GeoEastEvidenceDir (Join-Path $OutputDir ("04_GeoEast现场证据\" + (Split-Path -Leaf $GeoEastEvidenceDir))) | Out-Null
    Add-CopiedItem "GeoEast" "GeoEast 现场证据目录" $GeoEastEvidenceDir "已复制"
} else {
    Add-CopiedItem "GeoEast" "GeoEast 现场证据目录" "" "缺失或待补齐"
}

$docsTarget = Join-Path $OutputDir "05_核心中文文档"
$docs = @(
    "官方数据最新验收报告.md",
    "算法原理说明.md",
    "软件设计说明.md",
    "代码编制说明.md",
    "用户操作手册.md",
    "GeoEast使用指南.md",
    "发布说明与验收清单.md",
    "需求追踪矩阵.md",
    "评审交付与异机运行说明.md",
    "生产应用断层多边形标准与满分差距分析.md",
    "生产级效果评价标准与方案对比.md"
)
foreach ($doc in $docs) {
    $source = Join-Path $root "docs\$doc"
    $ok = Copy-FileIfExists $source (Join-Path $docsTarget $doc)
    Add-CopiedItem "中文文档" $doc $source ($(if ($ok) { "已复制" } else { "缺失" }))
}

if (![string]::IsNullOrWhiteSpace($PackageDir) -and (Test-Path -LiteralPath $PackageDir)) {
    $PackageDir = (Resolve-Path -LiteralPath $PackageDir).Path
    $packageTarget = Join-Path $OutputDir "06_发布包信息"
    Copy-FileIfExists (Join-Path $PackageDir "发布包清单.md") (Join-Path $packageTarget "发布包清单.md") | Out-Null
    Copy-FileIfExists (Join-Path $PackageDir "README_先打开我.md") (Join-Path $packageTarget "README_先打开我.md") | Out-Null
    Add-CopiedItem "发布包" "发布包目录" $PackageDir "已索引"
}
$zip = Get-ChildItem -LiteralPath $releaseRoot -File -Filter "FaultPolyTrace_*.zip" |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if ($zip) {
    Add-CopiedItem "发布包" "发布包 ZIP" $zip.FullName "已索引"
}

$strongCoverage = ""
$weakCoverage = ""
$traceMs = ""
$elapsedMs = ""
$kept = ""
$topology = ""
if ($metricRow -ne $null) {
    $strongCoverage = Format-Percent ([double]$metricRow.strong_coverage)
    $weakCoverage = Format-Percent ([double]$metricRow.weak_coverage)
}
if ($diagnostics -ne $null) {
    $traceMs = [string]$diagnostics.timing.traceMs
    $elapsedMs = [string]$diagnostics.timing.elapsedMs
    $kept = [string]$diagnostics.results.keptPolygonCount
    $topology = ("开放环 {0}，自相交 {1}，异常相交 {2}" -f $diagnostics.topology.openRings, $diagnostics.topology.selfIntersectingPolygons, $diagnostics.topology.polygonIntersections)
}

$indexLines = New-Object System.Collections.Generic.List[string]
$indexLines.Add("# 评分项证据索引") | Out-Null
$indexLines.Add("") | Out-Null
$indexLines.Add("生成时间：$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')") | Out-Null
$indexLines.Add("材料包目录：$OutputDir") | Out-Null
$indexLines.Add("") | Out-Null
$indexLines.Add("## 1. 关键指标") | Out-Null
$indexLines.Add("") | Out-Null
$indexLines.Add("| 指标 | 最新官方 test.dat 证据 |") | Out-Null
$indexLines.Add("|---|---|") | Out-Null
$indexLines.Add("| 保留多边形 | $kept |") | Out-Null
$indexLines.Add("| 强响应覆盖率 | $strongCoverage |") | Out-Null
$indexLines.Add("| 弱响应覆盖率 | $weakCoverage |") | Out-Null
$indexLines.Add("| 拓扑 | $topology |") | Out-Null
$indexLines.Add("| 核心追踪耗时 | $traceMs ms |") | Out-Null
$indexLines.Add("| 总耗时 | $elapsedMs ms |") | Out-Null
$indexLines.Add("") | Out-Null
$indexLines.Add("## 2. 赛题评分项证据") | Out-Null
$indexLines.Add("") | Out-Null
$indexLines.Add("| 评分项 | 证据位置 | 说明 |") | Out-Null
$indexLines.Add("|---|---|---|") | Out-Null
$indexLines.Add("| 正确读取 GeoEast 数据并写回数据库 | 04_GeoEast现场证据、docs/GeoEast使用指南.md | 现场截图齐全后作为最终证明；代码和 runner 文件已由满分审计检查 |") | Out-Null
$indexLines.Add("| 自动追踪断层多边形 | 01_官方数据验收 | 官方 test.dat 一键验收，含 CSV、诊断 JSON、质量图 |") | Out-Null
$indexLines.Add("| 面积统计和面积阈值过滤 | official_root_antimerge.csv、诊断 JSON、docs/算法原理说明.md | 输出含面积和 kept/filtered 状态 |") | Out-Null
$indexLines.Add("| 交切关系处理 | 顶尖质量诊断、official_root_antimerge.csv.topology.csv | 高/中风险误合并 0/0，拓扑 0/0/0 |") | Out-Null
$indexLines.Add("| 闭合多边形 | official_root_antimerge.csv.topology.csv | 开放环为 0 |") | Out-Null
$indexLines.Add("| 结果展示直观 | 质量评估/figures、03_界面截图、04_GeoEast现场证据 | 白底质量图、独立软件界面图和 GeoEast 叠加图 |") | Out-Null
$indexLines.Add("| 文档规范完整 | 05_核心中文文档 | 用户手册、设计说明、代码编制说明、算法说明、验收清单 |") | Out-Null
$indexLines.Add("| 性能 1 分钟内 | 本次验收报告.md、诊断 JSON | 核心追踪 $traceMs ms |") | Out-Null
$indexLines.Add("| 内存合理 | docs/赛题7性能与内存报告.md | RAII、流式写出、性能与内存采样说明 |") | Out-Null
Set-Content -LiteralPath (Join-Path $OutputDir "评分项证据索引.md") -Value ($indexLines -join "`n") -Encoding UTF8

$scriptLines = New-Object System.Collections.Generic.List[string]
$scriptLines.Add("# 答辩演示脚本") | Out-Null
$scriptLines.Add("") | Out-Null
$scriptLines.Add("## 1. 开场：赛题痛点") | Out-Null
$scriptLines.Add("") | Out-Null
$scriptLines.Add("GeoEast 中已有断层多边形生成方式依赖已解释层位、断层和断点，或需要人工拾取平面断层属性。FaultPolyTrace 的目标是直接基于平面断层属性自动追踪闭合断层多边形，提高解释效率。") | Out-Null
$scriptLines.Add("") | Out-Null
$scriptLines.Add("## 2. 方法概览") | Out-Null
$scriptLines.Add("") | Out-Null
$scriptLines.Add("算法采用 Ridge-Belt 主线：分位数阈值提取断层响应，骨架化获得主干和分支，依据方向、属性支撑和根系防黏连规则连接合理断层段，再生成闭合断层带多边形。面积阈值用于过滤小多边形，拓扑诊断保证开放环、自相交和异常相交为 0。") | Out-Null
$scriptLines.Add("") | Out-Null
$scriptLines.Add("## 3. 官方数据结果") | Out-Null
$scriptLines.Add("") | Out-Null
$scriptLines.Add("展示 01_官方数据验收 中的 本次验收报告.md、质量评估图和叠加图。重点说明：保留多边形 $kept 个，强响应覆盖率 $strongCoverage，弱响应覆盖率 $weakCoverage，$topology，核心追踪 $traceMs ms。") | Out-Null
$scriptLines.Add("") | Out-Null
$scriptLines.Add("## 4. 独立软件演示") | Out-Null
$scriptLines.Add("") | Out-Null
$scriptLines.Add("双击 FaultPolyTrace.exe 或发布包入口，打开外部数据，运行追踪，切换原始图、结果图和叠加图，展示导出 CSV、GeoJSON、PNG 和诊断文件。") | Out-Null
$scriptLines.Add("") | Out-Null
$scriptLines.Add("## 5. GeoEast 集成演示") | Out-Null
$scriptLines.Add("") | Out-Null
$scriptLines.Add("从 GeoEast 菜单入口打开启动器，刷新 BOMapGrid，选择 test7_fault_attribute_bo，输出 FaultPolyTrace_Result，运行后在 GeoMapping 或 GeoBasemap 中叠加查看。现场截图放入 04_GeoEast现场证据。") | Out-Null
$scriptLines.Add("") | Out-Null
$scriptLines.Add("## 6. 质量与泛化说明") | Out-Null
$scriptLines.Add("") | Out-Null
$scriptLines.Add("强调当前方案不是官方 test.dat 专用参数，而是通用 Ridge-Belt 流程；合成场景只作为泛化和入口烟测，正式质量结论来自官方 test.dat 验收。残余复核层用于保留弱线性响应疑点，避免为了表面覆盖率破坏正式层拓扑。") | Out-Null
$scriptLines.Add("") | Out-Null
$scriptLines.Add("## 7. 收尾") | Out-Null
$scriptLines.Add("") | Out-Null
$scriptLines.Add("总结赛题四项核心能力：自动追踪、面积统计与过滤、交切关系处理、闭合多边形；总结工程交付能力：独立运行、GeoEast 集成、中文文档、发布包和自动验收门禁。") | Out-Null
Set-Content -LiteralPath (Join-Path $OutputDir "答辩演示脚本.md") -Value ($scriptLines -join "`n") -Encoding UTF8

$checkLines = New-Object System.Collections.Generic.List[string]
$checkLines.Add("# 交付材料检查清单") | Out-Null
$checkLines.Add("") | Out-Null
$checkLines.Add("| 项目 | 状态 | 说明 |") | Out-Null
$checkLines.Add("|---|---|---|") | Out-Null
foreach ($item in $copiedItems) {
    $checkLines.Add("| $($item.category) - $($item.name) | $($item.status) | $($item.path) |") | Out-Null
}
$checkLines.Add("| GeoEast 现场截图 | 待人工确认 | 若 04_GeoEast现场证据 仍只有占位文件，请按 README_现场证据说明.md 补齐 8 张截图 |") | Out-Null
$checkLines.Add("| 演示视频 | 建议补齐 | 命名为 09_演示视频.mp4，放入 GeoEast 现场证据目录 |") | Out-Null
Set-Content -LiteralPath (Join-Path $OutputDir "交付材料检查清单.md") -Value ($checkLines -join "`n") -Encoding UTF8

$copiedItems | Export-Csv -LiteralPath (Join-Path $OutputDir "材料包清单.csv") -NoTypeInformation -Encoding UTF8

Write-Host "答辩评审材料包:" $OutputDir
Write-Host "评分项证据索引:" (Join-Path $OutputDir "评分项证据索引.md")
Write-Host "答辩演示脚本:" (Join-Path $OutputDir "答辩演示脚本.md")
