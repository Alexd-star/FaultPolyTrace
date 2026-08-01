#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$sourceDir = Join-Path $repoRoot "geoeast_plugin\evaluation_bootstrap"
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $repoRoot "build\evaluation_bootstrap\FaultPolyTrace.exe"
}
$outputDirectory = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null

$vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw "未找到 Visual Studio Installer 的 vswhere.exe。"
}
$installation = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1).Trim()
if ([string]::IsNullOrWhiteSpace($installation)) {
    throw "未找到包含 MSVC x64 工具链的 Visual Studio。"
}
$vcvars = Join-Path $installation "VC\Auxiliary\Build\vcvars64.bat"
$environmentLines = & cmd.exe /d /c ('call "{0}" >nul && set' -f $vcvars)
if ($LASTEXITCODE -ne 0) {
    throw "MSVC x64 构建环境初始化失败，退出码 $LASTEXITCODE。"
}
foreach ($line in $environmentLines) {
    if ($line -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
    }
}
if (!(Get-Command cl.exe -ErrorAction SilentlyContinue) -or !(Get-Command rc.exe -ErrorAction SilentlyContinue)) {
    throw "MSVC 或 Windows SDK 工具未进入当前构建环境。"
}

$resourcePath = Join-Path $outputDirectory "FaultPolyTraceEvaluation.res"
$main = Join-Path $sourceDir "main.cpp"
$object = Join-Path $outputDirectory "main.obj"
Push-Location $sourceDir
try {
    & rc.exe /nologo "/fo$resourcePath" "FaultPolyTraceEvaluation.rc"
    if ($LASTEXITCODE -ne 0) { throw "评测启动器资源编译失败，退出码 $LASTEXITCODE。" }
} finally {
    Pop-Location
}

& cl.exe /nologo /std:c++17 /O2 /MT /EHsc /W4 /WX /utf-8 /DUNICODE /D_UNICODE `
    "/Fo$object" "/Fe$OutputPath" $main $resourcePath user32.lib /link /SUBSYSTEM:WINDOWS
if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $OutputPath -PathType Leaf)) {
    throw "评测启动器编译失败，退出码 $LASTEXITCODE。"
}

Write-Host "评测启动器已生成：$OutputPath"
