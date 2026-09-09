# API Contract — Frontend ↔ Backend

> Этот документ — контракт между frontend (server/) и backend (client/ + common/).
> Оба разработчика должны следовать этому контракту.
> **Любые изменения в этом файле должны быть согласованы обеими сторонами.**

## Обзор

Frontend и Backend общаются через модуль `common/`:
- **Backend** создаёт и поддерживает `common/include/common/*.h`
- **Frontend** использует эти заголовки как API для работы с сетью и протоколом

```
Backend (client/)                     Frontend (server/)
     │                                       │
     │      common/include/common/           │
     │  ┌──────────────────────────────┐     │
     ├──│ Protocol.h     ← КОНТРАКТ   │─────┤
     ├──│ NetworkTypes.h ← КОНТРАКТ   │─────┤
     ├──│ VideoTypes.h   ← КОНТРАКТ   │─────┤
     │  └──────────────────────────────┘     │
     │                                       │
```

## Порты и протоколы

| Протокол | Порт | Направление | Назначение |
|:---|:---|:---|:---|
| UDP | 9100 | Agent → Teacher | Видеопоток (H.264 кадры) |
| TCP | 9101 | Agent ↔ Teacher | Управляющие команды |

## Формат пакетов

### PacketHeader (общий для всех пакетов)

```cpp
// common/include/common/Protocol.h

struct PacketHeader {
    uint32_t magic;      // = 0x434D4F4E ("CMON")
    uint16_t type;       // PacketType enum
    uint32_t sequence;   // Порядковый номер
    uint32_t payloadSize;// Размер данных после заголовка
    uint16_t flags;      // Битовые флаги
};
// Размер: 16 bytes, упаковка: #pragma pack(push, 1)
```

### Типы пакетов (PacketType)

```cpp
enum class PacketType : uint16_t {
    // --- TCP команды Teacher → Agent ---
    LOCK_INPUT       = 0x0001,
    UNLOCK_INPUT     = 0x0002,
    MOUSE_MOVE       = 0x0010,
    MOUSE_CLICK      = 0x0011,
    MOUSE_SCROLL     = 0x0012,
    KEY_PRESS        = 0x0020,
    KEY_RELEASE      = 0x0021,
    SET_QUALITY      = 0x0030,
    REQUEST_SCREENSHOT = 0x0040,
    SHUTDOWN_AGENT   = 0x00FF,

    // --- TCP команды Agent → Teacher ---
    HANDSHAKE        = 0x0100,
    HEARTBEAT        = 0x0101,
    ACK              = 0x0102,

    // --- UDP данные Agent → Teacher ---
    VIDEO_FRAME      = 0x0200,
};
```

## Командные структуры

### HANDSHAKE (Agent → Teacher, TCP)

```cpp
struct HandshakePayload {
    char     hostname[64];    // Имя ПК студента
    char     username[64];    // Имя пользователя
    uint16_t screenWidth;     // Разрешение экрана X
    uint16_t screenHeight;    // Разрешение экрана Y
    uint16_t agentVersion;    // Версия агента
    uint16_t reserved;
};
```

**Frontend обязан:** Создать `ClientSession` и `StudentTile` при получении.

### MOUSE_MOVE (Teacher → Agent, TCP)

```cpp
struct MouseMovePayload {
    float normalizedX;  // 0.0 — 1.0 (относительно экрана студента)
    float normalizedY;  // 0.0 — 1.0
};
```

**Backend обязан:** Преобразовать нормализованные координаты в абсолютные и вызвать `SendInput`.

### MOUSE_CLICK (Teacher → Agent, TCP)

```cpp
struct MouseClickPayload {
    float    normalizedX;
    float    normalizedY;
    uint8_t  button;     // 0=left, 1=right, 2=middle
    uint8_t  action;     // 0=press, 1=release, 2=click (press+release)
    uint16_t reserved;
};
```

### KEY_PRESS / KEY_RELEASE (Teacher → Agent, TCP)

```cpp
struct KeyPayload {
    uint16_t virtualKeyCode;  // Win32 VK_* code
    uint16_t scanCode;
    uint32_t flags;           // KEYEVENTF_* flags
};
```

### VIDEO_FRAME (Agent → Teacher, UDP)

```cpp
struct VideoFrameHeader {
    uint32_t clientId;    // Уникальный ID агента (из HANDSHAKE)
    uint64_t timestamp;   // Микросекунды с эпохи
    uint16_t width;
    uint16_t height;
    uint8_t  frameType;   // 0=I-frame (ключевой), 1=P-frame
    uint8_t  quality;     // 1-100
    uint16_t reserved;
};
// За заголовком следуют H.264 NAL units
```

**Frontend обязан:** Декодировать H.264 данные и отобразить в `StudentTile`.

### SET_QUALITY (Teacher → Agent, TCP)

```cpp
struct SetQualityPayload {
    uint8_t  quality;     // 1-100
    uint8_t  maxFps;      // 1-60
    uint16_t reserved;
};
```

## HEARTBEAT протокол

- Агент отправляет `HEARTBEAT` каждые **3 секунды**
- Если TeacherPanel не получает HEARTBEAT **10 секунд** — агент считается offline
- В `HeartbeatPayload`:

```cpp
struct HeartbeatPayload {
    uint32_t clientId;
    float    cpuUsage;      // 0.0 - 100.0
    uint32_t memoryUsageMB;
};
```

## Правила для разработчиков

### Backend разработчик:
1. **Не менять** формат `PacketHeader` без согласования
2. **Добавлять** новые `PacketType` только в зарезервированных диапазонах
3. **Всегда** отправлять `ACK` на команды управления
4. **Использовать** нормализованные координаты (0.0-1.0) для мыши

### Frontend разработчик:
1. **Использовать** `PacketHeader` для парсинга всех входящих пакетов
2. **Проверять** `magic` перед обработкой пакета
3. **Обрабатывать** потерю агентов (heartbeat timeout)
4. **Нормализовать** координаты мыши перед отправкой

### Добавление новых команд:
1. Добавить `PacketType` в `Protocol.h`
2. Создать структуру `*Payload` в `Protocol.h`
3. Обновить этот документ (`API_CONTRACT.md`)
4. Реализовать в client/ И server/
