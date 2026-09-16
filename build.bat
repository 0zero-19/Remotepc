@echo off
setlocal
echo ===================================================
echo   ClassroomMonitor - Building Project
echo ===================================================

:: Поиск Qt6 в стандартных путях
set QT_PATH=
if exist "C:\Qt\6.7.2\msvc2019_64" set QT_PATH=C:\Qt\6.7.2\msvc2019_64
if exist "C:\Qt\6.6.3\msvc2019_64" set QT_PATH=C:\Qt\6.6.3\msvc2019_64
if exist "C:\Qt\6.5.3\msvc2019_64" set QT_PATH=C:\Qt\6.5.3\msvc2019_64
if exist "C:\Qt\6.8.0\msvc2019_64" set QT_PATH=C:\Qt\6.8.0\msvc2019_64

if not "%QT_PATH%"=="" (
    set CMAKE_PREFIX_PATH=%QT_PATH%
    set PATH=%PATH%;C:\Program Files\CMake\bin;%QT_PATH%\bin
    echo [i] Найдена установка Qt6: %QT_PATH%
) else (
    set PATH=%PATH%;C:\Program Files\CMake\bin
    echo [i] Qt6 не найден в C:\Qt\ - будет собран только StudentAgent (клиент)
)

echo.
echo [1/3] Configuring CMake...
if not "%QT_PATH%"=="" (
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%QT_PATH%"
) else (
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
)

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
echo [3/3] Deploying DLLs...
if not "%QT_PATH%"=="" (
    if exist "%QT_PATH%\bin\windeployqt.exe" (
        "%QT_PATH%\bin\windeployqt.exe" --debug "build\Debug\TeacherPanel.exe" > nul 2>&1
    )
)

echo.
echo ===================================================
echo   СБОРКА УСПЕШНО ЗАВЕРШЕНА!
echo   Запуск преподавателя: run_teacher.bat
echo   Запуск студента:      run_student.bat
echo ===================================================
pause

