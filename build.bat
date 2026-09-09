@echo off
setlocal
echo ===================================================
echo   ClassroomMonitor - Building Project (MSVC + Qt6)
echo ===================================================

set CMAKE_PREFIX_PATH=C:\Qt\6.7.2\msvc2019_64
set PATH=%PATH%;C:\Program Files\CMake\bin;C:\Qt\6.7.2\msvc2019_64\bin

echo [1/3] Configuring CMake...
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.7.2/msvc2019_64"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] CMake configuration failed!
    echo Make sure 'Desktop development with C++' is installed in Visual Studio.
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [2/3] Compiling Debug configuration...
cmake --build build --config Debug
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build failed!
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [3/3] Deploying Qt6 DLLs to build\Debug...
if exist "C:\Qt\6.7.2\msvc2019_64\bin\windeployqt.exe" (
    "C:\Qt\6.7.2\msvc2019_64\bin\windeployqt.exe" --debug "build\Debug\TeacherPanel.exe" > nul 2>&1
)

echo.
echo ===================================================
echo   BUILD SUCCESSFUL! All DLLs deployed!
echo   Run teacher panel: run_teacher.bat (or double-click build\Debug\TeacherPanel.exe)
echo   Run student agent: run_student.bat (or double-click build\Debug\StudentAgent.exe)
echo ===================================================
pause
