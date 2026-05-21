param(
    [string]$GeoEastRoot = $(if ($env:FPT_GEOEAST_ROOT) { $env:FPT_GEOEAST_ROOT } else { "C:\Users\26312\GeoEast" }),
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

function Join-ScriptPathFromCodes([int[]]$Codes) {
    $name = -join ($Codes | ForEach-Object { [char]$_ })
    return Join-Path $PSScriptRoot $name
}

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$geoEastRc = Join-Path $GeoEastRoot "GeoEast-RC"
$pluginTargetDir = Join-Path $geoEastRc "bin\plugins\GeoAttribute\FaultPolyTraceMainframePlugin"
$binTargetDir = Join-Path $geoEastRc "bin"

if (!(Test-Path -LiteralPath $geoEastRc)) {
    throw "GeoEast-RC directory was not found: $geoEastRc"
}

if (!$SkipBuild) {
    # "生成产品图标.ps1" written by code points to avoid Windows console encoding issues.
    $iconScript = Join-ScriptPathFromCodes @(29983,25104,20135,21697,22270,26631,46,112,115,49)
    if (Test-Path -LiteralPath $iconScript) {
        & $iconScript
    }
    & (Join-Path $PSScriptRoot "build_geoeast_bo_runner.ps1") -GeoEastRoot $GeoEastRoot -UseVisualStudioProject
    & (Join-Path $PSScriptRoot "build_qt_launcher.ps1")
    $mainframeScript = Join-Path $PSScriptRoot "build_geoeast_mainframe_plugin.ps1"
    if (Test-Path -LiteralPath $mainframeScript) {
        & $mainframeScript -GeoEastRoot $GeoEastRoot
    }
}

New-Item -ItemType Directory -Force -Path $pluginTargetDir | Out-Null

$launcher = Join-Path $repoRoot "geoeast_plugin\qt_launcher\faultpolytrace_launcher.exe"
$runner = Join-Path $repoRoot "geoeast_plugin\bo_runner\faultpolytrace_geoeast_runner.exe"
$manifest = Join-Path $repoRoot "geoeast_plugin\mainframe_plugin\manifest.xml"
$iconIco = Join-Path $repoRoot "assets\icons\faultpolytrace.ico"
$iconPng = Join-Path $repoRoot "assets\icons\faultpolytrace_32.png"

foreach ($required in @($launcher, $runner, $manifest, $iconIco, $iconPng)) {
    if (!(Test-Path -LiteralPath $required)) {
        throw "Required deployment file is missing: $required"
    }
}

Copy-Item -LiteralPath $launcher -Destination (Join-Path $binTargetDir "faultpolytrace_launcher.exe") -Force
Copy-Item -LiteralPath $runner -Destination (Join-Path $binTargetDir "faultpolytrace_geoeast_runner.exe") -Force
Copy-Item -LiteralPath $manifest -Destination (Join-Path $pluginTargetDir "manifest.xml") -Force
Copy-Item -LiteralPath $iconIco -Destination (Join-Path $binTargetDir "faultpolytrace.ico") -Force
Copy-Item -LiteralPath $iconPng -Destination (Join-Path $binTargetDir "faultpolytrace.png") -Force

$pluginDllSource = Join-Path $repoRoot "geoeast_plugin\mainframe_plugin\FaultPolyTraceMainframePlugin.dll"
if (Test-Path -LiteralPath $pluginDllSource) {
    Copy-Item -LiteralPath $pluginDllSource -Destination (Join-Path $pluginTargetDir "FaultPolyTraceMainframePlugin.dll") -Force
}

Write-Host "GeoEast deployment completed."
Write-Host "Plugin dir: $pluginTargetDir"
Write-Host "Launcher: $(Join-Path $binTargetDir "faultpolytrace_launcher.exe")"
Write-Host "Runner: $(Join-Path $binTargetDir "faultpolytrace_geoeast_runner.exe")"
Write-Host "Icon: $(Join-Path $binTargetDir "faultpolytrace.ico")"
