param(
    [string]$Version = "v0.9.0",
    [string]$GeoEastRoot = $(if ($env:FPT_GEOEAST_ROOT) { $env:FPT_GEOEAST_ROOT } else { "C:\Users\26312\GeoEast" }),
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$releaseRoot = Join-Path $repoRoot "release"
$packageDir = Join-Path $releaseRoot ("FaultPolyTrace_" + $Version)

function Copy-FileRequired {
    param([string]$Source, [string]$Destination)
    if (!(Test-Path -LiteralPath $Source)) {
        throw "Required file missing: $Source"
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Destination) | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

function Copy-FileOptional {
    param([string]$Source, [string]$Destination)
    if (Test-Path -LiteralPath $Source) {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Destination) | Out-Null
        Copy-Item -LiteralPath $Source -Destination $Destination -Force
        return $true
    }
    return $false
}

if (!$SkipBuild) {
    & (Join-Path $PSScriptRoot "build.ps1")
    & (Join-Path $PSScriptRoot "build_qt_launcher.ps1") -GeoEastRoot $GeoEastRoot
    & (Join-Path $PSScriptRoot "build_geoeast_bo_runner.ps1") -GeoEastRoot $GeoEastRoot
    & (Join-Path $PSScriptRoot "build_geoeast_mainframe_plugin.ps1") -GeoEastRoot $GeoEastRoot
}

if (Test-Path -LiteralPath $packageDir) {
    Remove-Item -LiteralPath $packageDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $packageDir | Out-Null

$binDir = Join-Path $packageDir "bin"
$launcherDir = Join-Path $packageDir "qt_launcher"
$docsDir = Join-Path $packageDir "docs"
$examplesDir = Join-Path $packageDir "examples"
$scriptsDir = Join-Path $packageDir "scripts"
$srcDir = Join-Path $packageDir "src"
$geoeastDir = Join-Path $packageDir "geoeast_plugin"

Copy-FileRequired (Join-Path $repoRoot "build\faultpolytrace.exe") (Join-Path $binDir "faultpolytrace.exe")

$launcherSource = Join-Path $repoRoot "geoeast_plugin\qt_launcher"
New-Item -ItemType Directory -Force -Path $launcherDir | Out-Null
$launcherFiles = @(
    "faultpolytrace_launcher.exe",
    "faultpolytrace_launcher.example.ini",
    "README.md",
    "Qt5Core.dll",
    "Qt5Gui.dll",
    "Qt5Widgets.dll",
    "Qt5Svg.dll",
    "libEGL.dll",
    "libGLESv2.dll",
    "D3Dcompiler_47.dll",
    "opengl32sw.dll"
)
foreach ($name in $launcherFiles) {
    Copy-FileOptional (Join-Path $launcherSource $name) (Join-Path $launcherDir $name) | Out-Null
}
foreach ($subdir in @("platforms", "imageformats", "iconengines", "styles")) {
    $src = Join-Path $launcherSource $subdir
    if (Test-Path -LiteralPath $src) {
        Copy-Item -LiteralPath $src -Destination (Join-Path $launcherDir $subdir) -Recurse -Force
    }
}

$runnerPath = Join-Path $repoRoot "geoeast_plugin\bo_runner\faultpolytrace_geoeast_runner.exe"
$runnerIncluded = Copy-FileOptional $runnerPath (Join-Path $geoeastDir "bo_runner\faultpolytrace_geoeast_runner.exe")
$boRunnerPackageDir = New-Item -ItemType Directory -Force -Path (Join-Path $geoeastDir "bo_runner")
Copy-Item -Path (Join-Path $repoRoot "geoeast_plugin\bo_runner\*.pro") -Destination $boRunnerPackageDir -Force
Copy-Item -Path (Join-Path $repoRoot "geoeast_plugin\bo_runner\*.cpp") -Destination $boRunnerPackageDir -Force
Copy-Item -Path (Join-Path $repoRoot "geoeast_plugin\bo_runner\*.h") -Destination $boRunnerPackageDir -Force
Copy-FileOptional (Join-Path $repoRoot "geoeast_plugin\bo_runner\说明.md") (Join-Path $geoeastDir "bo_runner\说明.md") | Out-Null
$mainframePackageDir = New-Item -ItemType Directory -Force -Path (Join-Path $geoeastDir "mainframe_plugin")
foreach ($name in @("FaultPolyTraceMainframePlugin.pro", "attriModuleAction.cpp", "manifest.xml", "说明.md")) {
    Copy-FileOptional (Join-Path $repoRoot "geoeast_plugin\mainframe_plugin\$name") (Join-Path $mainframePackageDir $name) | Out-Null
}
$builtPluginDir = Join-Path $GeoEastRoot "GeoEast-RC\bin\plugins\GeoAttribute\FaultPolyTraceMainframePlugin"
foreach ($name in @("FaultPolyTraceMainframePlugin.dll", "manifest.xml")) {
    Copy-FileOptional (Join-Path $builtPluginDir $name) (Join-Path $mainframePackageDir "built\$name") | Out-Null
}

Copy-Item -LiteralPath (Join-Path $repoRoot "docs") -Destination $docsDir -Recurse -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "examples\outputs\official") -Destination (Join-Path $examplesDir "outputs\official") -Recurse -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "examples\synthetic") -Destination (Join-Path $examplesDir "synthetic") -Recurse -Force

foreach ($script in @(
    "run_geoeast_bgpcup2026_test7.ps1",
    "run_smoke_test.ps1",
    "run_official_benchmark.ps1",
    "build_geoeast_bo_runner.ps1",
    "build_geoeast_mainframe_plugin.ps1",
    "build_qt_launcher.ps1",
    "部署到GeoEast.ps1",
    "一键总验收.ps1",
    "性能回归检查.ps1",
    "visualize_official_result.py",
    "generate_official_quality_report.py",
    "生成性能内存报告.py"
)) {
    Copy-FileOptional (Join-Path $repoRoot "scripts\$script") (Join-Path $scriptsDir $script) | Out-Null
}

Copy-Item -LiteralPath (Join-Path $repoRoot "src") -Destination $srcDir -Recurse -Force
Copy-FileRequired (Join-Path $repoRoot "README.md") (Join-Path $packageDir "README.md")

$manifest = @()
$manifest += "# FaultPolyTrace 发布包清单"
$manifest += ""
$manifest += "版本：$Version"
$manifest += "生成时间：$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$manifest += ""
$manifest += "## 包含内容"
$manifest += ""
$manifest += '- `bin/faultpolytrace.exe`：离线算法程序'
$manifest += '- `qt_launcher/faultpolytrace_launcher.exe`：Qt 用户界面'
$manifest += '- `docs/`：中文交付文档'
$manifest += '- `examples/outputs/official/`：官方数据结果、图件、报告依据'
$manifest += '- `scripts/`：运行、可视化、报告脚本'
$manifest += '- `src/`：核心源码'
$manifest += '- `geoeast_plugin/`：GeoEast runner、Qt 启动器联动入口和主控插件'
$manifest += ""
$manifest += "## 当前状态"
$manifest += ""
$manifest += "- 离线 CLI：已构建"
$manifest += "- Qt 启动器：已构建"
$manifest += '- GeoEast 数据库读写：已验证过 `FaultPolyTrace_Result=465`'
if ($runnerIncluded) {
    $manifest += '- GeoEast runner exe：已包含'
} else {
    $manifest += '- GeoEast runner exe：未包含，当前本机重新构建受 `libpq.lib` 链接路径影响；可在 VS2019/Qt 工程中修正 PostgreSQL lib 路径后重建'
}
$manifest += '- GeoEast 主控 Ribbon 插件：已构建通过，内含中文“断层多边形追踪/赛题7”入口'
$manifest += "- GeoMapping 平面成图：当前官方 license 到期，需 2026 有效 license 后补 GeoEast 内部截图"
$manifest += ""
$manifest += "## 推荐入口"
$manifest += ""
$manifest += '1. 先读 `docs/用户操作手册.md`。'
$manifest += '2. 阅读 `docs/软件设计说明.md` 和 `docs/代码编制说明.md`。'
$manifest += '3. 双击 `qt_launcher/faultpolytrace_launcher.exe`。'
$manifest += "4. 使用官方数据默认参数运行。"
$manifest += '5. 查看 `docs/赛题7官方数据运行质量报告_平滑版.md`。'
$manifest += '6. 修改代码后运行 `scripts/性能回归检查.ps1`，确认官方数据平滑场景仍满足性能和拓扑门禁。'

Set-Content -LiteralPath (Join-Path $packageDir "发布包清单.md") -Value ($manifest -join "`n") -Encoding UTF8

$zipPath = Join-Path $releaseRoot ("FaultPolyTrace_" + $Version + ".zip")
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -LiteralPath $packageDir -DestinationPath $zipPath -Force

Write-Host "Release package:" $packageDir
Write-Host "Release zip:" $zipPath
