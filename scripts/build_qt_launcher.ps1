param(
    [string]$GeoEastRoot = $(if ($env:FPT_GEOEAST_ROOT) { $env:FPT_GEOEAST_ROOT } else { "C:\Users\26312\GeoEast" }),
    [string]$QMake = $env:FPT_QMAKE,
    [string]$VcVars = "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
)

$ErrorActionPreference = "Stop"

$projectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$launcherDir = Join-Path $projectRoot "geoeast_plugin\qt_launcher"

if ([string]::IsNullOrWhiteSpace($QMake)) {
    $qmakeCandidates = @(
        (Join-Path $GeoEastRoot "NGP\Qt\Qt5.15.2\5.15.2\msvc2019_64\bin\qmake.exe"),
        "D:\Qt\Qt5.14.2\5.14.2\msvc2017_64\bin\qmake.exe"
    )
    $QMake = $qmakeCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}

if (!(Test-Path -LiteralPath $VcVars)) {
    throw "Cannot find Visual Studio vcvars64.bat: $VcVars"
}
if ([string]::IsNullOrWhiteSpace($QMake) -or !(Test-Path -LiteralPath $QMake)) {
    throw "Cannot find qmake.exe. Set FPT_QMAKE or install GeoEast Qt / D:\Qt\Qt5.14.2."
}

$command = @"
call "$VcVars" && cd /d "$launcherDir" && "$QMake" FaultPolyTraceLauncher.pro -spec win32-msvc && nmake
"@

cmd /c $command

if ($LASTEXITCODE -ne 0) {
    throw "Qt launcher build failed with exit code $LASTEXITCODE."
}

Write-Host "Qt launcher built:" (Join-Path $launcherDir "faultpolytrace_launcher.exe")
