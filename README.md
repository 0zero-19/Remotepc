# ClassroomMonitor (Remotepc)

Система мониторинга и управления компьютерами в учебном классе.  
Преподаватель видит экраны всех студентов в реальном времени и может удалённо управлять ими.

## Возможности

- 🖥️ **Просмотр экранов** — DXGI Desktop Duplication + H.264 сжатие
- 🎮 **Удалённое управление** — мышь и клавиатура через SendInput
- 🔒 **Блокировка ввода** — блокировка клавиатуры/мыши студента с оверлеем
- 📡 **UDP видеопоток** — низкая задержка при передаче
- 🛡️ **TCP команды** — гарантированная доставка управляющих команд

## Архитектура

```
┌─────────────────────┐       UDP (видео)        ┌──────────────────────┐
│   Student Agent     │ ◄──────────────────────► │   Teacher Panel      │
│   (client/)         │       TCP (команды)       │   (server/)          │
│                     │ ◄──────────────────────► │                      │
│ • DXGI Capture      │                           │ • Qt6 GUI            │
│ • H.264 Encoder     │                           │ • Grid View          │
│ • Input Injection   │                           │ • Remote Control     │
│ • Lock Manager      │                           │ • Control Panel      │
└─────────────────────┘                           └──────────────────────┘
          ▲                                                ▲
          │              ┌──────────────┐                  │
          └──────────────│   common/    │──────────────────┘
                         │  Protocol    │
                         │  Network     │
                         └──────────────┘
```

## Требования

- **Windows 10/11** (DXGI Desktop Duplication)
- **Visual Studio 2022** (MSVC compiler)
- **CMake 3.21+**
- **Qt 6.7+** (Widgets, Network)

## Быстрый старт

### Вариант 1: Автоматическая установка (рекомендуется)

```powershell
.\scripts\setup.ps1
```

### Вариант 2: Docker (для сборки)

```powershell
docker-compose build
docker-compose run builder
```

### Вариант 3: Ручная сборка

```powershell
# 1. Настройка среды
$env:CMAKE_PREFIX_PATH = "C:\Qt\6.7.2\msvc2019_64"

# 2. Конфигурация
cmake -S . -B build --preset=debug

# 3. Сборка
cmake --build build --config Debug

# 4. Тесты
ctest --test-dir build
```

## Структура проекта

Подробнее см. [ARCHITECTURE.md](ARCHITECTURE.md) и [API_CONTRACT.md](API_CONTRACT.md).

| Директория | Разработчик | Описание |
|:---|:---|:---|
| `common/` | Backend (создаёт) | Общий протокол и типы |
| `client/` | Backend | Агент на ПК студента |
| `server/` | Frontend | Qt6 GUI панель преподавателя |
| `tests/` | Оба | Юнит-тесты |

## Команда

- **Backend**: client/ + common/ (DXGI, сеть, протокол)
- **Frontend**: server/ (Qt6 GUI, UX)
