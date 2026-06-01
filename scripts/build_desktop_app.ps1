param(
    [string]$GeoEastRoot = $(if ($env:FPT_GEOEAST_ROOT) { $env:FPT_GEOEAST_ROOT } elseif ($env:USERPROFILE) { Join-Path $env:USERPROFILE "GeoEast" } else { "C:\GeoEast" }),
    [string]$QMake = $env:FPT_QMAKE,
    [string]$VcVars = "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
)

$ErrorActionPreference = "Stop"

$projectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$desktopDir = Join-Path $projectRoot "standalone_app\qt_desktop"

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

$qtBin = Split-Path -Parent $QMake
$windeployQt = Join-Path $qtBin "windeployqt.exe"
$desktopExe = Join-Path $desktopDir "FaultPolyTrace.exe"

function Copy-VcRuntimeIfAvailable {
    param([string]$DestinationDir)

    $vcRuntimeCandidates = @(
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Redist\MSVC\14.29.30133\x64\Microsoft.VC142.CRT",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Redist\MSVC\v142\x64\Microsoft.VC142.CRT"
    )
    $vcRuntimeDir = $vcRuntimeCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($vcRuntimeDir)) {
        Write-Warning "Visual C++ runtime redist directory was not found. Another computer may need vc_redist.x64.exe."
        return
    }

    foreach ($dll in @("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll", "concrt140.dll")) {
        $source = Join-Path $vcRuntimeDir $dll
        if (Test-Path -LiteralPath $source) {
            Copy-Item -LiteralPath $source -Destination (Join-Path $DestinationDir $dll) -Force
        }
    }
}

function Write-QtConf {
    param([string]$DestinationDir)

    Set-Content -LiteralPath (Join-Path $DestinationDir "qt.conf") -Value @(
        "[Paths]",
        "Prefix=.",
        "Plugins=.",
        "Translations=translations"
    ) -Encoding ASCII
}

$command = @"
call "$VcVars" && set "PATH=$qtBin;!PATH!" && cd /d "$desktopDir" && "$QMake" FaultPolyTraceDesktop.pro -spec win32-msvc && nmake
"@

cmd /v:on /c $command

if ($LASTEXITCODE -ne 0) {
    throw "Desktop app build failed with exit code $LASTEXITCODE."
}

if (Test-Path -LiteralPath $windeployQt) {
    & $windeployQt --force $desktopExe
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt failed with exit code $LASTEXITCODE."
    }
    Copy-VcRuntimeIfAvailable $desktopDir
    Write-QtConf $desktopDir
} else {
    Write-Warning "windeployqt.exe was not found next to qmake.exe: $windeployQt"
    Write-QtConf $desktopDir
}

Write-Host "Desktop app built and deployed:" $desktopExe
