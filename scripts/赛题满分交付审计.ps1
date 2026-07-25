param(
    [string]$AcceptanceDir = "",
    [string]$ReleasePackageDir = "",
    [string]$StandaloneAcceptanceDir = "",
    [double]$MinStrongCoverage = 0.99,
    [double]$MinWeakCoverage = 0.955,
    [int]$MaxTraceMs = 60000,
    [int]$MaxElapsedMs = 8000,
    [int]$ReferenceKeptPolygons = 214,
    [int]$MaxKeptPolygonDelta = 25,
    [switch]$StrictManual
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path -LiteralPath (Join-Path $scriptRoot "..")
$outputsRoot = Join-Path $root "examples\outputs"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$auditDir = Join-Path $outputsRoot ("full_score_audit_" + $timestamp)
New-Item -ItemType Directory -Force -Path $auditDir | Out-Null

$script:auditResults = New-Object System.Collections.Generic.List[object]

function Add-Result {
    param(
        [string]$Category,
        [string]$Item,
        [string]$Status,
        [string]$Requirement,
        [string]$Evidence,
        [string]$Source = ""
    )
    $script:auditResults.Add([pscustomobject]@{
        category = $Category
        item = $Item
        status = $Status
        requirement = $Requirement
        evidence = $Evidence
        source = $Source
    }) | Out-Null
}

function Add-Pass {
    param([string]$Category, [string]$Item, [string]$Requirement, [string]$Evidence, [string]$Source = "")
    Add-Result -Category $Category -Item $Item -Status "通过" -Requirement $Requirement -Evidence $Evidence -Source $Source
}

function Add-Fail {
    param([string]$Category, [string]$Item, [string]$Requirement, [string]$Evidence, [string]$Source = "")
    Add-Result -Category $Category -Item $Item -Status "失败" -Requirement $Requirement -Evidence $Evidence -Source $Source
}

function Add-Manual {
    param([string]$Category, [string]$Item, [string]$Requirement, [string]$Evidence, [string]$Source = "")
    Add-Result -Category $Category -Item $Item -Status "待人工确认" -Requirement $Requirement -Evidence $Evidence -Source $Source
}

function Test-RequiredFile {
    param([string]$Category, [string]$Item, [string]$Path, [int64]$MinBytes = 1)
    if ((Test-Path -LiteralPath $Path) -and ((Get-Item -LiteralPath $Path).Length -ge $MinBytes)) {
        Add-Pass $Category $Item "文件存在且非空" $Path $Path
        return $true
    }
    Add-Fail $Category $Item "文件存在且非空" "缺少或过小：$Path" $Path
    return $false
}

function Test-RequiredAnyFile {
    param([string]$Category, [string]$Item, [string[]]$Paths, [int64]$MinBytes = 1)
    foreach ($path in $Paths) {
        if ((Test-Path -LiteralPath $path) -and ((Get-Item -LiteralPath $path).Length -ge $MinBytes)) {
            Add-Pass $Category $Item "任一合法位置存在且非空" $path $path
            return $true
        }
    }
    Add-Fail $Category $Item "任一合法位置存在且非空" ("缺少或过小：" + ($Paths -join "；")) ($Paths -join ";")
    return $false
}

function Test-RequiredDir {
    param([string]$Category, [string]$Item, [string]$Path)
    if (Test-Path -LiteralPath $Path) {
        Add-Pass $Category $Item "目录存在" $Path $Path
        return $true
    }
    Add-Fail $Category $Item "目录存在" "缺少目录：$Path" $Path
    return $false
}

function Find-LatestDirectory {
    param(
        [string]$BaseDir,
        [string]$Pattern,
        [string]$RequiredFile = ""
    )
    if (!(Test-Path -LiteralPath $BaseDir)) {
        return $null
    }
    return Get-ChildItem -LiteralPath $BaseDir -Directory |
        Where-Object {
            $_.Name -like $Pattern -and
            ([string]::IsNullOrWhiteSpace($RequiredFile) -or (Test-Path -LiteralPath (Join-Path $_.FullName $RequiredFile)))
        } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
}

function Format-Percent {
    param([double]$Value)
    return ("{0:N3}%" -f ($Value * 100.0))
}

function Read-JsonFile {
    param([string]$Path)
    return Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json
}

function Get-FirstCsvRow {
    param([string]$Path, [string]$Name = "")
    $rows = Import-Csv -LiteralPath $Path
    if (![string]::IsNullOrWhiteSpace($Name)) {
        $matched = $rows | Where-Object { $_.name -eq $Name } | Select-Object -First 1
        if ($matched) { return $matched }
    }
    return $rows | Select-Object -First 1
}

function Find-FileByStem {
    param([string]$Dir, [string]$Stem, [string[]]$Extensions, [int64]$MinBytes = 1)
    foreach ($ext in $Extensions) {
        $candidate = Join-Path $Dir ($Stem + $ext)
        if ((Test-Path -LiteralPath $candidate) -and ((Get-Item -LiteralPath $candidate).Length -ge $MinBytes)) {
            return (Get-Item -LiteralPath $candidate).FullName
        }
    }
    return ""
}

function Get-GeoEastEvidence {
    $figureRoots = @(
        (Join-Path $root "release\figure"),
        (Join-Path $root "figure")
    )
    $figureRoot = $figureRoots | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($figureRoot)) {
        return [pscustomobject]@{
            dir = ""
            screenshotsReady = $false
            videoReady = $false
            missing = @("未找到 release\figure 或 figure 目录")
            menu = ""
            launcher = ""
            grid = ""
            original = ""
            result = ""
            overlay = ""
            database = ""
            log = ""
            video = ""
        }
    }

    $evidenceDir = Get-ChildItem -LiteralPath $figureRoot -Directory |
        Where-Object { $_.Name -like "geoeast_acceptance_*" } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (!$evidenceDir) {
        return [pscustomobject]@{
            dir = ""
            screenshotsReady = $false
            videoReady = $false
            missing = @("未找到 geoeast_acceptance_* 现场证据目录")
            menu = ""
            launcher = ""
            grid = ""
            original = ""
            result = ""
            overlay = ""
            database = ""
            log = ""
            video = ""
        }
    }

    $imageExts = @(".png", ".jpg", ".jpeg", ".bmp")
    $videoExts = @(".mp4", ".mkv", ".avi", ".mov", ".wmv")
    $items = @(
        @{ Key = "menu"; Stem = "01_GeoEast菜单入口"; Name = "GeoEast 菜单入口截图" },
        @{ Key = "launcher"; Stem = "02_启动器自动配置"; Name = "启动器自动配置截图" },
        @{ Key = "grid"; Stem = "03_输入网格列表"; Name = "输入网格列表截图" },
        @{ Key = "original"; Stem = "04_原始网格显示"; Name = "原始网格显示截图" },
        @{ Key = "result"; Stem = "05_结果多边形图层"; Name = "结果多边形图层截图" },
        @{ Key = "overlay"; Stem = "06_原始与结果叠加"; Name = "原始与结果叠加截图" },
        @{ Key = "database"; Stem = "07_数据库对象管理"; Name = "数据库对象管理截图" },
        @{ Key = "log"; Stem = "08_运行日志与耗时"; Name = "运行日志与耗时截图" }
    )

    $files = @{}
    $missing = @()
    foreach ($item in $items) {
        $file = Find-FileByStem -Dir $evidenceDir.FullName -Stem $item.Stem -Extensions $imageExts -MinBytes 10000
        $files[$item.Key] = $file
        if ([string]::IsNullOrWhiteSpace($file)) {
            $missing += $item.Name
        }
    }
    $videoFile = Find-FileByStem -Dir $evidenceDir.FullName -Stem "09_演示视频" -Extensions $videoExts -MinBytes 100000

    return [pscustomobject]@{
        dir = $evidenceDir.FullName
        screenshotsReady = ($missing.Count -eq 0)
        videoReady = ![string]::IsNullOrWhiteSpace($videoFile)
        missing = $missing
        menu = $files["menu"]
        launcher = $files["launcher"]
        grid = $files["grid"]
        original = $files["original"]
        result = $files["result"]
        overlay = $files["overlay"]
        database = $files["database"]
        log = $files["log"]
        video = $videoFile
    }
}

if ([string]::IsNullOrWhiteSpace($AcceptanceDir)) {
    $latestAcceptance = Find-LatestDirectory -BaseDir $outputsRoot -Pattern "acceptance_*_official_root_antimerge" -RequiredFile "本次验收报告.md"
    if ($latestAcceptance) {
        $AcceptanceDir = $latestAcceptance.FullName
    }
}

if ([string]::IsNullOrWhiteSpace($ReleasePackageDir)) {
    $rootLooksLikePackage = (Test-Path -LiteralPath (Join-Path $root "发布包清单.md")) -and
        (Test-Path -LiteralPath (Join-Path $root "FaultPolyTrace.exe"))
    if ($rootLooksLikePackage) {
        $ReleasePackageDir = $root
    } else {
        $releaseRoot = Join-Path $root "release"
        $latestPackage = Find-LatestDirectory -BaseDir $releaseRoot -Pattern "FaultPolyTrace_*" -RequiredFile "发布包清单.md"
        if ($latestPackage) {
            $ReleasePackageDir = $latestPackage.FullName
        }
    }
}

if ([string]::IsNullOrWhiteSpace($StandaloneAcceptanceDir)) {
    $latestStandalone = Find-LatestDirectory -BaseDir $outputsRoot -Pattern "standalone_acceptance_*" -RequiredFile "独立软件发布包验收报告.md"
    if ($latestStandalone) {
        $StandaloneAcceptanceDir = $latestStandalone.FullName
    }
}

$officialDataPath = ""
$diagnostics = $null
$metricRow = $null
$residualSummary = $null

$acceptanceDirMissing = [string]::IsNullOrWhiteSpace($AcceptanceDir)
$acceptanceDirExists = $false
if (!$acceptanceDirMissing) {
    $acceptanceDirExists = Test-Path -LiteralPath $AcceptanceDir
}

$useAcceptanceDir = ((!$acceptanceDirMissing) -and $acceptanceDirExists)

if ($useAcceptanceDir) {
    $AcceptanceDir = (Resolve-Path -LiteralPath $AcceptanceDir).Path
    Add-Pass "官方 DAT 验收" "最新官方验收目录" "必须存在 acceptance_*_official_root_antimerge" $AcceptanceDir $AcceptanceDir

    $acceptanceReport = Join-Path $AcceptanceDir "本次验收报告.md"
    Test-RequiredFile "官方 DAT 验收" "本次验收报告" $acceptanceReport 1000 | Out-Null
    if (Test-Path -LiteralPath $acceptanceReport) {
        $reportText = Get-Content -LiteralPath $acceptanceReport -Raw -Encoding UTF8
        $officialDataMatch = [regex]::Match($reportText, '验收数据：(.+test\.dat)')
        if ($officialDataMatch.Success) {
            $officialDataPath = $officialDataMatch.Groups[1].Value.Trim()
        }
        if ($reportText -match "验收结论：通过") {
            Add-Pass "官方 DAT 验收" "一键验收结论" "验收报告必须显示通过" "报告显示：验收结论：通过" $acceptanceReport
        } else {
            Add-Fail "官方 DAT 验收" "一键验收结论" "验收报告必须显示通过" "报告未显示“验收结论：通过”" $acceptanceReport
        }
    }

    $diagnosticsPath = Join-Path $AcceptanceDir "official_root_antimerge.csv.diagnostics.json"
    if (Test-RequiredFile "官方 DAT 验收" "运行诊断 JSON" $diagnosticsPath 500) {
        $diagnostics = Read-JsonFile $diagnosticsPath
    }

    $metricsPath = Join-Path $AcceptanceDir "质量评估\metrics\质量评估汇总.csv"
    if (Test-RequiredFile "官方 DAT 验收" "质量评估汇总 CSV" $metricsPath 100) {
        $metricRow = Get-FirstCsvRow -Path $metricsPath -Name "official_root_antimerge"
    }

    $residualPath = Join-Path $AcceptanceDir "残余复核层\残余复核层摘要.json"
    if (Test-RequiredFile "官方 DAT 验收" "残余复核层摘要" $residualPath 50) {
        $residualSummary = Read-JsonFile $residualPath
    }
} else {
    Add-Fail "官方 DAT 验收" "最新官方验收目录" "必须存在 acceptance_*_official_root_antimerge" "未找到官方 DAT 验收目录。请先运行 scripts/一键总验收.ps1。"
}

if ($diagnostics -ne $null) {
    $grid = "{0} x {1}" -f $diagnostics.grid.nx, $diagnostics.grid.ny
    if ($diagnostics.grid.nx -eq 1501 -and $diagnostics.grid.ny -eq 701) {
        Add-Pass "官方 DAT 验收" "官方网格规模" "官方 test.dat 应为 1501 x 701" $grid
    } else {
        Add-Fail "官方 DAT 验收" "官方网格规模" "官方 test.dat 应为 1501 x 701" $grid
    }

    $kept = [int]$diagnostics.results.keptPolygonCount
    $delta = [math]::Abs($kept - $ReferenceKeptPolygons)
    if ($delta -le $MaxKeptPolygonDelta) {
        Add-Pass "核心算法质量" "多边形粒度稳定性" "保留数量接近参考 $ReferenceKeptPolygons，允许偏差 $MaxKeptPolygonDelta" "保留 $kept，偏差 $delta"
    } else {
        Add-Fail "核心算法质量" "多边形粒度稳定性" "保留数量接近参考 $ReferenceKeptPolygons，允许偏差 $MaxKeptPolygonDelta" "保留 $kept，偏差 $delta"
    }

    $traceMs = [int]$diagnostics.timing.traceMs
    $elapsedMs = [int]$diagnostics.timing.elapsedMs
    if ($traceMs -le $MaxTraceMs) {
        Add-Pass "性能指标" "核心追踪耗时" "核心处理必须小于 $MaxTraceMs ms" "$traceMs ms"
    } else {
        Add-Fail "性能指标" "核心追踪耗时" "核心处理必须小于 $MaxTraceMs ms" "$traceMs ms"
    }
    if ($elapsedMs -le $MaxElapsedMs) {
        Add-Pass "性能指标" "总运行耗时" "当前验收总耗时不超过 $MaxElapsedMs ms" "$elapsedMs ms"
    } else {
        Add-Fail "性能指标" "总运行耗时" "当前验收总耗时不超过 $MaxElapsedMs ms" "$elapsedMs ms"
    }

    $openRings = [int]$diagnostics.topology.openRings
    $selfIntersections = [int]$diagnostics.topology.selfIntersectingPolygons
    $polygonIntersections = [int]$diagnostics.topology.polygonIntersections
    if ($openRings -eq 0) { Add-Pass "拓扑质量" "闭合多边形" "开放环必须为 0" "开放环 $openRings" } else { Add-Fail "拓扑质量" "闭合多边形" "开放环必须为 0" "开放环 $openRings" }
    if ($selfIntersections -eq 0) { Add-Pass "拓扑质量" "自相交" "自相交多边形必须为 0" "自相交 $selfIntersections" } else { Add-Fail "拓扑质量" "自相交" "自相交多边形必须为 0" "自相交 $selfIntersections" }
    if ($polygonIntersections -eq 0) { Add-Pass "拓扑质量" "多边形异常相交" "异常相交必须为 0" "相交 $polygonIntersections" } else { Add-Fail "拓扑质量" "多边形异常相交" "相交必须为 0" "相交 $polygonIntersections" }

    if ($diagnostics.parameters.rootAntiMerge -eq $true) {
        Add-Pass "核心算法质量" "根系防黏连默认主线" "官方冲顶方案必须启用 rootAntiMerge" "rootAntiMerge=true"
    } else {
        Add-Fail "核心算法质量" "根系防黏连默认主线" "官方冲顶方案必须启用 rootAntiMerge" "rootAntiMerge=false"
    }

    if ($diagnostics.parameters.residualTrace -eq $false) {
        Add-Pass "核心算法质量" "残余线性响应处理策略" "正式层不应无脑吞入残余响应" "residualTrace=false，残余作为复核层输出"
    } else {
        Add-Fail "核心算法质量" "残余线性响应处理策略" "正式层不应无脑吞入残余响应" "residualTrace=true"
    }
}

if ($metricRow -ne $null) {
    $strongCoverage = [double]$metricRow.strong_coverage
    $weakCoverage = [double]$metricRow.weak_coverage
    if ($strongCoverage -ge $MinStrongCoverage) {
        Add-Pass "核心算法质量" "强响应覆盖率" "强响应覆盖率 >= $(Format-Percent $MinStrongCoverage)" "$(Format-Percent $strongCoverage)"
    } else {
        Add-Fail "核心算法质量" "强响应覆盖率" "强响应覆盖率 >= $(Format-Percent $MinStrongCoverage)" "$(Format-Percent $strongCoverage)"
    }
    if ($weakCoverage -ge $MinWeakCoverage) {
        Add-Pass "核心算法质量" "弱响应覆盖率" "弱响应覆盖率 >= $(Format-Percent $MinWeakCoverage)" "$(Format-Percent $weakCoverage)"
    } else {
        Add-Fail "核心算法质量" "弱响应覆盖率" "弱响应覆盖率 >= $(Format-Percent $MinWeakCoverage)" "$(Format-Percent $weakCoverage)"
    }
}

if ($residualSummary -ne $null) {
    if ([int]$residualSummary.candidate_count -ge 0) {
        Add-Pass "核心算法质量" "残余复核层" "必须对未纳入正式层的线性残余给出可解释复核层" ("候选 {0}，强覆盖 {1}，弱覆盖 {2}" -f $residualSummary.candidate_count, (Format-Percent ([double]$residualSummary.strong_coverage)), (Format-Percent ([double]$residualSummary.weak_coverage)))
    } else {
        Add-Fail "核心算法质量" "残余复核层" "必须对未纳入正式层的线性残余给出可解释复核层" "摘要异常"
    }
}

if ([string]::IsNullOrWhiteSpace($StandaloneAcceptanceDir) -or !(Test-Path -LiteralPath $StandaloneAcceptanceDir)) {
    Add-Fail "独立软件" "独立软件发布包验收记录" "必须存在 standalone_acceptance_* 验收报告" "未找到独立软件发布包验收目录"
} else {
    $StandaloneAcceptanceDir = (Resolve-Path -LiteralPath $StandaloneAcceptanceDir).Path
    $standaloneReport = Join-Path $StandaloneAcceptanceDir "独立软件发布包验收报告.md"
    if (Test-RequiredFile "独立软件" "独立软件发布包验收报告" $standaloneReport 500) {
        $standaloneText = Get-Content -LiteralPath $standaloneReport -Raw -Encoding UTF8
        $standaloneEvidenceDir = Join-Path $root ("release\figure\" + (Split-Path -Leaf $StandaloneAcceptanceDir))
        $standaloneEvidenceFiles = @(
            "01_GUI启动.png",
            "02_选择输入.png",
            "03_参数设置.png",
            "04_GUI运行结果.png"
        ) | ForEach-Object { Join-Path $standaloneEvidenceDir $_ }
        $standaloneManualGuiReady = @($standaloneEvidenceFiles | Where-Object {
            (Test-Path -LiteralPath $_ -PathType Leaf) -and (Get-Item -LiteralPath $_).Length -ge 10000
        }).Count -eq $standaloneEvidenceFiles.Count
        foreach ($name in @("发布包结构", "个人路径扫描", "核心 CLI 运行", "根目录 GUI 参数转发", "GUI 点击运行")) {
            if ($standaloneText -match "\| $([regex]::Escape($name)) \| 通过 \|") {
                Add-Pass "独立软件" $name "独立软件验收报告必须显示通过" "报告显示通过" $standaloneReport
            } elseif ($name -eq "GUI 点击运行" -and
                      $standaloneManualGuiReady) {
                Add-Pass "独立软件" $name "应由 GUI 自动验收或真实界面操作截图证明" ("真实界面操作截图齐全：" + ($standaloneEvidenceFiles -join "；")) $standaloneEvidenceDir
            } elseif ($name -eq "GUI 点击运行" -and
                      $standaloneText -match "\| GUI 点击运行 \| 已跳过 \|") {
                Add-Manual "独立软件" $name "应由真实界面操作截图或 GUI 自动验收记录证明" "自动点击已跳过；请使用真实界面操作截图补证" $standaloneReport
            } else {
                Add-Fail "独立软件" $name "独立软件验收报告必须显示通过" "报告未显示通过" $standaloneReport
            }
        }
    }
}

if ([string]::IsNullOrWhiteSpace($ReleasePackageDir) -or !(Test-Path -LiteralPath $ReleasePackageDir)) {
    Add-Fail "发布包" "最新发布包目录" "必须存在 FaultPolyTrace_* 发布包" "未找到发布包目录"
} else {
    $ReleasePackageDir = (Resolve-Path -LiteralPath $ReleasePackageDir).Path
    Add-Pass "发布包" "最新发布包目录" "必须存在 FaultPolyTrace_* 发布包" $ReleasePackageDir $ReleasePackageDir
    foreach ($item in @(
        @{Name="独立桌面软件"; Path="FaultPolyTrace.exe"; Min=1},
        @{Name="离线核心 CLI"; Path="bin\faultpolytrace.exe"; Min=1},
        @{Name="Qt 配置"; Path="qt.conf"; Min=1},
        @{Name="Qt 平台插件"; Path="platforms\qwindows.dll"; Min=1},
        @{Name="GeoEast 集成控件"; Path="qt_launcher\faultpolytrace_launcher.exe"; Min=1},
        @{Name="GeoEast BO Runner"; Path="geoeast_plugin\bo_runner\faultpolytrace_geoeast_runner.exe"; Min=1},
        @{Name="发布包清单"; Path="发布包清单.md"; Min=500},
        @{Name="快速入口说明"; Path="README_先打开我.md"; Min=500}
    )) {
        Test-RequiredFile "发布包" $item.Name (Join-Path $ReleasePackageDir $item.Path) $item.Min | Out-Null
    }

    foreach ($script in @(
        "一键总验收.ps1",
        "验收独立软件发布包.ps1",
        "检查参数一致性.ps1",
        "检查独立软件界面功能.ps1",
        "赛题满分交付审计.ps1",
        "生成答辩评审材料包.ps1",
        "生成残余复核层.py",
        "生成质量评估图.py",
        "顶尖质量诊断.py"
    )) {
        Test-RequiredFile "发布包脚本" $script (Join-Path $ReleasePackageDir "scripts\$script") 1 | Out-Null
    }

    $reviewPackDir = Join-Path $ReleasePackageDir "答辩评审材料包"
    if (Test-RequiredDir "评审交付" "答辩评审材料包" $reviewPackDir) {
        foreach ($item in @(
            @{Name="评分项证据索引"; Path="评分项证据索引.md"; Min=500},
            @{Name="答辩演示脚本"; Path="答辩演示脚本.md"; Min=500},
            @{Name="交付材料检查清单"; Path="交付材料检查清单.md"; Min=500},
            @{Name="材料包清单"; Path="材料包清单.csv"; Min=100}
        )) {
            Test-RequiredFile "评审交付" $item.Name (Join-Path $reviewPackDir $item.Path) $item.Min | Out-Null
        }
    }
}

$requiredDocs = @(
    @{Name="用户操作手册"; Path="docs\用户操作手册.md"; Min=5000},
    @{Name="GeoEast 使用指南"; Path="docs\GeoEast使用指南.md"; Min=3000},
    @{Name="项目架构说明"; Path="docs\05_项目架构说明.md"; Min=3000},
    @{Name="代码结构说明"; Path="docs\06_代码结构说明.md"; Min=2000},
    @{Name="算法原理说明"; Path="docs\算法原理说明.md"; Min=5000},
    @{Name="性能与内存报告"; Path="docs\赛题7性能与内存报告.md"; Min=1500},
    @{Name="发布说明与验收清单"; Path="docs\发布说明与验收清单.md"; Min=2000},
    @{Name="官方数据最新验收报告"; Path="docs\官方数据最新验收报告.md"; Min=2000},
    @{Name="需求追踪矩阵"; Path="docs\需求追踪矩阵.md"; Min=2000},
    @{Name="企业级用户操作手册与产品说明书"; Path="docs\企业级用户操作手册与产品说明书.md"; Min=50000}
)

foreach ($doc in $requiredDocs) {
    Test-RequiredFile "中文文档" $doc.Name (Join-Path $root $doc.Path) $doc.Min | Out-Null
}

$repoGeoEastFiles = @(
    @{Name="主控插件 manifest"; Paths=@("geoeast_plugin\mainframe_plugin\manifest.xml")},
    @{Name="主控插件工程"; Paths=@("geoeast_plugin\mainframe_plugin\FaultPolyTraceMainframePlugin.pro")},
    @{Name="Qt 集成控件工程"; Paths=@("geoeast_plugin\qt_launcher\FaultPolyTraceLauncher.pro")},
    @{Name="BO Runner 工程"; Paths=@("geoeast_plugin\bo_runner\FaultPolyTraceGeoEast.pro")},
    @{Name="BO Runner 可执行文件"; Paths=@("geoeast_plugin\bo_runner\faultpolytrace_geoeast_runner.exe")},
    @{Name="Qt 集成控件可执行文件"; Paths=@("geoeast_plugin\qt_launcher\faultpolytrace_launcher.exe", "qt_launcher\faultpolytrace_launcher.exe")}
)

foreach ($file in $repoGeoEastFiles) {
    $candidatePaths = @()
    foreach ($relativePath in $file.Paths) {
        $candidatePaths += (Join-Path $root $relativePath)
    }
    Test-RequiredAnyFile "GeoEast 集成" $file.Name $candidatePaths 1 | Out-Null
}

$geoEastEvidence = Get-GeoEastEvidence
if ($geoEastEvidence.screenshotsReady) {
    Add-Pass "GeoEast 集成" "GeoEast 菜单/按钮现场可见" "需在有效 license 和目标工区中确认菜单入口可见" $geoEastEvidence.menu $geoEastEvidence.dir
    Add-Pass "GeoEast 集成" "GeoEast 启动器自动配置与输入网格" "需证明从 GeoEast 入口打开后能自动配置并刷新 BOMapGrid" ("{0}；{1}" -f $geoEastEvidence.launcher, $geoEastEvidence.grid) $geoEastEvidence.dir
    Add-Pass "GeoEast 集成" "GeoEast 原始数据与结果叠加显示" "需在 GeoMapping/GeoBasemap 中叠加原始网格和输出多边形确认" ("{0}；{1}；{2}" -f $geoEastEvidence.original, $geoEastEvidence.result, $geoEastEvidence.overlay) $geoEastEvidence.dir
    Add-Pass "GeoEast 集成" "GeoEast 数据库写入当前工区" "需在目标工区确认 BOFaultPolygon 输出对象可见且可刷新" ("{0}；{1}" -f $geoEastEvidence.database, $geoEastEvidence.log) $geoEastEvidence.dir
} else {
    $missingEvidence = ($geoEastEvidence.missing -join "；")
    if ([string]::IsNullOrWhiteSpace($geoEastEvidence.dir)) {
        $missingEvidence = "未找到现场证据目录。可运行 scripts/检查GeoEast现场证据.ps1 -Initialize 生成模板。"
    } else {
        $missingEvidence = "现场证据目录：$($geoEastEvidence.dir)；缺少：$missingEvidence"
    }
    Add-Manual "GeoEast 集成" "GeoEast 菜单/按钮现场可见" "需在有效 license 和目标工区中确认菜单入口可见" $missingEvidence $geoEastEvidence.dir
    Add-Manual "GeoEast 集成" "GeoEast 原始数据与结果叠加显示" "需在 GeoMapping/GeoBasemap 中叠加原始网格和输出多边形确认" $missingEvidence $geoEastEvidence.dir
    Add-Manual "GeoEast 集成" "GeoEast 数据库写入当前工区" "需在目标工区确认 BOFaultPolygon 输出对象可见且可刷新" $missingEvidence $geoEastEvidence.dir
}

if ($geoEastEvidence.videoReady) {
    Add-Pass "评审交付" "演示视频或答辩截图" "建议准备双击独立软件、GeoEast 按钮、官方数据运行、结果叠加四类证据" $geoEastEvidence.video $geoEastEvidence.dir
} elseif ($geoEastEvidence.screenshotsReady) {
    Add-Pass "评审交付" "演示视频或答辩截图" "演示视频或答辩截图至少满足其一" ("现场答辩截图齐全：$($geoEastEvidence.dir)") $geoEastEvidence.dir
} else {
    Add-Manual "评审交付" "演示视频或答辩截图" "建议准备双击独立软件、GeoEast 按钮、官方数据运行、结果叠加四类证据" "未找到完整截图或 09_演示视频。可按 GeoEast 现场证据包模板补齐。" $geoEastEvidence.dir
}

$passCount = @($script:auditResults | Where-Object { $_.status -eq "通过" }).Count
$failCount = @($script:auditResults | Where-Object { $_.status -eq "失败" }).Count
$manualCount = @($script:auditResults | Where-Object { $_.status -eq "待人工确认" }).Count

$reportPath = Join-Path $auditDir "赛题满分交付审计报告.md"
$csvPath = Join-Path $auditDir "赛题满分交付审计明细.csv"

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add('# 赛题满分交付审计报告') | Out-Null
$lines.Add('') | Out-Null
$lines.Add("审计时间：$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')") | Out-Null
$lines.Add("项目根目录：$root") | Out-Null
$lines.Add("官方验收目录：$AcceptanceDir") | Out-Null
$lines.Add("发布包目录：$ReleasePackageDir") | Out-Null
$lines.Add("独立软件验收目录：$StandaloneAcceptanceDir") | Out-Null
if (![string]::IsNullOrWhiteSpace($officialDataPath)) {
    $lines.Add("官方数据：$officialDataPath") | Out-Null
}
$lines.Add('') | Out-Null
$lines.Add('## 1. 总体结论') | Out-Null
$lines.Add('') | Out-Null
$lines.Add('| 状态 | 数量 |') | Out-Null
$lines.Add('|---|---:|') | Out-Null
$lines.Add("| 通过 | $passCount |") | Out-Null
$lines.Add("| 失败 | $failCount |") | Out-Null
$lines.Add("| 待人工确认 | $manualCount |") | Out-Null
$lines.Add('') | Out-Null
if ($failCount -eq 0) {
    $lines.Add('自动审计硬门槛未发现失败项。待人工确认项不等同于失败，但属于提交前必须补齐的现场证据。') | Out-Null
} else {
    $lines.Add('自动审计发现失败项，请先修复失败项再继续打包或提交。') | Out-Null
}
$lines.Add('') | Out-Null
$lines.Add('## 2. 审计明细') | Out-Null
$lines.Add('') | Out-Null
$lines.Add('| 分类 | 项目 | 状态 | 要求 | 证据 |') | Out-Null
$lines.Add('|---|---|---|---|---|') | Out-Null
foreach ($row in $script:auditResults) {
    $evidence = ($row.evidence -replace "\|", "/").Trim()
    $requirement = ($row.requirement -replace "\|", "/").Trim()
    $lines.Add("| $($row.category) | $($row.item) | $($row.status) | $requirement | $evidence |") | Out-Null
}
$lines.Add('') | Out-Null
$lines.Add('## 3. 提交前人工补证建议') | Out-Null
$lines.Add('') | Out-Null
$lines.Add('1. 在 GeoEast 有效 license 与目标工区中打开“断层多边形追踪”入口，并截图菜单、按钮和启动器界面。') | Out-Null
$lines.Add('2. 在 GeoMapping 或 GeoBasemap 中同时显示官方原始网格和输出断层多边形，截图原图、结果图和叠加图。') | Out-Null
$lines.Add('3. 录制 1 到 2 分钟演示视频，覆盖独立软件打开、参数模板、官方数据运行、GeoEast 入口和结果查看。') | Out-Null
$lines.Add('4. 若修改算法参数或代码，必须重新运行 `scripts/一键总验收.ps1 -SkipGeoEastBuild -SkipPackage`，并以新的官方 DAT 验收目录作为正式记录。') | Out-Null

Set-Content -LiteralPath $reportPath -Value ($lines -join "`n") -Encoding UTF8
$script:auditResults | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8

Write-Host "赛题满分交付审计报告:" $reportPath
Write-Host "通过:" $passCount "失败:" $failCount "待人工确认:" $manualCount

if ($StrictManual -and $manualCount -gt 0) {
    Write-Error "存在待人工确认项，且启用了 StrictManual。"
    exit 1
}
if ($failCount -gt 0) {
    exit 1
}
