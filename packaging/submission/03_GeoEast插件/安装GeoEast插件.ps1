[CmdletBinding()]
param(
    [string]$GeoEastRoot = $env:FPT_GEOEAST_ROOT
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($GeoEastRoot)) {
    $candidate = if ($env:USERPROFILE) { Join-Path $env:USERPROFILE "GeoEast" } else { "" }
    if ($candidate -and (Test-Path -LiteralPath (Join-Path $candidate "GeoEast-RC"))) {
        $GeoEastRoot = $candidate
    }
}
if ([string]::IsNullOrWhiteSpace($GeoEastRoot)) {
    throw "未自动检测到 GeoEast。请使用 -GeoEastRoot 参数指定包含 GeoEast-RC 和 NGP 的根目录。"
}

$packageRoot = $PSScriptRoot
$geoEastRc = Join-Path $GeoEastRoot "GeoEast-RC"
$binDir = Join-Path $geoEastRc "bin"
if (!(Test-Path -LiteralPath $binDir -PathType Container)) {
    throw "GeoEast-RC\bin 不存在：$binDir"
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
New-Item -ItemType Directory -Force -Path $launcherTarget, $pluginTarget | Out-Null

Get-ChildItem -LiteralPath $launcherSource -Force | Copy-Item -Destination $launcherTarget -Recurse -Force
Copy-Item -LiteralPath (Join-Path $launcherSource "faultpolytrace_launcher.exe") -Destination (Join-Path $binDir "faultpolytrace_launcher.exe") -Force
Copy-Item -LiteralPath $runnerSource -Destination (Join-Path $binDir "faultpolytrace_geoeast_runner.exe") -Force
Copy-Item -LiteralPath $runnerSource -Destination (Join-Path $launcherTarget "faultpolytrace_geoeast_runner.exe") -Force
Copy-Item -LiteralPath (Join-Path $pluginSource "FaultPolyTraceMainframePlugin.dll") -Destination $pluginTarget -Force
Copy-Item -LiteralPath (Join-Path $pluginSource "manifest.xml") -Destination $pluginTarget -Force

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
Write-Host "请重新启动 GeoEast，并在 Ribbon 中打开“断层多边形追踪”。"



