@echo off
setlocal
echo ===================================================
echo   Starting ClassroomMonitor - Student Agent
echo ===================================================

if exist "build\Debug\StudentAgent.exe" (
    "build\Debug\StudentAgent.exe"
    exit /b 0
)
if exist "build\Release\StudentAgent.exe" (
    "build\Release\StudentAgent.exe"
    exit /b 0
)
if exist "build\client\Debug\StudentAgent.exe" (
    "build\client\Debug\StudentAgent.exe"
    exit /b 0
)

echo [ERROR] StudentAgent.exe not found! Please run build.bat first.
pause
