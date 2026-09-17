@echo off
setlocal
cd /d "%~dp0"

:: 1. Если доступен тихий загрузчик VBS — запускаем через wscript и мгновенно закрываемся
if exist "run_student.vbs" (
    start "" wscript //nologo "%~dp0run_student.vbs" %*
    exit /b 0
)

:: 2. Резервный быстрый запуск без задержки на пересборку
set SERVER_IP=%1
if "%SERVER_IP%"=="" (
    if exist "server_ip.txt" (
        set /p SAVED_IP=<server_ip.txt
    )
)
if "%SERVER_IP%"=="" set SERVER_IP=%SAVED_IP%

if exist "build\Debug\StudentAgent.exe" (
    start "" "build\Debug\StudentAgent.exe" %SERVER_IP%
    exit /b 0
)
if exist "build\Release\StudentAgent.exe" (
    start "" "build\Release\StudentAgent.exe" %SERVER_IP%
    exit /b 0
)
if exist "bin\StudentAgent.exe" (
    start "" "bin\StudentAgent.exe" %SERVER_IP%
    exit /b 0
)

:: Если исполняемый файл еще не был скомпилирован — собираем
echo [ClassroomMonitor] Первый запуск: сборка StudentAgent...
call build.bat
if exist "build\Debug\StudentAgent.exe" (
    start "" "build\Debug\StudentAgent.exe" %SERVER_IP%
    exit /b 0
)
if exist "build\Release\StudentAgent.exe" (
    start "" "build\Release\StudentAgent.exe" %SERVER_IP%
    exit /b 0
)

pause
