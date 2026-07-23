[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$reportDir = Join-Path $repoRoot "examples\outputs\customer_portability_$timestamp"
$reportPath = Join-Path $reportDir "客户机可移植性检查报告.md"

$files = @(
    "standalone_app\qt_desktop\FaultPolyTraceWindow.cpp",
    "standalone_app\qt_desktop\FaultPolyTraceWindow.h",
    "standalone_app\qt_desktop\main.cpp",
    "standalone_app\qt_desktop\FaultPolyTraceDesktop.pro",
    "geoeast_plugin\qt_launcher\FaultTraceDialog.cpp",
    "geoeast_plugin\qt_launcher\FaultTraceDialog.h",
    "geoeast_plugin\qt_launcher\main.cpp",
    "geoeast_plugin\qt_launcher\FaultPolyTraceLauncher.pro",
    "geoeast_plugin\qt_launcher\faultpolytrace_launcher.example.ini",
    "geoeast_plugin\bo_runner\main.cpp",
    "geoeast_plugin\bo_runner\FaultPolyTraceGeoEast.vcxproj",
    "scripts\build_desktop_app.ps1",
    "scripts\build_qt_launcher.ps1",
    "scripts\build_geoeast_bo_runner.ps1",
    "scripts\build_geoeast_mainframe_plugin.ps1",
    "scripts\部署到GeoEast.ps1",
    "scripts\打包发布包.ps1",
    "scripts\run_geoeast_bgpcup2026_test7.ps1"
)

$patterns = @(
    @{ Name = "开发机用户路径"; Regex = 'C:[\\/]+Users[\\/]+[A-Za-z0-9._-]+' },
    @{ Name = "开发机百度网盘路径"; Regex = "D:[\\/]+BaiduNetdiskDownload" },
    @{ Name = "固定赛题项目默认值"; Regex = '"BGPCup2026_Test7"' },
    @{ Name = "固定赛题输入网格默认值"; Regex = '"test7_fault_attribute(_bo)?"' },
    @{ Name = "固定开发机用户名默认值"; Regex = 'user(Edit_)?->setText\("[^\"]+"\)' }
)

function Is-AllowedLegacyCleanupLine {
    param([string]$Line)
    return $Line.Contains("isLegacyDeveloperDefault") -or
        $Line.Contains("normalized ==") -or
        $Line.Contains("normalized.contains")
}

$violations = [System.Collections.Generic.List[object]]::new()
foreach ($relative in $files) {
    $path = Join-Path $repoRoot $relative
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
        $violations.Add([pscustomobject]@{
            File = $relative
            Line = 0
            Rule = "文件存在"
            Text = "缺少待检查文件"
        }) | Out-Null
        continue
    }

    $lineNo = 0
    foreach ($line in Get-Content -LiteralPath $path) {
        ++$lineNo
        if (Is-AllowedLegacyCleanupLine -Line $line) {
            continue
        }
        foreach ($pattern in $patterns) {
            if ($line -match $pattern.Regex) {
                $violations.Add([pscustomobject]@{
                    File = $relative
                    Line = $lineNo
                    Rule = $pattern.Name
                    Text = $line.Trim()
                }) | Out-Null
            }
        }
    }
}

New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("# 客户机可移植性检查报告") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("生成时间：$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## 检查范围") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("本检查覆盖独立软件、GeoEast 启动器、GeoEast runner、构建部署脚本和发布包脚本，目标是避免客户打开软件后看到开发机路径、固定用户名、固定赛题项目或固定测试网格默认值。") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## 汇总") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("| 项目 | 数量 |") | Out-Null
$lines.Add("|---|---:|") | Out-Null
$lines.Add("| 违规项 | $($violations.Count) |") | Out-Null
$lines.Add("") | Out-Null

if ($violations.Count -gt 0) {
    $lines.Add("## 违规明细") | Out-Null
    $lines.Add("") | Out-Null
    $lines.Add("| 文件 | 行号 | 规则 | 内容 |") | Out-Null
    $lines.Add("|---|---:|---|---|") | Out-Null
    foreach ($item in $violations) {
        $safeText = ($item.Text -replace "\|", "\|" -replace "`r?`n", " ")
        $lines.Add("| $($item.File) | $($item.Line) | $($item.Rule) | $safeText |") | Out-Null
    }
} else {
    $lines.Add("未发现客户可见链路中的开发机硬编码路径或固定比赛对象默认值。") | Out-Null
}

$lines | Set-Content -LiteralPath $reportPath -Encoding UTF8
Write-Host "Customer portability report: $reportPath"
Write-Host "Violations: $($violations.Count)"

if ($violations.Count -gt 0) {
    throw "客户机可移植性检查失败。"
}
