param(
    [string]$GeoEastRoot = $(if ($env:FPT_GEOEAST_ROOT) { $env:FPT_GEOEAST_ROOT } elseif ($env:USERPROFILE -and (Test-Path -LiteralPath (Join-Path $env:USERPROFILE "GeoEast"))) { Join-Path $env:USERPROFILE "GeoEast" } else { "C:\GeoEast" }),
    [string]$Configuration = "Release",
    [switch]$RegenerateProject,
    [switch]$UseVisualStudioProject
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$projectDir = Join-Path $repoRoot "geoeast_plugin\bo_runner"
$proFile = Join-Path $projectDir "FaultPolyTraceGeoEast.pro"
$vcxproj = Join-Path $projectDir "FaultPolyTraceGeoEast.vcxproj"

$env:NGP = Join-Path $GeoEastRoot "NGP"
$env:IECO = $env:NGP
$env:NDPDIR = $env:NGP
$env:NGPDIR = $env:NGP
$env:GEOEAST = Join-Path $GeoEastRoot "GeoEast-RC"
$env:PG_HOME = Join-Path $env:NGP "support\PostgreSQL\product"

$qtBinCandidates = @(
    (Join-Path $env:NGP "Qt\Qt5.15.2\5.15.2\msvc2019_64\bin"),
    "D:\Qt\Qt5.14.2\5.14.2\msvc2017_64\bin"
)
$qtBin = $qtBinCandidates | Where-Object { Test-Path -LiteralPath (Join-Path $_ "qmake.exe") } | Select-Object -First 1
if (!$qtBin) {
    throw "qmake.exe was not found. Check GeoEast Qt or D:\Qt\Qt5.14.2."
}

$vsDevCmd = "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat"
if (!(Test-Path -LiteralPath $vsDevCmd)) {
    throw "VS2019 VsDevCmd.bat was not found: $vsDevCmd"
}

$env:PATH = (@(
    $qtBin,
    (Join-Path $env:PG_HOME "bin"),
    (Join-Path $env:PG_HOME "lib"),
    (Join-Path $env:GEOEAST "bin"),
    (Join-Path $env:GEOEAST "bin\dbus"),
    (Join-Path $env:NGP "libso\nap"),
    (Join-Path $env:NGP "libso\ndp"),
    (Join-Path $env:NGP "libso\common"),
    (Join-Path $env:NGP "libso\geoscene"),
    (Join-Path $env:NGP "licmng\bin")
) -join ";") + ";" + $env:PATH

$libpqPath = Join-Path $env:PG_HOME "lib\libpq.lib"
if (!(Test-Path -LiteralPath $libpqPath)) {
    throw "libpq.lib was not found: $libpqPath"
}

Push-Location $projectDir
try {
    if ($UseVisualStudioProject -and ($RegenerateProject -or !(Test-Path -LiteralPath $vcxproj))) {
        & (Join-Path $qtBin "qmake.exe") -tp vc $proFile
        if ($LASTEXITCODE -ne 0) {
            throw "qmake failed to generate Visual Studio project. Exit code: $LASTEXITCODE"
        }
    }

    if ($UseVisualStudioProject) {
        & (Join-Path $PSScriptRoot "fix_geoeast_vcxproj_libs.ps1")
        $cmd = "`"$vsDevCmd`" -arch=amd64 -host_arch=amd64 && msbuild `"$vcxproj`" /m /p:Configuration=$Configuration /p:Platform=x64"
        & cmd.exe /c $cmd
        if ($LASTEXITCODE -ne 0) {
            throw "GeoEast BO runner msbuild failed. Exit code: $LASTEXITCODE"
        }
    } else {
        $configAdd = $Configuration.ToLowerInvariant()
        $configRemove = if ($Configuration -ieq "Debug") { "release" } else { "debug" }
        & (Join-Path $qtBin "qmake.exe") $proFile -spec win32-msvc "CONFIG+=$configAdd" "CONFIG-=$configRemove"
        if ($LASTEXITCODE -ne 0) {
            throw "qmake failed to generate Makefile. Exit code: $LASTEXITCODE"
        }

        $cmd = "`"$vsDevCmd`" -arch=amd64 -host_arch=amd64 && nmake /NOLOGO"
        & cmd.exe /c $cmd
        if ($LASTEXITCODE -ne 0) {
            throw "GeoEast BO runner nmake failed. Exit code: $LASTEXITCODE"
        }
    }
}
finally {
    Pop-Location
}

$exe = Join-Path $projectDir "faultpolytrace_geoeast_runner.exe"
if (!(Test-Path -LiteralPath $exe)) {
    $candidate = Join-Path $projectDir "$Configuration\faultpolytrace_geoeast_runner.exe"
    if (Test-Path -LiteralPath $candidate) {
        Copy-Item -LiteralPath $candidate -Destination $exe -Force
    }
}

if (!(Test-Path -LiteralPath $exe)) {
    throw "Build finished but output exe was not found: $exe"
}

Write-Host "GeoEast BO runner built: $exe"
