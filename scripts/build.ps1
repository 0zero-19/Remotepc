# =============================================================================
# ClassroomMonitor — Сборка проекта
# Запуск: .\scripts\build.ps1
# Требует: VS Build Tools 2022, CMake, Qt 6.7.2
# =============================================================================

param(
    [string]$Config = "Debug",
    [string]$QtPath = "C:\Qt\6.7.2\msvc2019_64",
    [switch]$SkipQtInstall
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path $PSScriptRoot -Parent

function Write-Step($msg) {
    Write-Host "`n======================================" -ForegroundColor Cyan
    Write-Host "  $msg" -ForegroundColor Cyan
    Write-Host "======================================`n" -ForegroundColor Cyan
}

function Test-Command($cmd) {
    try { Get-Command $cmd -ErrorAction Stop | Out-Null; return $true }
    catch { return $false }
}

# Обновляем PATH (на случай если CMake только что установили)
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" +
            [System.Environment]::GetEnvironmentVariable("Path","User")

# --- Проверка Visual Studio ---
Write-Step "Проверка Visual Studio 2022"
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vsWhere)) {
    $vsWhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
}
if (Test-Path $vsWhere) {
    $vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    if ($vsPath) {
        Write-Host "  VS найден: $vsPath" -ForegroundColor Green
    } else {
        Write-Host "  VS установлен, но workload VCTools не найден" -ForegroundColor Yellow
        Write-Host "  Запустите: scripts\install_vs_ADMIN.bat (от администратора)" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "  VS 2022 не найден!" -ForegroundColor Red
    Write-Host "  Запустите: scripts\install_vs_ADMIN.bat (от администратора)" -ForegroundColor Red
    exit 1
}

# --- Проверка CMake ---
Write-Step "Проверка CMake"
if (Test-Command "cmake") {
    Write-Host "  CMake: $(cmake --version | Select-Object -First 1)" -ForegroundColor Green
} else {
    Write-Host "  CMake не найден! Устанавливаю..." -ForegroundColor Yellow
    winget install Kitware.CMake --accept-package-agreements --accept-source-agreements
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" +
                [System.Environment]::GetEnvironmentVariable("Path","User")
}

# --- Установка / проверка Qt ---
Write-Step "Проверка Qt 6.7.2"
if (Test-Path "$QtPath\bin\qmake.exe") {
    Write-Host "  Qt найден: $QtPath" -ForegroundColor Green
} elseif (-not $SkipQtInstall) {
    Write-Host "  Qt не найден. Устанавливаю через aqtinstall (~4 GB, 10-20 мин)..." -ForegroundColor Yellow
    pip install aqtinstall --upgrade --quiet
    aqt install-qt windows desktop 6.7.2 win64_msvc2019_64 -O "C:\Qt"
    if (-not (Test-Path "$QtPath\bin\qmake.exe")) {
        Write-Host "  Ошибка установки Qt!" -ForegroundColor Red; exit 1
    }
    Write-Host "  Qt 6.7.2 установлен" -ForegroundColor Green
} else {
    Write-Host "  Qt не найден и -SkipQtInstall задан. Продолжаю (возможны ошибки)..." -ForegroundColor Yellow
}

$env:CMAKE_PREFIX_PATH = $QtPath

# --- CMake Configure ---
Write-Step "Конфигурация CMake ($Config)"
Push-Location $ProjectRoot
$buildDir = "build\$($Config.ToLower())"
cmake -S . -B $buildDir -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_PREFIX_PATH="$QtPath" `
      -DCMAKE_BUILD_TYPE=$Config

if ($LASTEXITCODE -ne 0) { Write-Host "Ошибка CMake configure!" -ForegroundColor Red; exit 1 }

# --- Build ---
Write-Step "Сборка ($Config)"
cmake --build $buildDir --config $Config --parallel

if ($LASTEXITCODE -ne 0) { Write-Host "Ошибка сборки!" -ForegroundColor Red; exit 1 }

# --- Итог ---
Write-Step "Сборка завершена!"
$agentExe  = "$buildDir\client\$Config\StudentAgent.exe"
$panelExe  = "$buildDir\server\$Config\TeacherPanel.exe"

if (Test-Path $agentExe) { Write-Host "  StudentAgent : $agentExe" -ForegroundColor Green }
else                      { Write-Host "  StudentAgent : НЕ НАЙДЕН" -ForegroundColor Yellow }

if (Test-Path $panelExe)  { Write-Host "  TeacherPanel : $panelExe"  -ForegroundColor Green }
else                      { Write-Host "  TeacherPanel : НЕ НАЙДЕН"  -ForegroundColor Yellow }

Write-Host "`n  Для запуска:" -ForegroundColor White
Write-Host "    .\$panelExe"   -ForegroundColor Yellow
Pop-Location
