# =============================================================================
# ClassroomMonitor — Docker Build Environment
# 
# Используется для сборки проекта без ручной установки зависимостей.
# Базовый образ: Windows Server Core с MSVC Build Tools + Qt6.
#
# ВАЖНО: Для запуска этого Docker-файла нужен Docker Desktop в режиме
# Windows Containers (не Linux!). Переключение:
#   Правый клик на Docker Desktop в трее → "Switch to Windows containers..."
#
# Использование:
#   docker-compose build
#   docker-compose run builder
# =============================================================================

# Этап 1: Базовый образ с Build Tools
FROM mcr.microsoft.com/dotnet/framework/sdk:4.8-windowsservercore-ltsc2022 AS buildtools

SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop'; $ProgressPreference = 'SilentlyContinue';"]

# Установка Chocolatey
RUN Set-ExecutionPolicy Bypass -Scope Process -Force; \
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; \
    iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))

# Установка инструментов сборки
RUN choco install -y cmake --installargs 'ADD_CMAKE_TO_PATH=System' ; \
    choco install -y git ; \
    choco install -y python3 --params '/InstallDir:C:\Python312'

# Установка Visual Studio Build Tools с C++ workload
RUN choco install -y visualstudio2022buildtools --package-parameters \
    "'--add Microsoft.VisualStudio.Workload.VCTools \
      --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 \
      --add Microsoft.VisualStudio.Component.Windows11SDK.22621 \
      --includeRecommended --quiet --wait'"

# Установка Qt6 через aqtinstall
RUN pip install aqtinstall ; \
    aqt install-qt windows desktop 6.7.2 win64_msvc2019_64 -O C:\Qt --modules qtnetworkauth

# Переменные окружения
ENV CMAKE_PREFIX_PATH="C:\\Qt\\6.7.2\\msvc2019_64" \
    Qt6_DIR="C:\\Qt\\6.7.2\\msvc2019_64\\lib\\cmake\\Qt6" \
    PATH="C:\\Qt\\6.7.2\\msvc2019_64\\bin;C:\\Program Files\\CMake\\bin;${PATH}"

# Этап 2: Сборка проекта
FROM buildtools AS builder

WORKDIR C:\\project
COPY . .

# Скрипт сборки — запускает vcvarsall и cmake
RUN Write-Host '=== Configuring ===' ; \
    cmake -S . -B build -G 'Visual Studio 17 2022' -A x64 \
      -DCMAKE_PREFIX_PATH='C:/Qt/6.7.2/msvc2019_64' ; \
    Write-Host '=== Building ===' ; \
    cmake --build build --config Release ; \
    Write-Host '=== Done ==='

# Артефакты сборки
RUN New-Item -ItemType Directory -Force -Path C:\output ; \
    Copy-Item build\Release\*.exe C:\output\ -ErrorAction SilentlyContinue ; \
    Copy-Item build\client\Release\*.exe C:\output\ -ErrorAction SilentlyContinue ; \
    Copy-Item build\server\Release\*.exe C:\output\ -ErrorAction SilentlyContinue

CMD ["powershell", "-NoExit", "-Command", "Write-Host 'Build complete. Binaries in C:\\output'"]
