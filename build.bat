@echo off
setlocal EnableDelayedExpansion

echo ===================================================
echo   ClassroomMonitor - Building Project
echo ===================================================
echo.

:: -----------------------------------------------
:: Track build results
:: -----------------------------------------------
set "BUILD_OK=0"
set "STUDENT_BUILT=0"
set "TEACHER_BUILT=0"
set "QT_FOUND=0"

:: -----------------------------------------------
:: Find Qt6
:: -----------------------------------------------
set "QT_PATH="
if exist "C:\Qt\6.7.2\msvc2019_64" set "QT_PATH=C:\Qt\6.7.2\msvc2019_64"
if exist "C:\Qt\6.6.3\msvc2019_64" set "QT_PATH=C:\Qt\6.6.3\msvc2019_64"
if exist "C:\Qt\6.5.3\msvc2019_64" set "QT_PATH=C:\Qt\6.5.3\msvc2019_64"
if exist "C:\Qt\6.8.0\msvc2019_64" set "QT_PATH=C:\Qt\6.8.0\msvc2019_64"

if defined QT_PATH (
    set "CMAKE_PREFIX_PATH=!QT_PATH!"
    set "PATH=!PATH!;C:\Program Files\CMake\bin;!QT_PATH!\bin"
    set "QT_FOUND=1"
    echo [OK] Found Qt6: !QT_PATH!
) else (
    set "PATH=%PATH%;C:\Program Files\CMake\bin"
    echo [!!] Qt6 NOT found in C:\Qt\ -- only StudentAgent will be built
)

:: -----------------------------------------------
:: Step 1/3: CMake Configure
:: -----------------------------------------------
echo.
echo ---------------------------------------------------
echo [1/3] Configuring CMake...
echo ---------------------------------------------------

if defined QT_PATH (
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="!QT_PATH!"
) else (
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
)

if errorlevel 1 (
    echo.
    echo [FAIL] CMake configuration FAILED!
    echo        Make sure 'Desktop development with C++' is installed in Visual Studio.
    echo.
    echo ===================================================
    echo   RESULT: BUILD FAILED [cmake configure]
    echo ===================================================
    echo.
    pause
    exit /b 1
)
echo [OK] CMake configuration completed successfully.

:: -----------------------------------------------
:: Step 2/3: Compile
:: -----------------------------------------------
echo.
echo ---------------------------------------------------
echo [2/3] Compiling Debug configuration...
echo ---------------------------------------------------

cmake --build build --config Debug

if errorlevel 1 (
    echo.
    echo [FAIL] Compilation FAILED!
    echo.
    echo ===================================================
    echo   RESULT: BUILD FAILED [compilation]
    echo ===================================================
    echo.
    pause
    exit /b 1
)
echo [OK] Compilation completed.

:: -----------------------------------------------
:: Step 3/3: Deploy Qt DLLs
:: -----------------------------------------------
echo.
echo ---------------------------------------------------
echo [3/3] Deploying DLLs...
echo ---------------------------------------------------

if not defined QT_PATH (
    echo [--] Qt not installed -- deploy skipped.
    goto :check_results
)

if not exist "!QT_PATH!\bin\windeployqt.exe" (
    echo [WARN] windeployqt.exe not found in !QT_PATH!\bin\
    goto :check_results
)

if not exist "build\Debug\TeacherPanel.exe" (
    echo [--] TeacherPanel.exe not found -- deploy skipped.
    goto :check_results
)

echo      Deploying Qt DLLs for TeacherPanel...
"!QT_PATH!\bin\windeployqt.exe" --debug "build\Debug\TeacherPanel.exe"
if errorlevel 1 (
    echo [WARN] windeployqt finished with errors, but build continues.
) else (
    echo [OK] Qt DLLs deployed successfully.
)

:: -----------------------------------------------
:: Check build results
:: -----------------------------------------------
:check_results
echo.
echo ---------------------------------------------------
echo   Checking build artifacts...
echo ---------------------------------------------------

if exist "build\Debug\StudentAgent.exe" (
    set "STUDENT_BUILT=1"
    echo [OK]   StudentAgent.exe  -- BUILT
) else (
    echo [FAIL] StudentAgent.exe  -- NOT FOUND!
)

if "!QT_FOUND!" == "1" (
    if exist "build\Debug\TeacherPanel.exe" (
        set "TEACHER_BUILT=1"
        echo [OK]   TeacherPanel.exe  -- BUILT
    ) else (
        echo [FAIL] TeacherPanel.exe  -- NOT FOUND!
    )
) else (
    echo [--]   TeacherPanel.exe  -- SKIPPED [no Qt6]
)

if exist "build\Debug\ProtocolTests.exe" (
    echo [OK]   ProtocolTests.exe -- BUILT
) else (
    echo [--]   ProtocolTests.exe -- not found
)

:: -----------------------------------------------
:: Final result
:: -----------------------------------------------
echo.

if "!STUDENT_BUILT!" == "1" set "BUILD_OK=1"
if "!QT_FOUND!" == "1" (
    if "!TEACHER_BUILT!" == "0" set "BUILD_OK=0"
)

if "!BUILD_OK!" == "1" (
    echo ===================================================
    echo   [OK] BUILD COMPLETED SUCCESSFULLY!
    echo ===================================================
    if "!STUDENT_BUILT!" == "1" echo   Run student:  run_student.bat
    if "!TEACHER_BUILT!" == "1" echo   Run teacher:  run_teacher.bat
    echo ===================================================
) else (
    echo ===================================================
    echo   [FAIL] BUILD COMPLETED WITH ERRORS!
    echo   See output above for details.
    echo ===================================================
)

echo.
pause
