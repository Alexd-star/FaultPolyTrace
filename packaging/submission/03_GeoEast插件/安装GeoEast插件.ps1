[CmdletBinding()]
param(
    [string]$GeoEastRoot = $env:FPT_GEOEAST_ROOT
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($GeoEastRoot)) {
    $candidates = @(
        $(if ($env:USERPROFILE) { Join-Path $env:USERPROFILE "GeoEast" } else { $null }),
        "C:\GeoEast",
        "D:\GeoEast",
        $(if ($env:ProgramFiles) { Join-Path $env:ProgramFiles "GeoEast" } else { $null }),
        $(if (${env:ProgramFiles(x86)}) { Join-Path ${env:ProgramFiles(x86)} "GeoEast" } else { $null })
    )
    $GeoEastRoot = $candidates | Where-Object {
        $_ -and
        (Test-Path -LiteralPath (Join-Path $_ "GeoEast-RC") -PathType Container) -and
        (Test-Path -LiteralPath (Join-Path $_ "NGP") -PathType Container)
    } | Select-Object -First 1
}
if ([string]::IsNullOrWhiteSpace($GeoEastRoot)) {
    throw "未自动检测到 GeoEast。请运行：安装GeoEast插件.cmd -GeoEastRoot `"D:\您的GeoEast目录`"，指定同时包含 GeoEast-RC 和 NGP 的根目录。"
}
$GeoEastRoot = (Resolve-Path -LiteralPath $GeoEastRoot).Path

if (Get-Process geoeast -ErrorAction SilentlyContinue) {
    throw "检测到 GeoEast 正在运行。请先保存当前工区并完全退出 GeoEast，再重新安装插件，避免 DLL 被占用或只更新一部分文件。"
}

$packageRoot = $PSScriptRoot
$geoEastRc = Join-Path $GeoEastRoot "GeoEast-RC"
$binDir = Join-Path $geoEastRc "bin"
if (!(Test-Path -LiteralPath $binDir -PathType Container)) {
    throw "GeoEast-RC\bin 不存在：$binDir"
}
if (!(Test-Path -LiteralPath (Join-Path $GeoEastRoot "NGP") -PathType Container)) {
    throw "所选目录缺少 NGP：$GeoEastRoot"
}

$launcherSource = Join-Path $packageRoot "FaultPolyTrace_GeoEastPlugin_{{VERSION}}\Launcher"
$runnerSource = Join-Path $packageRoot "FaultPolyTrace_GeoEastPlugin_{{VERSION}}\BO_Runner\faultpolytrace_geoeast_runner.exe"
$pluginSource = Join-Path $packageRoot "FaultPolyTrace_GeoEastPlugin_{{VERSION}}\Mainframe_Plugin"
$iconSource = Join-Path $packageRoot "FaultPolyTrace_GeoEastPlugin_{{VERSION}}\图标"
$required = @(
    (Join-Path $launcherSource "faultpolytrace_launcher.exe"),
    $runnerSource,
    (Join-Path $pluginSource "FaultPolyTraceMainframePlugin.dll"),
    (Join-Path $pluginSource "manifest.xml")
)
foreach ($path in $required) {
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "插件包缺少文件：$path" }
}

$launcherTarget = Join-Path $binDir "FaultPolyTraceLauncher"
$pluginTarget = Join-Path $binDir "plugins\GeoAttribute\FaultPolyTraceMainframePlugin"
try {
    New-Item -ItemType Directory -Force -Path $launcherTarget, $pluginTarget | Out-Null
} catch {
    throw "无法写入 GeoEast 安装目录：$binDir。请右键以管理员身份运行安装GeoEast插件.cmd，或联系系统管理员授予该目录写入权限。`n$($_.Exception.Message)"
}

$backupRoot = if ($env:LOCALAPPDATA) {
    Join-Path $env:LOCALAPPDATA ("FaultPolyTrace\GeoEastPluginBackups\" + (Get-Date -Format "yyyyMMdd_HHmmss"))
} else {
    Join-Path $env:TEMP ("FaultPolyTrace_GeoEastPluginBackup_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
}
$existingFiles = @(
    (Join-Path $binDir "faultpolytrace_launcher.exe"),
    (Join-Path $binDir "faultpolytrace_geoeast_runner.exe"),
    (Join-Path $pluginTarget "FaultPolyTraceMainframePlugin.dll"),
    (Join-Path $pluginTarget "manifest.xml")
) | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
if ($existingFiles.Count -gt 0) {
    New-Item -ItemType Directory -Force -Path $backupRoot | Out-Null
    foreach ($existing in $existingFiles) {
        Copy-Item -LiteralPath $existing -Destination (Join-Path $backupRoot (Split-Path $existing -Leaf)) -Force
    }
}

$deploymentTargets = @(
    (Join-Path $binDir "faultpolytrace_launcher.exe"),
    (Join-Path $binDir "faultpolytrace_geoeast_runner.exe"),
    (Join-Path $pluginTarget "FaultPolyTraceMainframePlugin.dll"),
    (Join-Path $pluginTarget "manifest.xml")
)
try {
    Get-ChildItem -LiteralPath $launcherSource -Force | Copy-Item -Destination $launcherTarget -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $launcherSource "faultpolytrace_launcher.exe") -Destination (Join-Path $binDir "faultpolytrace_launcher.exe") -Force
    Copy-Item -LiteralPath $runnerSource -Destination (Join-Path $binDir "faultpolytrace_geoeast_runner.exe") -Force
    Copy-Item -LiteralPath $runnerSource -Destination (Join-Path $launcherTarget "faultpolytrace_geoeast_runner.exe") -Force
    Copy-Item -LiteralPath (Join-Path $pluginSource "FaultPolyTraceMainframePlugin.dll") -Destination $pluginTarget -Force
    Copy-Item -LiteralPath (Join-Path $pluginSource "manifest.xml") -Destination $pluginTarget -Force

    $deployedPairs = @(
        @((Join-Path $launcherSource "faultpolytrace_launcher.exe"), (Join-Path $binDir "faultpolytrace_launcher.exe")),
        @($runnerSource, (Join-Path $binDir "faultpolytrace_geoeast_runner.exe")),
        @((Join-Path $pluginSource "FaultPolyTraceMainframePlugin.dll"), (Join-Path $pluginTarget "FaultPolyTraceMainframePlugin.dll"))
    )
    foreach ($pair in $deployedPairs) {
        if ((Get-FileHash -Algorithm SHA256 -LiteralPath $pair[0]).Hash -ne
            (Get-FileHash -Algorithm SHA256 -LiteralPath $pair[1]).Hash) {
            throw "部署后文件哈希不一致：$($pair[1])"
        }
    }
} catch {
    $failure = $_.Exception.Message
    foreach ($existing in $existingFiles) {
        $backup = Join-Path $backupRoot (Split-Path $existing -Leaf)
        if (Test-Path -LiteralPath $backup -PathType Leaf) {
            Copy-Item -LiteralPath $backup -Destination $existing -Force
        }
    }
    foreach ($target in $deploymentTargets) {
        if ($target -notin $existingFiles -and (Test-Path -LiteralPath $target -PathType Leaf)) {
            Remove-Item -LiteralPath $target -Force
        }
    }
    throw "插件部署失败，已恢复安装前的关键文件。原因：$failure"
}

foreach ($name in @("faultpolytrace.ico", "faultpolytrace_32.png")) {
    $source = Join-Path $iconSource $name
    if (Test-Path -LiteralPath $source -PathType Leaf) {
        Copy-Item -LiteralPath $source -Destination $launcherTarget -Force
        Copy-Item -LiteralPath $source -Destination $binDir -Force
    }
}

Write-Host "FaultPolyTrace GeoEast 插件部署完成。"
Write-Host "主框架插件：$pluginTarget"
Write-Host "集成界面：$(Join-Path $binDir 'faultpolytrace_launcher.exe')"
Write-Host "BO Runner：$(Join-Path $binDir 'faultpolytrace_geoeast_runner.exe')"
if ($existingFiles.Count -gt 0) { Write-Host "旧插件备份：$backupRoot" }
Write-Host "请重新启动 GeoEast，并在 Ribbon 中打开“断层多边形追踪”。"



