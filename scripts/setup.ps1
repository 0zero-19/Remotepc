# =============================================================================
# ClassroomMonitor — Dependency Setup & Check
# =============================================================================

param(
    [string]$QtVersion = "6.7.2",
    [string]$QtPath = "C:\Qt",
    [switch]$SkipQt,
    [switch]$SkipCMake
)

$ErrorActionPreference = "Continue"

function Write-Step($message) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "  $message" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host ""
}

function Test-Command($command) {
    try { Get-Command $command -ErrorAction Stop | Out-Null; return $true }
    catch { return $false }
}

# --- 1. Check Visual Studio C++ Compiler ---
Write-Step "1. Checking Visual Studio C++ Tools"
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$hasCpp = $false

if (Test-Path $vsWhere) {
    $vsCppPath = (& $vsWhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
    if ($vsCppPath) {
        Write-Host "  [OK] Visual Studio C++ (MSVC) is installed at: $vsCppPath" -ForegroundColor Green
        $hasCpp = $true
    } else {
        Write-Host "  [!] Visual Studio is installed, but 'Desktop development with C++' is MISSING." -ForegroundColor Yellow
        Write-Host "  Action needed:" -ForegroundColor Yellow
        Write-Host "  1. Open Visual Studio Installer" -ForegroundColor Cyan
        Write-Host "  2. Click 'Modify' (Изменить) on Visual Studio 2022" -ForegroundColor Cyan
        Write-Host "  3. Check 'Desktop development with C++' (Разработка классических приложений на C++)" -ForegroundColor Cyan
        Write-Host "  4. Click 'Modify' in bottom right corner to install." -ForegroundColor Cyan
    }
} else {
    Write-Host "  [X] Visual Studio 2022 Installer not found!" -ForegroundColor Red
    Write-Host "  Please download from: https://visualstudio.microsoft.com/downloads/" -ForegroundColor Yellow
}

# --- 2. Check CMake ---
if (-not $SkipCMake) {
    Write-Step "2. Checking CMake"
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
    if (Test-Command "cmake") {
        $cmakeVer = (cmake --version | Select-Object -First 1)
        Write-Host "  [OK] $cmakeVer" -ForegroundColor Green
    } else {
        Write-Host "  Installing CMake via winget..." -ForegroundColor Yellow
        winget install Kitware.CMake --accept-package-agreements --accept-source-agreements
        $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
        Write-Host "  [OK] CMake installed" -ForegroundColor Green
    }
}

# --- 3. Check Qt6 ---
if (-not $SkipQt) {
    Write-Step "3. Checking Qt $QtVersion"
    $qtBinPath = "$QtPath\$QtVersion\msvc2019_64\bin"

    if (Test-Path $qtBinPath) {
        Write-Host "  [OK] Qt $QtVersion is installed at $QtPath" -ForegroundColor Green
    } else {
        Write-Host "  Qt $QtVersion not found at $QtPath. Installing..." -ForegroundColor Yellow
        if (-not (Test-Command "pip")) {
            Write-Host "  [X] Python/pip not found. Please install Python 3.x" -ForegroundColor Red
        } else {
            pip install aqtinstall
            $aqtExe = "aqt"
            & $aqtExe install-qt windows desktop $QtVersion win64_msvc2019_64 -O $QtPath --modules qtnetworkauth
            if (Test-Path $qtBinPath) {
                Write-Host "  [OK] Qt $QtVersion installed at $QtPath" -ForegroundColor Green
            } else {
                Write-Host "  [X] Qt installation failed." -ForegroundColor Red
            }
        }
    }

    [System.Environment]::SetEnvironmentVariable("CMAKE_PREFIX_PATH", "$QtPath\$QtVersion\msvc2019_64", "User")
    $env:CMAKE_PREFIX_PATH = "$QtPath\$QtVersion\msvc2019_64"
    Write-Host "  CMAKE_PREFIX_PATH set to: $env:CMAKE_PREFIX_PATH" -ForegroundColor Gray
}

# --- Summary ---
Write-Step "Summary"
if ($hasCpp) {
    Write-Host "  Ready to build! Run:" -ForegroundColor Green
    Write-Host "    .\build.bat" -ForegroundColor Yellow
    Write-Host "    .\run_teacher.bat" -ForegroundColor Yellow
} else {
    Write-Host "  Please finish installing 'Desktop development with C++' in Visual Studio Installer," -ForegroundColor Yellow
    Write-Host "  then run .\build.bat" -ForegroundColor Yellow
}
