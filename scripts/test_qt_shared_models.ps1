param(
    [string]$QMake = $(if ($env:FPT_QMAKE) { $env:FPT_QMAKE } else { "" })
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$geoEastRoot = if ($env:GeoEast_HOME) { $env:GeoEast_HOME } elseif ($env:GEOEAST_HOME) { $env:GEOEAST_HOME } else { "D:\GeoEast" }
if ([string]::IsNullOrWhiteSpace($QMake)) {
    $QMake = @(
        (Join-Path $geoEastRoot "NGP\Qt\Qt5.15.2\5.15.2\msvc2019_64\bin\qmake.exe"),
        "D:\Qt\Qt5.14.2\5.14.2\msvc2017_64\bin\qmake.exe"
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if ([string]::IsNullOrWhiteSpace($QMake) -or !(Test-Path -LiteralPath $QMake)) {
    throw "Cannot find qmake.exe. Set FPT_QMAKE or install the project Qt runtime."
}

$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path -LiteralPath $vsWhere)) {
    throw "Cannot find Visual Studio vswhere.exe."
}
$vsRoot = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ([string]::IsNullOrWhiteSpace($vsRoot)) {
    throw "Cannot find a Visual C++ toolchain."
}
$vcVars = Join-Path $vsRoot "VC\Auxiliary\Build\vcvars64.bat"
$qtBin = Split-Path -Parent $QMake
$testDir = Join-Path $repoRoot "tests_cpp"
$buildDir = Join-Path $repoRoot "build\tests\qt_quality_review_model"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$command = @"
call "$vcVars" && set "PATH=$qtBin;!PATH!" && cd /d "$buildDir" && "$QMake" "$testDir\test_quality_review_model.pro" -spec win32-msvc && nmake /NOLOGO && "$buildDir\release\test_quality_review_model.exe"
"@
cmd.exe /d /v:on /s /c $command
if ($LASTEXITCODE -ne 0) {
    throw "Qt shared-model tests failed with exit code $LASTEXITCODE."
}

Write-Host "All Qt shared-model tests passed."
