# =============================================================================
# Установка Visual Studio 2022 Build Tools с правами администратора
# Запуск: Правой кнопкой -> "Запуск от имени администратора"
# =============================================================================

$ErrorActionPreference = "Continue"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Установка VS Build Tools 2022" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# Проверка прав администратора
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "`n❌ Нужны права администратора!" -ForegroundColor Red
    Write-Host "   Закройте этот скрипт и запустите его снова:" -ForegroundColor Yellow
    Write-Host "   Правой кнопкой на файл -> 'Запуск от имени администратора'" -ForegroundColor Yellow
    Read-Host "`nНажмите Enter для выхода"
    exit 1
}

Write-Host "`n✅ Права администратора подтверждены" -ForegroundColor Green

# Скачать и запустить установщик VS Build Tools напрямую
$installerUrl = "https://download.visualstudio.microsoft.com/download/pr/f8d58e41-102a-4347-a567-60d7235da3b5/aac092d0d839fd078e86b301d886130f1605891061242b36facf55ccbdd5a0a7/vs_BuildTools.exe"
$installerPath = "$env:TEMP\vs_BuildTools.exe"

Write-Host "`n📦 Скачиваю VS Build Tools installer..." -ForegroundColor Yellow

# Используем winget (уже скачан кеш)
winget install Microsoft.VisualStudio.2022.BuildTools `
    --accept-package-agreements `
    --accept-source-agreements `
    --override "--passive --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.Windows11SDK.22621 --includeRecommended"

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n✅ VS Build Tools 2022 успешно установлен!" -ForegroundColor Green
} else {
    Write-Host "`n⚠️ winget завершился с кодом $LASTEXITCODE" -ForegroundColor Yellow
    Write-Host "   Проверим наличие VS..." -ForegroundColor Yellow

    if (Test-Path "C:\Program Files\Microsoft Visual Studio\2022") {
        Write-Host "   ✅ VS найден по пути C:\Program Files\Microsoft Visual Studio\2022" -ForegroundColor Green
    } else {
        Write-Host "`n   Попытка прямой загрузки..." -ForegroundColor Yellow
        Invoke-WebRequest -Uri $installerUrl -OutFile $installerPath -UseBasicParsing
        Start-Process -FilePath $installerPath -ArgumentList "--passive --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.Windows11SDK.22621 --includeRecommended" -Wait
    }
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  Следующий шаг: установка Qt6" -ForegroundColor Cyan
Write-Host "  Запустите: .\scripts\setup.ps1 -SkipCMake" -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Cyan
Read-Host "`nНажмите Enter для выхода"
