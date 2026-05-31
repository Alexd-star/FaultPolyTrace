@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

if not exist "qt_launcher\faultpolytrace_launcher.exe" (
    echo 未找到 qt_launcher\faultpolytrace_launcher.exe
    echo 请确认正在从完整发布包根目录运行本脚本。
    pause
    exit /b 1
)

start "" "%~dp0qt_launcher\faultpolytrace_launcher.exe"
