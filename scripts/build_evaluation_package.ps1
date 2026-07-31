#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$Version = "v1.1.0",
    [string]$OutputDir = "",
    [string]$LauncherPayloadDir = "",
    [string]$RunnerExe = "",
    [switch]$SkipBootstrapBuild
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot ("release\FaultPolyTrace_评测运行程序_" + $Version)
}
if ([string]::IsNullOrWhiteSpace($LauncherPayloadDir)) {
    $LauncherPayloadDir = Join-Path $repoRoot "geoeast_plugin\qt_launcher"
}
if ([string]::IsNullOrWhiteSpace($RunnerExe)) {
    $RunnerExe = Join-Path $repoRoot "geoeast_plugin\bo_runner\faultpolytrace_geoeast_runner.exe"
}
$OutputDir = [System.IO.Path]::GetFullPath($OutputDir)
$LauncherPayloadDir = [System.IO.Path]::GetFullPath($LauncherPayloadDir)
$RunnerExe = [System.IO.Path]::GetFullPath($RunnerExe)

$requiredPayload = @(
    "faultpolytrace_launcher.exe",
    "qt.conf",
    "Qt5Core.dll",
    "Qt5Gui.dll",
    "Qt5Widgets.dll",
    "platforms\qwindows.dll"
)
foreach ($relative in $requiredPayload) {
    $path = Join-Path $LauncherPayloadDir $relative
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "评测运行包缺少 Launcher 文件：$path"
    }
}
if (!(Test-Path -LiteralPath $RunnerExe -PathType Leaf)) {
    throw "评测运行包缺少 BO Runner：$RunnerExe"
}

$bootstrap = Join-Path $repoRoot "build\evaluation_bootstrap\FaultPolyTrace.exe"
if (!$SkipBootstrapBuild -or !(Test-Path -LiteralPath $bootstrap -PathType Leaf)) {
    & (Join-Path $PSScriptRoot "build_evaluation_bootstrap.ps1") -OutputPath $bootstrap
}
if (!(Test-Path -LiteralPath $bootstrap -PathType Leaf)) {
    throw "顶层评测启动器不存在：$bootstrap"
}

if (Test-Path -LiteralPath $OutputDir) {
    $resolvedOutput = [System.IO.Path]::GetFullPath($OutputDir)
    $resolvedRelease = [System.IO.Path]::GetFullPath((Join-Path $repoRoot "release"))
    if (!$resolvedOutput.StartsWith($resolvedRelease, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "为避免误删，已有输出目录必须位于仓库 release 目录内：$resolvedOutput"
    }
    Remove-Item -LiteralPath $OutputDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$runtimeDir = Join-Path $OutputDir "pyalgo_faultpolytrace"
New-Item -ItemType Directory -Force -Path $runtimeDir | Out-Null

Copy-Item -LiteralPath $bootstrap -Destination (Join-Path $OutputDir "FaultPolyTrace.exe") -Force
Get-ChildItem -LiteralPath $LauncherPayloadDir -Force | ForEach-Object {
    if ($_.Name -ne "faultpolytrace_launcher.ini") {
        Copy-Item -LiteralPath $_.FullName -Destination $runtimeDir -Recurse -Force
    }
}
Copy-Item -LiteralPath $RunnerExe -Destination (Join-Path $runtimeDir "faultpolytrace_geoeast_runner.exe") -Force
Copy-Item -LiteralPath (Join-Path $repoRoot "packaging\evaluation\评测运行说明.md") -Destination $runtimeDir -Force

$manifestPath = Join-Path $runtimeDir "评测运行包清单.json"
$manifest = [ordered]@{
    schemaVersion = 1
    product = "FaultPolyTrace"
    version = $Version
    generatedAt = (Get-Date).ToString("o")
    rootExecutable = "FaultPolyTrace.exe"
    runtimeDirectory = "pyalgo_faultpolytrace"
    launcherSha256 = (Get-FileHash -LiteralPath (Join-Path $runtimeDir "faultpolytrace_launcher.exe") -Algorithm SHA256).Hash
    runnerSha256 = (Get-FileHash -LiteralPath (Join-Path $runtimeDir "faultpolytrace_geoeast_runner.exe") -Algorithm SHA256).Hash
    bootstrapSha256 = (Get-FileHash -LiteralPath (Join-Path $OutputDir "FaultPolyTrace.exe") -Algorithm SHA256).Hash
    parameterPreset = "recommended-antimerge"
    qualityMode = "production"
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

$hashFile = Join-Path $runtimeDir "SHA256SUMS.txt"
$hashLines = New-Object System.Collections.Generic.List[string]
Get-ChildItem -LiteralPath $OutputDir -Recurse -File |
    Where-Object { $_.FullName -ne $hashFile } |
    Sort-Object FullName |
    ForEach-Object {
        $relative = $_.FullName.Substring($OutputDir.Length).TrimStart('\').Replace('\', '/')
        $hashLines.Add("$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash) *$relative")
    }
[System.IO.File]::WriteAllLines($hashFile, $hashLines, (New-Object System.Text.UTF8Encoding($false)))

$rootItems = @(Get-ChildItem -LiteralPath $OutputDir -Force)
if ($rootItems.Count -ne 2 -or
    !(Test-Path -LiteralPath (Join-Path $OutputDir "FaultPolyTrace.exe") -PathType Leaf) -or
    !(Test-Path -LiteralPath $runtimeDir -PathType Container)) {
    throw "评测包根目录必须严格只有 FaultPolyTrace.exe 和 pyalgo_faultpolytrace。"
}

$zipPath = "$OutputDir.zip"
if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
Compress-Archive -Path (Join-Path $OutputDir "*") -DestinationPath $zipPath -CompressionLevel Optimal
Write-Host "评测运行目录：$OutputDir"
Write-Host "评测运行 ZIP：$zipPath"
