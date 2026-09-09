# ClassroomMonitor — Архитектура

> Этот документ описывает полную архитектуру системы.
> Предназначен для разработчиков и AI-помощников, чтобы понимать как все компоненты связаны.

## Обзор системы

ClassroomMonitor — клиент-серверная система для мониторинга и управления
компьютерами в учебном классе. Состоит из двух исполняемых файлов:

1. **StudentAgent** (`client/`) — фоновый процесс на ПК каждого студента
2. **TeacherPanel** (`server/`) — GUI-приложение на ПК преподавателя

## Потоки данных

```
Student PC #1 ──────────────────────────────────────── Teacher PC
Student PC #2 ──────────────────────────────────────── (TeacherPanel)
Student PC #N ──────────────────────────────────────── 

Каждый StudentAgent:
  1. Захватывает экран через DXGI Desktop Duplication
  2. Кодирует кадр в H.264 (Media Foundation)
  3. Отправляет по UDP на TeacherPanel
  4. Слушает TCP-команды от TeacherPanel

TeacherPanel:
  1. Принимает UDP-видеопотоки от всех агентов
  2. Декодирует и отображает в виде сетки (QGridLayout)
  3. Отправляет TCP-команды (блокировка, управление, и т.д.)
```

## Сетевая модель

### UDP — Видеопоток (порт 9100)

```
StudentAgent → TeacherPanel

Пакет:
┌──────────────────────────────────────────────────────┐
│ PacketHeader (16 bytes)                              │
│   magic: uint32  = 0x434D4F4E ("CMON")               │
│   type:  uint16  = VIDEO_FRAME                        │
│   seq:   uint32  = sequence number                    │
│   size:  uint32  = payload size                       │
│   flags: uint16  = compression flags                  │
├──────────────────────────────────────────────────────┤
│ VideoFrameHeader (24 bytes)                          │
│   clientId:   uint32                                  │
│   timestamp:  uint64                                  │
│   width:      uint16                                  │
│   height:     uint16                                  │
│   frameType:  uint8  (I-frame / P-frame)             │
│   quality:    uint8                                   │
│   reserved:   uint16                                  │
├──────────────────────────────────────────────────────┤
│ H.264 encoded frame data (variable)                  │
└──────────────────────────────────────────────────────┘
```

### TCP — Управляющие команды (порт 9101)

```
TeacherPanel ↔ StudentAgent (двунаправленный)

Команды Teacher → Student:
  - LOCK_INPUT         блокировка клавиатуры/мыши
  - UNLOCK_INPUT       разблокировка
  - MOUSE_MOVE         перемещение курсора
  - MOUSE_CLICK        клик мыши
  - KEY_PRESS          нажатие клавиши
  - KEY_RELEASE        отпускание клавиши
  - REQUEST_SCREENSHOT запрос скриншота
  - SET_QUALITY        изменение качества потока
  - SHUTDOWN_AGENT     завершение агента

Команды Student → Teacher:
  - HANDSHAKE          регистрация агента (имя, IP, разрешение экрана)
  - HEARTBEAT          жив/мёртв
  - ACK                подтверждение команды
```

## Модульная структура

### common/ — Общая библиотека (CommonLib)

**Тип:** Статическая библиотека (`STATIC`)
**Зависимости:** Ws2_32 (Windows Sockets)

```
common/
├── include/common/
│   ├── Protocol.h       ← Все типы пакетов, enum CommandType, структуры
│   ├── NetworkTypes.h   ← Конфигурация портов, адресов
│   └── VideoTypes.h     ← Типы видеокадров, параметры кодека
└── src/
    └── Protocol.cpp     ← Сериализация пакетов в байты
```

**Кто использует:** client/ и server/

### client/ — Агент студента (StudentAgent)

**Тип:** Исполняемый файл (WIN32 — без консоли)
**Зависимости:** CommonLib, DXGI, D3D11, MFPlat, MFReadWrite, User32

```
client/
├── include/client/
│   ├── ScreenCapturer.h   ← DXGI Desktop Duplication API
│   ├── VideoEncoder.h     ← Media Foundation H.264
│   ├── InputInjector.h    ← SendInput для удалённого управления
│   └── LockManager.h      ← Низкоуровневые хуки + оверлей блокировки
└── src/
    ├── ScreenCapturer.cpp
    ├── VideoEncoder.cpp
    ├── InputInjector.cpp
    ├── LockManager.cpp
    └── main.cpp           ← Цикл: захват → кодирование → отправка
```

### server/ — Панель преподавателя (TeacherPanel)

**Тип:** Исполняемый файл (Qt6 application)
**Зависимости:** CommonLib, Qt6::Core, Qt6::Gui, Qt6::Widgets, Qt6::Network

```
server/
├── include/server/
│   ├── MainWindow.h       ← QMainWindow с сеткой студентов
│   ├── StudentTile.h      ← QWidget для одного студента (видео + имя + статус)
│   ├── ClientSession.h    ← QObject управляющий TCP+UDP сессией с агентом
│   └── ControlPanel.h     ← QWidget с кнопками управления
└── src/
    ├── MainWindow.cpp
    ├── StudentTile.cpp
    ├── ClientSession.cpp
    ├── ControlPanel.cpp
    └── main.cpp
```

## Жизненный цикл соединения

```
1. TeacherPanel запускается, слушает TCP:9101 и UDP:9100
2. StudentAgent запускается, подключается по TCP к TeacherPanel
3. StudentAgent отправляет HANDSHAKE (имя ПК, разрешение экрана)
4. TeacherPanel создаёт ClientSession и StudentTile для нового агента
5. StudentAgent начинает захват экрана и отправку кадров по UDP
6. TeacherPanel принимает кадры, декодирует, отображает в StudentTile
7. Преподаватель может:
   - Кликнуть на StudentTile → полноэкранный просмотр + управление
   - Нажать "Заблокировать всех" → LOCK_INPUT по TCP всем агентам
   - Использовать мышь/клавиатуру → MOUSE_MOVE/KEY_PRESS по TCP
8. HEARTBEAT каждые 3 секунды, иначе студент помечается как offline
```

## Сборочные зависимости

| Зависимость | Версия | Где используется |
|:---|:---|:---|
| Qt6 | 6.7.2 | server/ |
| DXGI | Windows SDK | client/ |
| D3D11 | Windows SDK | client/ |
| Media Foundation | Windows SDK | client/ |
| Winsock2 | Windows SDK | common/, client/, server/ |
| MSVC | VS 2022 | Все |
| CMake | 3.21+ | Сборка |
