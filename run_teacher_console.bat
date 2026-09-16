@echo off
setlocal
echo ===================================================
echo   Starting ClassroomMonitor - Teacher Panel (Console Debug)
echo ===================================================

set PATH=C:\Qt\6.7.2\msvc2019_64\bin;%PATH%

if exist "build\Debug\TeacherPanel.exe" (
    "build\Debug\TeacherPanel.exe" --console
    exit /b 0
)
if exist "build\Release\TeacherPanel.exe" (
    "build\Release\TeacherPanel.exe" --console
    exit /b 0
)

echo [ERROR] TeacherPanel.exe not found! Please run build.bat first.
pause
