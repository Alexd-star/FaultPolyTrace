param(
    [string]$Version = "v0.9.0",
    [string]$GeoEastRoot = $(if ($env:FPT_GEOEAST_ROOT) { $env:FPT_GEOEAST_ROOT } elseif ($env:USERPROFILE) { Join-Path $env:USERPROFILE "GeoEast" } else { "C:\GeoEast" }),
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

function Sanitize-PackageTextFiles {
    param([string]$Root)
    Get-ChildItem -LiteralPath $Root -Recurse -File -Include *.md,*.txt,*.ini,*.ps1,*.cmd,*.py | ForEach-Object {
        $text = Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8
        $text = $text -replace 'C:\\Users\\[^\\]+\\Documents\\BGPCup\\FaultPolyTrace', "<安装目录>\FaultPolyTrace"
        $text = $text -replace 'C:/Users/[^/]+/Documents/BGPCup/FaultPolyTrace', "<安装目录>/FaultPolyTrace"
        $text = $text -replace 'C:\\Users\\[^\\]+\\GeoEast', "<GeoEast安装根目录>"
        $text = $text -replace 'C:/Users/[^/]+/GeoEast', "<GeoEast安装根目录>"
        $text = $text -replace '[A-Za-z]:[\\/]+BaiduNetdiskDownload', "<数据目录>"
        if ($env:USERNAME) {
            $escapedUser = [regex]::Escape($env:USERNAME)
            $text = $text -replace "(?<!\d)$escapedUser(?!\d)", '<GeoEast用户名>'
        }
        Set-Content -LiteralPath $_.FullName -Value $text -Encoding UTF8
    }
}

if (!$SkipBuild) {
    & (Join-Path $PSScriptRoot "build.ps1")
    & (Join-Path $PSScriptRoot "build_desktop_app.ps1") -GeoEastRoot $GeoEastRoot
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
$standaloneDir = Join-Path $packageDir "standalone_app"
$geoeastDir = Join-Path $packageDir "geoeast_plugin"
$prerequisitesDir = Join-Path $packageDir "prerequisites"
$figureDir = Join-Path $packageDir "figure"

Copy-FileRequired (Join-Path $repoRoot "build\faultpolytrace.exe") (Join-Path $binDir "faultpolytrace.exe")

$desktopSource = Join-Path $repoRoot "standalone_app\qt_desktop"
$desktopExe = Join-Path $desktopSource "FaultPolyTrace.exe"
Copy-FileRequired $desktopExe (Join-Path $packageDir "FaultPolyTrace.exe")
foreach ($name in @(
    "qt.conf",
    "Qt5Core.dll",
    "Qt5Gui.dll",
    "Qt5Widgets.dll",
    "Qt5Svg.dll",
    "libEGL.dll",
    "libGLESv2.dll",
    "D3Dcompiler_47.dll",
    "opengl32sw.dll",
    "msvcp140.dll",
    "vcruntime140.dll",
    "vcruntime140_1.dll",
    "concrt140.dll"
)) {
    Copy-FileOptional (Join-Path $desktopSource $name) (Join-Path $packageDir $name) | Out-Null
}
foreach ($subdir in @("platforms", "imageformats", "iconengines", "styles", "translations")) {
    $src = Join-Path $desktopSource $subdir
    if (Test-Path -LiteralPath $src) {
        Copy-Item -LiteralPath $src -Destination (Join-Path $packageDir $subdir) -Recurse -Force
    }
}

$launcherSource = Join-Path $repoRoot "geoeast_plugin\qt_launcher"
New-Item -ItemType Directory -Force -Path $launcherDir | Out-Null
$launcherFiles = @(
    "faultpolytrace_launcher.exe",
    "faultpolytrace_launcher.example.ini",
    "README.md",
    "qt.conf",
    "Qt5Core.dll",
    "Qt5Gui.dll",
    "Qt5Widgets.dll",
    "Qt5Svg.dll",
    "libEGL.dll",
    "libGLESv2.dll",
    "D3Dcompiler_47.dll",
    "opengl32sw.dll",
    "msvcp140.dll",
    "vcruntime140.dll",
    "vcruntime140_1.dll",
    "concrt140.dll"
)
foreach ($name in $launcherFiles) {
    Copy-FileOptional (Join-Path $launcherSource $name) (Join-Path $launcherDir $name) | Out-Null
}
foreach ($subdir in @("platforms", "imageformats", "iconengines", "styles", "translations")) {
    $src = Join-Path $launcherSource $subdir
    if (Test-Path -LiteralPath $src) {
        Copy-Item -LiteralPath $src -Destination (Join-Path $launcherDir $subdir) -Recurse -Force
    }
}

$packagingWindowsDir = Join-Path $repoRoot "packaging\windows"
if (Test-Path -LiteralPath $packagingWindowsDir) {
    Copy-Item -Path (Join-Path $packagingWindowsDir "*") -Destination $packageDir -Force
}

$vcRedistCandidates = @(
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Redist\MSVC\14.29.30133\vc_redist.x64.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Redist\MSVC\v142\vc_redist.x64.exe"
)
$vcRedist = $vcRedistCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (![string]::IsNullOrWhiteSpace($vcRedist)) {
    Copy-FileOptional $vcRedist (Join-Path $prerequisitesDir "vc_redist.x64.exe") | Out-Null
}

$runnerPath = Join-Path $repoRoot "geoeast_plugin\bo_runner\faultpolytrace_geoeast_runner.exe"
$runnerIncluded = Copy-FileOptional $runnerPath (Join-Path $geoeastDir "bo_runner\faultpolytrace_geoeast_runner.exe")
$boRunnerPackageDir = New-Item -ItemType Directory -Force -Path (Join-Path $geoeastDir "bo_runner")
Copy-Item -Path (Join-Path $repoRoot "geoeast_plugin\bo_runner\*.pro") -Destination $boRunnerPackageDir -Force
Copy-Item -Path (Join-Path $repoRoot "geoeast_plugin\bo_runner\*.cpp") -Destination $boRunnerPackageDir -Force
Copy-Item -Path (Join-Path $repoRoot "geoeast_plugin\bo_runner\*.h") -Destination $boRunnerPackageDir -Force
Copy-FileOptional (Join-Path $repoRoot "geoeast_plugin\bo_runner\说明.md") (Join-Path $geoeastDir "bo_runner\说明.md") | Out-Null
$qtLauncherPackageDir = New-Item -ItemType Directory -Force -Path (Join-Path $geoeastDir "qt_launcher")
foreach ($pattern in @("*.pro", "*.cpp", "*.h", "*.md", "*.ini")) {
    Get-ChildItem -LiteralPath $launcherSource -Filter $pattern -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $qtLauncherPackageDir -Force
    }
}
$mainframePackageDir = New-Item -ItemType Directory -Force -Path (Join-Path $geoeastDir "mainframe_plugin")
foreach ($name in @("FaultPolyTraceMainframePlugin.pro", "attriModuleAction.cpp", "manifest.xml", "说明.md")) {
    Copy-FileOptional (Join-Path $repoRoot "geoeast_plugin\mainframe_plugin\$name") (Join-Path $mainframePackageDir $name) | Out-Null
}
$builtPluginDir = Join-Path $GeoEastRoot "GeoEast-RC\bin\plugins\GeoAttribute\FaultPolyTraceMainframePlugin"
foreach ($name in @("FaultPolyTraceMainframePlugin.dll", "manifest.xml")) {
    Copy-FileOptional (Join-Path $builtPluginDir $name) (Join-Path $mainframePackageDir "built\$name") | Out-Null
}

Copy-Item -LiteralPath (Join-Path $repoRoot "docs") -Destination $docsDir -Recurse -Force
$standalonePackageDir = New-Item -ItemType Directory -Force -Path (Join-Path $standaloneDir "qt_desktop")
foreach ($name in @("FaultPolyTraceDesktop.pro", "main.cpp", "FaultPolyTraceWindow.cpp", "FaultPolyTraceWindow.h")) {
    Copy-FileOptional (Join-Path $repoRoot "standalone_app\qt_desktop\$name") (Join-Path $standalonePackageDir $name) | Out-Null
}
$releaseFigureDir = Join-Path $releaseRoot "figure"
if (Test-Path -LiteralPath $releaseFigureDir) {
    Copy-Item -LiteralPath $releaseFigureDir -Destination $figureDir -Recurse -Force
}
Copy-Item -LiteralPath (Join-Path $repoRoot "examples\outputs\official") -Destination (Join-Path $examplesDir "outputs\official") -Recurse -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "examples\synthetic") -Destination (Join-Path $examplesDir "synthetic") -Recurse -Force

foreach ($script in @(
    "run_geoeast_bgpcup2026_test7.ps1",
    "run_smoke_test.ps1",
    "run_official_benchmark.ps1",
    "build_geoeast_bo_runner.ps1",
    "build_geoeast_mainframe_plugin.ps1",
    "build_desktop_app.ps1",
    "build_qt_launcher.ps1",
    "打包发布包.ps1",
    "部署到GeoEast.ps1",
    "一键总验收.ps1",
    "验收独立软件发布包.ps1",
    "检查参数一致性.ps1",
    "性能回归检查.ps1",
    "visualize_official_result.py",
    "generate_official_quality_report.py",
    "生成性能内存报告.py",
    "生成质量评估图.py",
    "生成漏检质控图.py",
    "生成残余复核层.py",
    "生成前后对比图.py",
    "顶尖质量诊断.py",
    "漏检根因诊断.py",
    "生成冲顶复核包.py",
    "综合质量评分.py",
    "自动参数推荐.py",
    "wkt_geometry.py"
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
$manifest += '- `FaultPolyTrace.exe`：独立桌面软件，处理外部网格文件，不包含 GeoEast 数据库配置'
$manifest += '- `qt_launcher/faultpolytrace_launcher.exe`：GeoEast 集成控件，用于数据库读写和平台入口'
$manifest += '- `01_打开图形界面.cmd`：评审推荐图形入口'
$manifest += '- `02_运行离线演示.cmd`：无需 GeoEast 的双击离线演示'
$manifest += '- `03_查看命令行帮助.cmd`：命令行参数帮助'
$manifest += '- `docs/`：中文交付文档'
$manifest += '- `figure/`：界面样式截图和 GeoEast 参考截图'
$manifest += '- `examples/outputs/official/`：官方数据结果、图件、报告依据'
$manifest += '- `scripts/`：运行、可视化、报告脚本'
$manifest += '- `src/`：核心源码'
$manifest += '- `standalone_app/`：独立桌面软件 Qt 工程源码'
$manifest += '- `geoeast_plugin/`：GeoEast runner、集成控件和主控插件'
$manifest += '- `prerequisites/`：可选运行库安装包'
$manifest += ""
$manifest += "## 当前状态"
$manifest += ""
$manifest += "- 离线 CLI：已构建"
$manifest += "- 独立桌面软件：已构建"
$manifest += "- GeoEast 集成控件：已构建"
$manifest += '- GeoEast 数据库读写：已验证过 `FaultPolyTrace_Result=465`'
if ($runnerIncluded) {
    $manifest += '- GeoEast runner exe：已包含'
} else {
    $manifest += '- GeoEast runner exe：未包含，当前本机重新构建受 `libpq.lib` 链接路径影响；可在 VS2019/Qt 工程中修正 PostgreSQL lib 路径后重建'
}
$manifest += '- GeoEast 主控 Ribbon 插件：已构建通过，内含中文“断层多边形追踪/赛题7”入口'
$manifest += "- GeoMapping 平面成图：需使用与 GeoEast 版本和年份匹配的有效 license，最终交付前应补 GeoEast 内部截图"
$manifest += ""
$manifest += "## 推荐入口"
$manifest += ""
$manifest += '1. 先读 `README_先打开我.md`。'
$manifest += '2. 双击 `FaultPolyTrace.exe` 或 `01_打开图形界面.cmd` 打开独立桌面软件。'
$manifest += '3. 没有 GeoEast 环境时，双击 `02_运行离线演示.cmd` 验证算法独立运行能力。'
$manifest += '4. 有 GeoEast 环境时，按 `docs/用户操作手册.md` 或 `docs/GeoEast使用指南.md` 使用 GeoEast 菜单入口，入口程序是 `qt_launcher/faultpolytrace_launcher.exe`。'
$manifest += '5. 查看 `docs/官方数据最新验收报告.md`。'
$manifest += '6. 修改界面参数、模板参数、CLI 参数或 Runner 参数后运行 `scripts/检查参数一致性.ps1`，确认独立软件、GeoEast 控件、核心算法和发布包链路一致。'
$manifest += '7. 修改独立软件或发布包后运行 `scripts/验收独立软件发布包.ps1 -PackageDir <发布包目录>`，确认 GUI、CLI、根目录转发和输出文件完整。'
$manifest += '8. 修改算法后运行 `scripts/一键总验收.ps1 -SkipGeoEastBuild -SkipPackage`，确认官方数据仍满足覆盖、防黏连、拓扑和性能门禁。'

Set-Content -LiteralPath (Join-Path $packageDir "发布包清单.md") -Value ($manifest -join "`n") -Encoding UTF8

Sanitize-PackageTextFiles $packageDir

$zipPath = Join-Path $releaseRoot ("FaultPolyTrace_" + $Version + ".zip")
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -LiteralPath $packageDir -DestinationPath $zipPath -Force

Write-Host "Release package:" $packageDir
Write-Host "Release zip:" $zipPath
