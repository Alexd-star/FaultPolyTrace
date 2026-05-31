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
set "OUTDIR=outputs\offline_demo_%TS%"
mkdir "%OUTDIR%" >nul 2>nul

echo 正在运行离线断层多边形自动追踪演示...
echo 输出目录: %OUTDIR%
echo.

"%~dp0bin\faultpolytrace.exe" ^
  --input "%~dp0examples\synthetic\broken_ridge_fault_grid.txt" ^
  --input-format matrix ^
  --output "%~dp0%OUTDIR%\fault_polygons.csv" ^
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
    echo 诊断文件: %OUTDIR%\fault_polygons.csv.diagnostics.json
)
echo.
pause
