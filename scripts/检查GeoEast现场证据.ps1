param(
    [string]$EvidenceDir = "",
    [switch]$Initialize,
    [switch]$Strict
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path -LiteralPath (Join-Path $scriptRoot "..")
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"

function Get-FigureRoot {
    $repoFigure = Join-Path $root "release\figure"
    if (Test-Path -LiteralPath (Join-Path $root "release")) {
        New-Item -ItemType Directory -Force -Path $repoFigure | Out-Null
        return (Resolve-Path -LiteralPath $repoFigure).Path
    }

    $packageFigure = Join-Path $root "figure"
    New-Item -ItemType Directory -Force -Path $packageFigure | Out-Null
    return (Resolve-Path -LiteralPath $packageFigure).Path
}

function Get-LatestEvidenceDir {
    param([string]$FigureRoot)
    Get-ChildItem -LiteralPath $FigureRoot -Directory |
        Where-Object { $_.Name -like "geoeast_acceptance_*" } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
}

$imageExts = @(".png", ".jpg", ".jpeg", ".bmp")
$videoExts = @(".mp4", ".mkv", ".avi", ".mov", ".wmv")

$requiredImages = @(
    @{ Stem = '01_GeoEast菜单入口'; Name = 'GeoEast 菜单或按钮入口可见'; Description = '显示 GeoEast 主界面中“断层多边形追踪”菜单、按钮或 Ribbon 入口。' },
    @{ Stem = '02_启动器自动配置'; Name = '启动器自动配置'; Description = '显示从 GeoEast 入口打开后的启动器，数据库、项目、用户、Runner 或根目录配置已自动识别。' },
    @{ Stem = '03_输入网格列表'; Name = '输入网格列表'; Description = '显示刷新 BOMapGrid 后可选择官方输入网格，例如 test7_fault_attribute_bo。' },
    @{ Stem = '04_原始网格显示'; Name = '原始网格显示'; Description = '显示官方 test.dat 或 GeoEast BOMapGrid 原始断层属性图，范围和色标正确。' },
    @{ Stem = '05_结果多边形图层'; Name = '结果多边形图层'; Description = '显示 FaultPolyTrace_Result 或等价 BOFaultPolygon 结果图层可见。' },
    @{ Stem = '06_原始与结果叠加'; Name = '原始与结果叠加'; Description = '显示原始网格与输出断层多边形叠加，颜色、线宽、范围和图层顺序清晰。' },
    @{ Stem = '07_数据库对象管理'; Name = '数据库对象管理'; Description = '显示结果对象已写入 GeoEast 数据库或对象管理器中可刷新查看。' },
    @{ Stem = '08_运行日志与耗时'; Name = '运行日志与耗时'; Description = '显示启动器运行日志、核心耗时和写库结果，证明不是静态截图。' }
)

$results = New-Object System.Collections.Generic.List[object]

function Add-Result {
    param([string]$Item, [string]$Status, [string]$Requirement, [string]$Evidence)
    $results.Add([pscustomobject]@{
        item = $Item
        status = $Status
        requirement = $Requirement
        evidence = $Evidence
    }) | Out-Null
}

function Find-FileByStem {
    param([string]$Dir, [string]$Stem, [string[]]$Extensions)
    foreach ($ext in $Extensions) {
        $candidate = Join-Path $Dir ($Stem + $ext)
        if (Test-Path -LiteralPath $candidate) {
            return Get-Item -LiteralPath $candidate
        }
    }
    return $null
}

function Write-EvidenceReadme {
    param([string]$Dir)
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("# GeoEast 现场验收证据包") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("本目录用于保存 GeoEast 真实运行环境下的截图、录屏和检查报告。请使用以下固定文件名，便于满分交付审计脚本自动识别。") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("## 必需截图") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("| 文件名 | 内容要求 |") | Out-Null
    $lines.Add("|---|---|") | Out-Null
    foreach ($item in $requiredImages) {
        $lines.Add("| `$($item.Stem).png` | $($item.Description) |") | Out-Null
    }
    $lines.Add("") | Out-Null
    $lines.Add("## 建议录屏") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("建议录制 `09_演示视频.mp4`，覆盖从 GeoEast 菜单入口打开启动器、选择输入网格、运行追踪、查看结果图层和叠加结果的完整流程。") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("## 操作提示") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("1. 先打开 GeoEast 和赛题工区。") | Out-Null
    $lines.Add("2. 点击【断层多边形追踪】入口，确认启动器自动识别数据库、项目和用户。") | Out-Null
    $lines.Add("3. 点击【刷新网格】，选择官方输入网格并运行。") | Out-Null
    $lines.Add("4. 在 GeoMapping、GeoBasemap 或工区底图中分别截图原始网格、结果多边形和叠加图。") | Out-Null
    $lines.Add("5. 将截图按固定文件名放入本目录，再运行 `scripts/检查GeoEast现场证据.ps1 -EvidenceDir <本目录>`。") | Out-Null

    Set-Content -LiteralPath (Join-Path $Dir "README_现场证据说明.md") -Value ($lines -join "`n") -Encoding UTF8
}

$figureRoot = Get-FigureRoot

if ([string]::IsNullOrWhiteSpace($EvidenceDir)) {
    if ($Initialize) {
        $EvidenceDir = Join-Path $figureRoot ("geoeast_acceptance_" + $timestamp)
    } else {
        $latest = Get-LatestEvidenceDir -FigureRoot $figureRoot
        if ($latest) {
            $EvidenceDir = $latest.FullName
        }
    }
}

if ([string]::IsNullOrWhiteSpace($EvidenceDir)) {
    Add-Result "GeoEast 现场证据目录" "失败" "需要存在 release/figure/geoeast_acceptance_* 证据目录" "未找到证据目录。可先运行 scripts/检查GeoEast现场证据.ps1 -Initialize。"
} else {
    if ($Initialize) {
        New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null
        New-Item -ItemType Directory -Force -Path (Join-Path $EvidenceDir "logs") | Out-Null
        Write-EvidenceReadme -Dir $EvidenceDir
        foreach ($item in $requiredImages) {
            $placeholder = Join-Path $EvidenceDir ($item.Stem + "_请替换为截图.txt")
            Set-Content -LiteralPath $placeholder -Value $item.Description -Encoding UTF8
        }
        Set-Content -LiteralPath (Join-Path $EvidenceDir "09_演示视频_请替换为录屏.txt") -Value "请将演示视频命名为 09_演示视频.mp4，建议 1 到 2 分钟。" -Encoding UTF8
    }

    if (Test-Path -LiteralPath $EvidenceDir) {
        $EvidenceDir = (Resolve-Path -LiteralPath $EvidenceDir).Path
        Add-Result "GeoEast 现场证据目录" "通过" "证据目录存在" $EvidenceDir
        foreach ($item in $requiredImages) {
            $file = Find-FileByStem -Dir $EvidenceDir -Stem $item.Stem -Extensions $imageExts
            if ($file -and $file.Length -ge 10000) {
                Add-Result $item.Name "通过" "需要截图文件 $($item.Stem).png/.jpg/.bmp，且不应是空文件" $file.FullName
            } elseif ($file) {
                Add-Result $item.Name "失败" "截图文件至少 10 KB，避免误把空白或占位文件当证据" "$($file.FullName)；大小 $($file.Length) 字节"
            } else {
                Add-Result $item.Name "失败" "需要截图文件 $($item.Stem).png/.jpg/.bmp" "未找到"
            }
        }

        $video = Find-FileByStem -Dir $EvidenceDir -Stem "09_演示视频" -Extensions $videoExts
        if ($video -and $video.Length -ge 100000) {
            Add-Result "演示视频" "通过" "建议提供 09_演示视频.mp4，且至少 100 KB" $video.FullName
        } elseif ($video) {
            Add-Result "演示视频" "失败" "演示视频至少 100 KB，避免误把占位文件当证据" "$($video.FullName)；大小 $($video.Length) 字节"
        } else {
            Add-Result "演示视频" "待补充" "建议提供 09_演示视频.mp4；若暂无视频，需保证 8 张现场截图齐全" "未找到"
        }
    } else {
        Add-Result "GeoEast 现场证据目录" "失败" "证据目录存在" "缺少目录：$EvidenceDir"
    }
}

$passCount = ($results | Where-Object { $_.status -eq "通过" }).Count
$failCount = ($results | Where-Object { $_.status -eq "失败" }).Count
$pendingCount = ($results | Where-Object { $_.status -eq "待补充" }).Count

if (![string]::IsNullOrWhiteSpace($EvidenceDir)) {
    New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null
    $reportPath = Join-Path $EvidenceDir "GeoEast现场证据检查报告.md"
    $csvPath = Join-Path $EvidenceDir "GeoEast现场证据检查明细.csv"

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("# GeoEast 现场证据检查报告") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("检查时间：$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')") | Out-Null
    $lines.Add("证据目录：$EvidenceDir") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("## 1. 汇总") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("| 状态 | 数量 |") | Out-Null
    $lines.Add("|---|---:|") | Out-Null
    $lines.Add("| 通过 | $passCount |") | Out-Null
    $lines.Add("| 失败 | $failCount |") | Out-Null
    $lines.Add("| 待补充 | $pendingCount |") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("## 2. 明细") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("| 项目 | 状态 | 要求 | 证据 |") | Out-Null
    $lines.Add("|---|---|---|---|") | Out-Null
    foreach ($row in $results) {
        $requirement = ($row.requirement -replace "\|", "/")
        $evidence = ($row.evidence -replace "\|", "/")
        $lines.Add("| $($row.item) | $($row.status) | $requirement | $evidence |") | Out-Null
    }
    $lines.Add("") | Out-Null
    $lines.Add("## 3. 结论") | Out-Null
    $lines.Add("") | Out-Null
    if ($failCount -eq 0) {
        $lines.Add("GeoEast 现场截图硬证据已满足自动检查要求。若演示视频仍为待补充，建议在正式答辩前继续补齐。") | Out-Null
    } else {
        $lines.Add("GeoEast 现场截图硬证据尚未满足自动检查要求，请按明细补齐后重新运行检查。") | Out-Null
    }

    Set-Content -LiteralPath $reportPath -Value ($lines -join "`n") -Encoding UTF8
    $results | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8
    Write-Host "GeoEast 现场证据检查报告:" $reportPath
    Write-Host "通过:" $passCount "失败:" $failCount "待补充:" $pendingCount
}

if ($Strict -and ($failCount -gt 0 -or $pendingCount -gt 0)) {
    exit 1
}
if (!$Initialize -and $failCount -gt 0) {
    exit 1
}
