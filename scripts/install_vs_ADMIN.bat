@echo off
:: ============================================================
:: Запустить КАК АДМИНИСТРАТОР (ПКМ -> "Запуск от имени администратора")
:: ============================================================
echo.
echo ==========================================
echo   Установка Visual Studio Build Tools 2022
echo ==========================================
echo.

:: Проверка прав
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [ОШИБКА] Нужны права АДМИНИСТРАТОРА!
    echo.
    echo Закройте это окно и запустите файл
    echo ПРАВОЙ КНОПКОЙ -> "Запуск от имени администратора"
    pause
    exit /b 1
)

echo [OK] Права администратора подтверждены
echo.

set INSTALLER=%TEMP%\vs_BuildTools.exe

if not exist "%INSTALLER%" (
    echo Скачиваю установщик...
    powershell -Command "Invoke-WebRequest -Uri 'https://aka.ms/vs/17/release/vs_BuildTools.exe' -OutFile '%INSTALLER%' -UseBasicParsing"
)

echo Запускаю установщик Visual Studio Build Tools...
echo (это займет 15-30 минут)
echo.

"%INSTALLER%" --passive ^
    --add Microsoft.VisualStudio.Workload.VCTools ^
    --add Microsoft.VisualStudio.Component.Windows11SDK.22621 ^
    --includeRecommended ^
    --wait

if %errorLevel% equ 0 (
    echo.
    echo [УСПЕХ] Visual Studio Build Tools 2022 установлен!
    echo.
    echo Следующий шаг - запустите в PowerShell:
    echo   cd C:\Users\Mechatronics\Desktop\Remotepc
    echo   .\scripts\build.ps1
) else (
    echo.
    echo [ОШИБКА] Код ошибки: %errorLevel%
)

pause
