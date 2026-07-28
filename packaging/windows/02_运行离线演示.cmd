@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

if not exist "bin\faultpolytrace.exe" (
    echo 未找到 bin\faultpolytrace.exe
    echo 请确认正在从完整发布包根目录运行本脚本。
    pause
    exit /b 1
)

if not exist "examples\synthetic\broken_ridge_fault_grid.txt" (
    echo 未找到离线演示数据 examples\synthetic\broken_ridge_fault_grid.txt
    pause
    exit /b 1
)

for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set TS=%%i
for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "[Console]::Write([Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments))"`) do set "DOCROOT=%%i"
if not defined DOCROOT set "DOCROOT=%LOCALAPPDATA%"
if not defined DOCROOT set "DOCROOT=%TEMP%"
set "OUTDIR=%DOCROOT%\FaultPolyTraceOutputs\offline_demo_%TS%"
mkdir "%OUTDIR%" >nul 2>nul
if errorlevel 1 (
    set "OUTDIR=%TEMP%\FaultPolyTraceOutputs\offline_demo_%TS%"
    mkdir "%OUTDIR%" >nul 2>nul
)
if not exist "%OUTDIR%" (
    echo 无法创建演示输出目录，请检查当前用户的文档目录和临时目录权限。
    pause
    exit /b 1
)

echo 正在运行离线断层多边形自动追踪演示...
echo 输出目录: %OUTDIR%
echo.

"%~dp0bin\faultpolytrace.exe" ^
  --input "%~dp0examples\synthetic\broken_ridge_fault_grid.txt" ^
  --input-format matrix ^
  --output "%OUTDIR%\fault_polygons.csv" ^
  --diagnostics "%OUTDIR%\fault_polygons.diagnostics.json" ^
  --topology-output "%OUTDIR%\fault_polygons.topology.csv" ^
  --branches-output "%OUTDIR%\fault_polygons.branches.csv" ^
  --trace-mode ridge-belt ^
  --threshold-mode percentile ^
  --percentile 0.98 ^
  --min-area 20 ^
  --root-anti-merge ^
  --residual-edge-fill-radius 2 ^
  --residual-edge-fill-min-value-ratio 0.97 ^
  --residual-edge-fill-max-patch-span 5

echo.
if errorlevel 1 (
    echo 离线演示运行失败，请把本窗口内容和输出目录一起提供给技术人员。
) else (
    echo 离线演示运行完成。
    echo 结果文件: %OUTDIR%\fault_polygons.csv
    echo 诊断文件: %OUTDIR%\fault_polygons.diagnostics.json
    echo 拓扑检查: %OUTDIR%\fault_polygons.topology.csv
    echo 分支结果: %OUTDIR%\fault_polygons.branches.csv
)
echo.
pause
