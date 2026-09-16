@echo off
setlocal
echo ===================================================
echo   ClassroomMonitor - Student Agent (Console Debug Mode)
echo ===================================================

set SERVER_IP=%1

if "%SERVER_IP%"=="" (
    if exist "server_ip.txt" (
        set /p SAVED_IP=<server_ip.txt
    )
)

where cmake >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [1/2] Проверка и сборка последней версии StudentAgent...
    if not exist "build" (
        cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    )
    cmake --build build --config Debug --target StudentAgent
)

echo [2/2] Запуск Агента Студента с живой консолью...
if exist "build\Debug\StudentAgent.exe" (
    "build\Debug\StudentAgent.exe" --console %SERVER_IP%
    exit /b 0
)
if exist "build\Release\StudentAgent.exe" (
    "build\Release\StudentAgent.exe" --console %SERVER_IP%
    exit /b 0
)

echo [ERROR] StudentAgent.exe not found! Please run build.bat first.
pause
