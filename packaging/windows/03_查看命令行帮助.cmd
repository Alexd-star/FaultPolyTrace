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

"%~dp0bin\faultpolytrace.exe" --help
echo.
pause
