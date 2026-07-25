param(
    [string]$Version = "v1.0.0-rc2-unified",
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
    $textExtensions = @(".md", ".txt", ".ini", ".ps1", ".cmd", ".py", ".csv", ".json")
    $utf8Bom = New-Object System.Text.UTF8Encoding($true)
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    Get-ChildItem -LiteralPath $Root -Recurse -File |
        Where-Object { $textExtensions -contains $_.Extension.ToLowerInvariant() } |
        ForEach-Object {
            $original = [System.IO.File]::ReadAllText($_.FullName, [System.Text.Encoding]::UTF8)
            $text = $original -replace 'C:\\Users\\[^\\]+\\Documents\\BGPCup\\FaultPolyTrace', "<安装目录>\FaultPolyTrace"
            $text = $text -replace 'C:/Users/[^/]+/Documents/BGPCup/FaultPolyTrace', "<安装目录>/FaultPolyTrace"
            $text = $text -replace 'C:\\Users\\[^\\]+\\GeoEast', "<GeoEast安装根目录>"
            $text = $text -replace 'C:/Users/[^/]+/GeoEast', "<GeoEast安装根目录>"
            $text = $text -replace '[A-Za-z]:[\\/]+BaiduNetdiskDownload', "<数据目录>"
            if ($env:USERNAME) {
                $escapedUser = [regex]::Escape($env:USERNAME)
                $text = $text -replace "(?<!\d)$escapedUser(?!\d)", '<GeoEast用户名>'
            }
            if ($text -ne $original) {
                $encoding = $(if ($_.Extension.ToLowerInvariant() -eq ".ps1") { $utf8Bom } else { $utf8NoBom })
                [System.IO.File]::WriteAllText($_.FullName, $text, $encoding)
            }
    }
}

function Get-AggregateSha256 {
    param([object[]]$Records)
    $lines = @($Records | Sort-Object path | ForEach-Object { "{0}`t{1}" -f $_.path, $_.sha256 })
    $bytes = [System.Text.Encoding]::UTF8.GetBytes(($lines -join "`n"))
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace("-", "")
    } finally {
        $sha.Dispose()
    }
}

function Get-SourceSnapshot {
    param([string]$Root)
    $paths = @(& git -C $Root -c core.quotepath=false ls-files --cached --others --exclude-standard 2>$null) |
        Where-Object {
            $_ -match '^(src/|standalone_app/qt_desktop/|geoeast_plugin/|packaging/windows/|scripts/|tests/|tests_cpp/)' -or
            $_ -in @('CMakeLists.txt', 'pytest.ini')
        } |
        Where-Object { $_ -match '\.(cpp|c|h|hpp|pro|pri|vcxproj|filters|ps1|py|iss|isl|xml|ini|txt)$' } |
        Sort-Object -Unique
    $records = @()
    foreach ($relative in $paths) {
        $absolute = Join-Path $Root ($relative -replace '/', '\')
        if (Test-Path -LiteralPath $absolute -PathType Leaf) {
            $records += [ordered]@{
                path = $relative
                sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $absolute).Hash
            }
        }
    }
    return [ordered]@{
        fileCount = $records.Count
        sha256 = Get-AggregateSha256 -Records $records
        files = $records
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
$iconIco = Join-Path $repoRoot "assets\icons\faultpolytrace.ico"
$iconPng = Join-Path $repoRoot "assets\icons\faultpolytrace_32.png"

Copy-FileRequired (Join-Path $repoRoot "build\faultpolytrace.exe") (Join-Path $binDir "faultpolytrace.exe")

$desktopSource = Join-Path $repoRoot "standalone_app\qt_desktop"
$desktopExe = Join-Path $desktopSource "FaultPolyTrace.exe"
Copy-FileRequired $desktopExe (Join-Path $packageDir "FaultPolyTrace.exe")
Copy-FileRequired $iconIco (Join-Path $packageDir "faultpolytrace.ico")
Copy-FileRequired $iconPng (Join-Path $packageDir "faultpolytrace.png")
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
Copy-FileRequired $iconIco (Join-Path $launcherDir "faultpolytrace.ico")
Copy-FileRequired $iconPng (Join-Path $launcherDir "faultpolytrace.png")
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
Copy-Item -Path (Join-Path $repoRoot "geoeast_plugin\bo_runner\*.vcxproj") -Destination $boRunnerPackageDir -Force
Copy-Item -Path (Join-Path $repoRoot "geoeast_plugin\bo_runner\*.vcxproj.filters") -Destination $boRunnerPackageDir -Force
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
Get-ChildItem -LiteralPath $docsDir -Recurse -File -Include "*.lic", "*.key", "*.pem" |
    Remove-Item -Force
$standalonePackageDir = New-Item -ItemType Directory -Force -Path (Join-Path $standaloneDir "qt_desktop")
foreach ($name in @("FaultPolyTraceDesktop.pro", "main.cpp", "FaultPolyTraceWindow.cpp", "FaultPolyTraceWindow.h")) {
    Copy-FileOptional (Join-Path $repoRoot "standalone_app\qt_desktop\$name") (Join-Path $standalonePackageDir $name) | Out-Null
}
$releaseFigureDir = Join-Path $releaseRoot "figure"
if (Test-Path -LiteralPath $releaseFigureDir) {
    New-Item -ItemType Directory -Force -Path $figureDir | Out-Null
    foreach ($pattern in @("standalone_acceptance_*", "geoeast_acceptance_*")) {
        $latestEvidence = Get-ChildItem -LiteralPath $releaseFigureDir -Directory |
            Where-Object { $_.Name -like $pattern } |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($latestEvidence) {
            Copy-Item -LiteralPath $latestEvidence.FullName -Destination (Join-Path $figureDir $latestEvidence.Name) -Recurse -Force
        }
    }
}
Copy-Item -LiteralPath (Join-Path $repoRoot "examples\outputs\official") -Destination (Join-Path $examplesDir "outputs\official") -Recurse -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "examples\synthetic") -Destination (Join-Path $examplesDir "synthetic") -Recurse -Force

$repoOutputsDir = Join-Path $repoRoot "examples\outputs"
$latestOfficialAcceptance = Get-ChildItem -LiteralPath $repoOutputsDir -Directory |
    Where-Object {
        $_.Name -like "acceptance_*_official_root_antimerge" -and
        (Test-Path -LiteralPath (Join-Path $_.FullName "本次验收报告.md"))
    } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if ($latestOfficialAcceptance) {
    Copy-Item -LiteralPath $latestOfficialAcceptance.FullName -Destination (Join-Path $examplesDir "outputs\$($latestOfficialAcceptance.Name)") -Recurse -Force
}
$latestStandaloneAcceptance = Get-ChildItem -LiteralPath $repoOutputsDir -Directory |
    Where-Object {
        $_.Name -like "standalone_acceptance_*" -and
        (Test-Path -LiteralPath (Join-Path $_.FullName "独立软件发布包验收报告.md"))
    } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if ($latestStandaloneAcceptance) {
    Copy-Item -LiteralPath $latestStandaloneAcceptance.FullName -Destination (Join-Path $examplesDir "outputs\$($latestStandaloneAcceptance.Name)") -Recurse -Force
}

foreach ($script in @(
    "run_geoeast_bgpcup2026_test7.ps1",
    "run_smoke_test.ps1",
    "run_official_benchmark.ps1",
    "运行冲顶候选集成入口.ps1",
    "build_geoeast_bo_runner.ps1",
    "build_geoeast_mainframe_plugin.ps1",
    "build_desktop_app.ps1",
    "build_qt_launcher.ps1",
    "打包发布包.ps1",
    "GenerateBuildManifest.ps1",
    "TestWindowsInstaller.ps1",
    "部署到GeoEast.ps1",
    "一键总验收.ps1",
    "验收独立软件发布包.ps1",
    "检查参数一致性.ps1",
    "检查独立软件界面功能.ps1",
    "检查GeoEast插件界面功能.ps1",
    "检查客户机硬编码.ps1",
    "检查GeoEast现场证据.ps1",
    "赛题满分交付审计.ps1",
    "生成答辩评审材料包.ps1",
    "性能回归检查.ps1",
    "visualize_official_result.py",
    "generate_official_quality_report.py",
    "生成性能内存报告.py",
    "生成质量评估图.py",
    "生成漏检质控图.py",
    "生成残余复核层.py",
    "生成前后对比图.py",
    "生成纯断层多边形图.py",
    "顶尖质量诊断.py",
    "全图分块视觉复核.py",
    "冲顶候选集成入口.py",
    "冲顶候选后处理流水线.py",
    "生成冲顶候选证据包.py",
    "局部候选修形实验.py",
    "分支归属拆分实验.py",
    "局部形态质量诊断.py",
    "局部修形拓扑核验.py",
    "冲顶实验晋级验收.py",
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

$gitHead = (& git -C $repoRoot rev-parse HEAD 2>$null | Select-Object -First 1)
$gitStatus = @(& git -C $repoRoot status --porcelain 2>$null)
$sourceSnapshot = Get-SourceSnapshot -Root $repoRoot
$buildId = "FPT-{0}-{1}" -f $Version, $sourceSnapshot.sha256.Substring(0, 16)
$buildIdentity = [ordered]@{
    schemaVersion = 2
    buildId = $buildId
    version = $Version
    generatedAt = (Get-Date).ToString("o")
    gitHead = $gitHead
    gitDirty = ($gitStatus.Count -gt 0)
    gitChangedPathCount = $gitStatus.Count
    gitStatus = $gitStatus
    sourceSnapshot = $sourceSnapshot
    artifacts = @()
    runtimeDependencies = @()
}
foreach ($relative in @(
    "bin\faultpolytrace.exe",
    "FaultPolyTrace.exe",
    "qt_launcher\faultpolytrace_launcher.exe",
    "geoeast_plugin\bo_runner\faultpolytrace_geoeast_runner.exe",
    "geoeast_plugin\mainframe_plugin\built\FaultPolyTraceMainframePlugin.dll"
)) {
    $artifact = Join-Path $packageDir $relative
    if (Test-Path -LiteralPath $artifact -PathType Leaf) {
        $info = Get-Item -LiteralPath $artifact
        $buildIdentity.artifacts += [ordered]@{
            path = $relative.Replace("\", "/")
            sizeBytes = $info.Length
            modifiedAt = $info.LastWriteTime.ToString("o")
            sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $artifact).Hash
        }
    }
}
foreach ($dependency in Get-ChildItem -LiteralPath $packageDir -File -Filter "*.dll") {
    $buildIdentity.runtimeDependencies += [ordered]@{
        name = $dependency.Name
        sizeBytes = $dependency.Length
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $dependency.FullName).Hash
    }
}
$buildIdentity | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $packageDir "构建与依赖清单.json") -Encoding UTF8

$manifest = @()
$manifest += "# FaultPolyTrace 发布包清单"
$manifest += ""
$manifest += "版本：$Version"
$manifest += "统一构建标识：$buildId"
$manifest += "源码快照 SHA256：$($sourceSnapshot.sha256)"
$manifest += "生成时间：$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$manifest += "源码 Git HEAD：$gitHead"
$manifest += "源码工作区状态：$(if ($gitStatus.Count -gt 0) { '含未提交改动，详见构建与依赖清单' } else { '干净' })"
$manifest += ""
$manifest += "## 包含内容"
$manifest += ""
$manifest += '- `bin/faultpolytrace.exe`：离线算法程序'
$manifest += '- `FaultPolyTrace.exe`：独立桌面软件，支持外部网格文件，也可按客户环境配置 GeoEast 数据库连接'
$manifest += '- `qt_launcher/faultpolytrace_launcher.exe`：GeoEast 集成控件，用于数据库读写和平台入口'
$manifest += '- `01_打开图形界面.cmd`：评审推荐图形入口'
$manifest += '- `02_运行离线演示.cmd`：无需 GeoEast 的双击离线演示'
$manifest += '- `03_查看命令行帮助.cmd`：命令行参数帮助'
$manifest += '- `docs/`：中文交付文档'
$manifest += '- `figure/`：界面样式截图和 GeoEast 参考截图'
$manifest += '- `examples/outputs/official/`：官方数据结果、图件、报告依据'
$manifest += '- `examples/outputs/acceptance_*_official_root_antimerge/`：最新官方 DAT 一键验收证据链'
$manifest += '- `examples/outputs/standalone_acceptance_*/`：最新独立软件发布包验收证据链'
$manifest += '- `答辩评审材料包/`：评分项证据索引、答辩演示脚本和交付检查清单'
$manifest += '- `scripts/`：运行、可视化、报告脚本'
$manifest += '- `src/`：核心源码'
$manifest += '- `standalone_app/`：独立桌面软件 Qt 工程源码'
$manifest += '- `geoeast_plugin/`：GeoEast runner、集成控件和主控插件'
$manifest += '- `prerequisites/`：可选运行库安装包'
$manifest += '- `构建与依赖清单.json`：源码版本、工作区状态、二进制 SHA256 和运行库清单'
$manifest += ""
$manifest += "## 当前状态"
$manifest += ""
$manifest += "- 离线 CLI：已构建"
$manifest += "- 独立桌面软件：已构建"
$manifest += "- GeoEast 集成控件：已构建"
$manifest += '- GeoEast 适配器：SDK 编译通过，并具备字段初始化、含洞拒绝、删除状态检查和写后重读校验'
$manifest += '- GeoEast 现场数据库往返：待人工确认；必须在有效许可和目标工区中完成读、写、显示、删除截图'
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
$manifest += '7. 修改独立软件菜单、工具栏、视图切换或主操作按钮后运行 `scripts/检查独立软件界面功能.ps1`，确认没有空控件和漏联动。'
$manifest += '8. 修改独立软件或发布包后运行 `scripts/验收独立软件发布包.ps1 -PackageDir <发布包目录>`，确认 GUI、CLI、根目录转发和输出文件完整。'
$manifest += '9. 修改默认配置、构建脚本或发布脚本后运行 `scripts/检查客户机硬编码.ps1`，确认客户可见链路不含开发机路径、固定用户名和固定测试对象。'
$manifest += '10. 修改算法后运行 `scripts/一键总验收.ps1 -SkipGeoEastBuild -SkipPackage`，确认官方数据仍满足覆盖、防黏连、拓扑和性能门禁。'
$manifest += '11. 在 GeoEast 现场截图前运行 `scripts/检查GeoEast现场证据.ps1 -Initialize`，按模板补齐截图和录屏后再运行该脚本校验。'
$manifest += '12. 提交或打包前运行 `scripts/赛题满分交付审计.ps1`，汇总官方 DAT、独立软件、GeoEast 集成、文档和发布包证据。'
$manifest += '13. 答辩或提交前查看 `答辩评审材料包/评分项证据索引.md`，也可运行 `scripts/生成答辩评审材料包.ps1` 重新生成材料包。'

Set-Content -LiteralPath (Join-Path $packageDir "发布包清单.md") -Value ($manifest -join "`n") -Encoding UTF8

$reviewPackScript = Join-Path $repoRoot "scripts\生成答辩评审材料包.ps1"
if (Test-Path -LiteralPath $reviewPackScript) {
    & $reviewPackScript -OutputDir (Join-Path $packageDir "答辩评审材料包") -PackageDir $packageDir
}

Sanitize-PackageTextFiles $packageDir

$packageRecords = @(Get-ChildItem -LiteralPath $packageDir -Recurse -File |
    Where-Object { $_.Name -ne "构建与依赖清单.json" } |
    ForEach-Object {
        [ordered]@{
            path = $_.FullName.Substring($packageDir.Length).TrimStart('\').Replace('\', '/')
            sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash
        }
    })
$buildIdentity.packageFileCount = $packageRecords.Count
$buildIdentity.packageContentSha256 = Get-AggregateSha256 -Records $packageRecords
$buildIdentity | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $packageDir "构建与依赖清单.json") -Encoding UTF8

$zipPath = Join-Path $releaseRoot ("FaultPolyTrace_" + $Version + ".zip")
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -LiteralPath $packageDir -DestinationPath $zipPath -Force

Write-Host "Release package:" $packageDir
Write-Host "Release zip:" $zipPath
