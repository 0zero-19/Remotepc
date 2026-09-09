@echo off
setlocal
echo ===================================================
echo   ClassroomMonitor - Student Agent
echo ===================================================

set SERVER_IP=%1

if "%SERVER_IP%"=="" (
    if exist "server_ip.txt" (
        set /p SAVED_IP=<server_ip.txt
    )
)

if exist "build\Debug\StudentAgent.exe" (
    "build\Debug\StudentAgent.exe" %SERVER_IP%
    exit /b 0
)
if exist "build\Release\StudentAgent.exe" (
    "build\Release\StudentAgent.exe" %SERVER_IP%
    exit /b 0
)
if exist "build\client\Debug\StudentAgent.exe" (
    "build\client\Debug\StudentAgent.exe" %SERVER_IP%
    exit /b 0
)

echo [ERROR] StudentAgent.exe not found! Please run build.bat first.
pause
