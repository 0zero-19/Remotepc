@echo off
setlocal
echo ===================================================
echo   Starting ClassroomMonitor - Teacher Panel (Qt6)
echo ===================================================

set PATH=C:\Qt\6.7.2\msvc2019_64\bin;%PATH%

if exist "build\Debug\TeacherPanel.exe" (
    start "" "build\Debug\TeacherPanel.exe"
    exit /b 0
)
if exist "build\Release\TeacherPanel.exe" (
    start "" "build\Release\TeacherPanel.exe"
    exit /b 0
)
if exist "build\server\Debug\TeacherPanel.exe" (
    start "" "build\server\Debug\TeacherPanel.exe"
    exit /b 0
)

echo [ERROR] TeacherPanel.exe not found! Please run build.bat first.
pause
