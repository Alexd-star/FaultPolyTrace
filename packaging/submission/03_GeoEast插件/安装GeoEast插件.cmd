@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0安装GeoEast插件.ps1" %*
if errorlevel 1 (
    echo.
    echo 插件安装未完成。请根据上方提示处理后重试。
    echo 如果提示没有写入权限，请右键本文件并选择“以管理员身份运行”。
    pause
    exit /b 1
)

echo.
echo 插件安装完成。请重新启动 GeoEast。
pause
