param(
    [string]$GeoEastRoot = $env:FPT_GEOEAST_ROOT,
    [string]$QMake = $env:FPT_GEOEAST_QMAKE,
    [string]$VcVars = "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($GeoEastRoot)) {
    $GeoEastRoot = "C:\Users\26312\GeoEast"
}

$env:NGP = Join-Path $GeoEastRoot "NGP"
$env:IECO = $env:NGP
$env:NDPDIR = $env:NGP
$env:NGPDIR = $env:NGP
$env:GEOEAST = Join-Path $GeoEastRoot "GeoEast-RC"
$env:BASE_MAKE_PATH = $env:GEOEAST
$env:GGI_MAKE_PATH = $env:GEOEAST
$env:PG_HOME = Join-Path $env:NGP "support\PostgreSQL\product"

if ([string]::IsNullOrWhiteSpace($QMake)) {
    $QMake = Join-Path $env:NGP "Qt\Qt5.15.2\5.15.2\msvc2019_64\bin\qmake.exe"
}

if (!(Test-Path -LiteralPath $VcVars)) {
    throw "Cannot find Visual Studio vcvars64.bat: $VcVars"
}
if (!(Test-Path -LiteralPath $QMake)) {
    throw "Cannot find qmake.exe: $QMake"
}

$projectRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$pluginDir = Join-Path $projectRoot "geoeast_plugin\mainframe_plugin"

$env:PATH = (Join-Path $env:GEOEAST "bin") + ";" +
    (Join-Path $env:GEOEAST "bin\dbus") + ";" +
    (Join-Path $env:NGP "libso\nap") + ";" +
    (Join-Path $env:NGP "libso\ndp") + ";" +
    (Join-Path $env:NGP "libso\common") + ";" +
    (Join-Path $env:NGP "Qt\Qt5.15.2\5.15.2\msvc2019_64\bin") + ";" +
    $env:PATH

$qmakeCommand = @"
call "$VcVars" && cd /d "$pluginDir" && "$QMake" FaultPolyTraceMainframePlugin.pro -spec win32-msvc
"@

cmd /c $qmakeCommand

if ($LASTEXITCODE -ne 0) {
    throw "GeoEast mainframe plugin qmake failed with exit code $LASTEXITCODE."
}

foreach ($makefileName in @("Makefile.Release", "Makefile.Debug", "Makefile")) {
    $makefilePath = Join-Path $pluginDir $makefileName
    if (!(Test-Path -LiteralPath $makefilePath)) {
        continue
    }
    $text = Get-Content -LiteralPath $makefilePath -Raw
    $text = $text.Replace(' dpcpool.lib', ' $(NGP)\libso\ndp\cpool.lib')
    $text = $text.Replace('/LIBPATH:\gdal\lib\release gdal_i.lib', '/LIBPATH:$(NGP)\support\gdal\lib\release gdal_i.lib')
    $text = $text.Replace('/LIBPATH:\gdal\lib\debug gdal_i.lib', '/LIBPATH:$(NGP)\support\gdal\lib\debug gdal_i.lib')
    $text = $text.Replace('/LIBPATH:\iconv\lib\release iconv.lib', '/LIBPATH:$(NGP)\support\iconv\lib\release iconv.lib')
    $text = $text.Replace('/LIBPATH:\iconv\lib\debug iconv.lib', '/LIBPATH:$(NGP)\support\iconv\lib\debug iconv.lib')
    $text = $text.Replace('/LIBPATH:\strptime\lib\release strptime.lib', '/LIBPATH:$(NGP)\support\strptime\lib\release strptime.lib')
    $text = $text.Replace('/LIBPATH:\strptime\lib\debug strptime.lib', '/LIBPATH:$(NGP)\support\strptime\lib\debug strptime.lib')
    $text = $text.Replace('/LIBPATH:\syslog\lib\release syslog.lib', '/LIBPATH:$(NGP)\support\syslog\lib\release syslog.lib')
    $text = $text.Replace('/LIBPATH:\syslog\lib\debug syslog.lib', '/LIBPATH:$(NGP)\support\syslog\lib\debug syslog.lib')
    $text = $text.Replace('/LIBPATH:\thread\lib\release pthreadVC2.lib', '/LIBPATH:$(NGP)\support\thread\lib\release pthreadVC2.lib')
    $text = $text.Replace('/LIBPATH:\thread\lib\debug pthreadVC2d.lib', '/LIBPATH:$(NGP)\support\thread\lib\debug pthreadVC2d.lib')
    $text = $text.Replace('/LIBPATH:\lib libpq.lib', '/LIBPATH:$(PG_HOME)\lib libpq.lib')
    $text = $text.Replace('/LIBPATH:\libxml2-2.7.6\lib\release libxml2.lib', '/LIBPATH:$(NGP)\support\libxml2-2.9.9\lib\release libxml2.lib')
    $text = $text.Replace('/LIBPATH:\libxml2-2.7.6\lib\debug libxml2.lib', '/LIBPATH:$(NGP)\support\libxml2-2.9.9\lib\debug libxml2.lib')
    Set-Content -LiteralPath $makefilePath -Value $text -Encoding UTF8
}

$command = @"
call "$VcVars" && cd /d "$pluginDir" && nmake
"@

cmd /c $command

if ($LASTEXITCODE -ne 0) {
    $partialPluginDir = Join-Path $env:GEOEAST "bin\plugins\GeoAttribute\FaultPolyTraceMainframePlugin"
    if (Test-Path -LiteralPath $partialPluginDir) {
        Remove-Item -LiteralPath $partialPluginDir -Recurse -Force
    }
    throw "GeoEast mainframe plugin build failed with exit code $LASTEXITCODE."
}

Write-Host "GeoEast mainframe plugin built under:" $pluginDir
