# =============================================================================
# ClassroomMonitor — Автоматическая установка зависимостей
#
# Запуск:
#   .\scripts\setup.ps1
#
# Устанавливает: CMake, Qt6, проверяет Visual Studio
# =============================================================================

param(
    [string]$QtVersion = "6.7.2",
    [string]$QtPath = "C:\Qt",
    [switch]$SkipQt,
    [switch]$SkipCMake
)

$ErrorActionPreference = "Stop"

function Write-Step($message) {
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "  $message" -ForegroundColor Cyan
    Write-Host "========================================`n" -ForegroundColor Cyan
}

function Test-Command($command) {
    try { Get-Command $command -ErrorAction Stop; return $true }
    catch { return $false }
}

# --- Проверка Visual Studio ---
Write-Step "Проверка Visual Studio 2022"
$vsPath = "C:\Program Files\Microsoft Visual Studio\2022"
if (Test-Path $vsPath) {
    $edition = (Get-ChildItem $vsPath -Directory | Select-Object -First 1).Name
    Write-Host "  ✅ Visual Studio 2022 $edition найден" -ForegroundColor Green
} else {
    Write-Host "  ❌ Visual Studio 2022 не найден!" -ForegroundColor Red
    Write-Host "  Скачайте: https://visualstudio.microsoft.com/downloads/" -ForegroundColor Yellow
    Write-Host "  Установите workload 'Desktop development with C++'" -ForegroundColor Yellow
    exit 1
}

# --- Установка CMake ---
if (-not $SkipCMake) {
    Write-Step "Установка CMake"
    if (Test-Command "cmake") {
        $cmakeVer = (cmake --version | Select-Object -First 1)
        Write-Host "  ✅ CMake уже установлен: $cmakeVer" -ForegroundColor Green
    } else {
        Write-Host "  📦 Устанавливаю CMake через winget..." -ForegroundColor Yellow
        winget install Kitware.CMake --accept-package-agreements --accept-source-agreements
        # Обновляем PATH для текущей сессии
        $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
        Write-Host "  ✅ CMake установлен" -ForegroundColor Green
    }
}

# --- Установка Qt6 ---
if (-not $SkipQt) {
    Write-Step "Установка Qt $QtVersion"
    $qtBinPath = "$QtPath\$QtVersion\msvc2019_64\bin"

    if (Test-Path $qtBinPath) {
        Write-Host "  ✅ Qt $QtVersion уже установлен в $QtPath" -ForegroundColor Green
    } else {
        # Проверяем pip/python
        if (-not (Test-Command "pip")) {
            Write-Host "  ❌ Python/pip не найден! Установите Python 3.x" -ForegroundColor Red
            exit 1
        }

        Write-Host "  📦 Устанавливаю aqtinstall..." -ForegroundColor Yellow
        pip install aqtinstall

        $aqtPath = "$env:APPDATA\Python\Python312\Scripts\aqt.exe"
        if (-not (Test-Path $aqtPath)) {
            $aqtPath = "aqt"  # Попробуем из PATH
        }

        Write-Host "  📦 Скачиваю Qt $QtVersion (это займёт несколько минут)..." -ForegroundColor Yellow
        & $aqtPath install-qt windows desktop $QtVersion win64_msvc2019_64 -O $QtPath --modules qtnetworkauth

        if (Test-Path $qtBinPath) {
            Write-Host "  ✅ Qt $QtVersion установлен в $QtPath" -ForegroundColor Green
        } else {
            Write-Host "  ❌ Ошибка установки Qt!" -ForegroundColor Red
            exit 1
        }
    }

    # Устанавливаем переменную окружения
    [System.Environment]::SetEnvironmentVariable("CMAKE_PREFIX_PATH", "$QtPath\$QtVersion\msvc2019_64", "User")
    $env:CMAKE_PREFIX_PATH = "$QtPath\$QtVersion\msvc2019_64"
    Write-Host "  📝 CMAKE_PREFIX_PATH = $env:CMAKE_PREFIX_PATH" -ForegroundColor Gray
}

# --- Итог ---
Write-Step "Установка завершена!"
Write-Host "  Для сборки проекта выполните:" -ForegroundColor White
Write-Host ""
Write-Host '    cmake -S . -B build --preset=debug' -ForegroundColor Yellow
Write-Host '    cmake --build build --config Debug' -ForegroundColor Yellow
Write-Host ""
Write-Host "  Или в Visual Studio:" -ForegroundColor White
Write-Host "    File → Open → CMake... → выберите CMakeLists.txt" -ForegroundColor Yellow
Write-Host ""
