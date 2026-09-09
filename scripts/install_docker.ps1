# =============================================================================
# install_docker.ps1 — Автоматическая установка Docker Desktop
# Запуск от имени Администратора:
#   Set-ExecutionPolicy Bypass -Scope Process -Force
#   .\scripts\install_docker.ps1
# =============================================================================

$ErrorActionPreference = "Stop"

function Write-Step($msg) {
    Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
    Write-Host "  $msg" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
}

# --- Проверка прав администратора ---
if (-NOT ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Host "❌ Запустите скрипт от имени Администратора!" -ForegroundColor Red
    exit 1
}

Write-Step "Система: $((Get-WmiObject Win32_OperatingSystem).Caption)"

# --- Включить Hyper-V ---
Write-Step "Включение Hyper-V..."
$hyperv = Get-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V-All -ErrorAction SilentlyContinue
if ($hyperv.State -eq "Enabled") {
    Write-Host "✅ Hyper-V уже включён" -ForegroundColor Green
} else {
    Write-Host "📦 Включаю Hyper-V (требует перезагрузки)..." -ForegroundColor Yellow
    Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V -All -NoRestart
    Write-Host "⚠️  Hyper-V включён. После перезагрузки запустите скрипт снова." -ForegroundColor Yellow
}

# --- Включить Containers feature ---
Write-Step "Включение Windows Containers..."
$containers = Get-WindowsOptionalFeature -Online -FeatureName Containers -ErrorAction SilentlyContinue
if ($containers -and $containers.State -eq "Enabled") {
    Write-Host "✅ Containers feature уже включён" -ForegroundColor Green
} else {
    Write-Host "📦 Включаю Containers feature..." -ForegroundColor Yellow
    Enable-WindowsOptionalFeature -Online -FeatureName Containers -All -NoRestart
}

# --- Скачать и установить Docker Desktop ---
Write-Step "Установка Docker Desktop..."
$dockerInstalled = Test-Path "C:\Program Files\Docker\Docker\Docker Desktop.exe"
if ($dockerInstalled) {
    Write-Host "✅ Docker Desktop уже установлен" -ForegroundColor Green
} else {
    $dockerUrl = "https://desktop.docker.com/win/main/amd64/Docker Desktop Installer.exe"
    $installer = "$env:TEMP\DockerDesktopInstaller.exe"

    Write-Host "📥 Скачиваю Docker Desktop (~600MB)..." -ForegroundColor Yellow
    $wc = New-Object System.Net.WebClient
    $wc.DownloadFile($dockerUrl, $installer)

    Write-Host "📦 Устанавливаю Docker Desktop..." -ForegroundColor Yellow
    Start-Process -FilePath $installer -ArgumentList "install --quiet --accept-license" -Wait

    Write-Host "✅ Docker Desktop установлен!" -ForegroundColor Green
    Remove-Item $installer -Force
}

# --- Переключить на Windows Containers ---
Write-Step "Переключение на Windows Containers..."
$dockerCli = "C:\Program Files\Docker\Docker\DockerCli.exe"
if (Test-Path $dockerCli) {
    Write-Host "🔄 Переключаю на Windows Containers..." -ForegroundColor Yellow
    & $dockerCli -SwitchWindowsEngine
    Start-Sleep -Seconds 5
    Write-Host "✅ Переключено на Windows Containers" -ForegroundColor Green
} else {
    Write-Host "⚠️  Docker Desktop не запущен. Запустите вручную, затем:" -ForegroundColor Yellow
    Write-Host '   Правый клик на иконку Docker в трее → "Switch to Windows containers..."' -ForegroundColor White
}

# --- Итог ---
Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Green
Write-Host "  ✅ Готово! Теперь запустите:" -ForegroundColor Green
Write-Host "" 
Write-Host "  cd C:\Users\Mechatronics\Desktop\Remotepc" -ForegroundColor Yellow
Write-Host "  docker-compose build" -ForegroundColor Yellow
Write-Host "  docker-compose run builder" -ForegroundColor Yellow
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Green
