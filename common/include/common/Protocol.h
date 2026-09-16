#pragma once
// =============================================================================
// ClassroomMonitor — Protocol
//
// Общий протокол между StudentAgent (client) и TeacherPanel (server).
// Этот файл — КОНТРАКТ. Изменения должны быть согласованы обоими разработчиками.
// См. API_CONTRACT.md для полной документации.
// =============================================================================

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

namespace cm {

// =============================================================================
// Константы
// =============================================================================

constexpr uint32_t PROTOCOL_MAGIC   = 0x434D4F4E;  // "CMON"
constexpr uint16_t PROTOCOL_VERSION = 1;
constexpr size_t   MAX_HOSTNAME_LEN = 64;
constexpr size_t   MAX_USERNAME_LEN = 64;

// =============================================================================
// Типы пакетов
// =============================================================================

enum class PacketType : uint16_t {
    // --- TCP: Teacher → Agent ---
    LOCK_INPUT          = 0x0001,
    UNLOCK_INPUT        = 0x0002,
    MOUSE_MOVE          = 0x0010,
    MOUSE_CLICK         = 0x0011,
    MOUSE_SCROLL        = 0x0012,
    KEY_PRESS           = 0x0020,
    KEY_RELEASE         = 0x0021,
    SET_QUALITY         = 0x0030,
    REQUEST_SCREENSHOT  = 0x0040,
    SHUTDOWN_AGENT      = 0x00FF,

    // --- TCP: Agent → Teacher ---
    HANDSHAKE           = 0x0100,
    HEARTBEAT           = 0x0101,
    ACK                 = 0x0102,

    // --- UDP: Agent → Teacher ---
    VIDEO_FRAME         = 0x0200,
};

// =============================================================================
// Заголовок пакета — общий для всех пакетов (TCP и UDP)
// =============================================================================

#pragma pack(push, 1)

struct PacketHeader {
    uint32_t magic       = PROTOCOL_MAGIC;
    uint16_t type        = 0;       // PacketType
    uint32_t sequence    = 0;       // Порядковый номер
    uint32_t payloadSize = 0;       // Размер данных после заголовка
    uint16_t flags       = 0;       // Зарезервировано
};
static_assert(sizeof(PacketHeader) == 16, "PacketHeader must be 16 bytes");

// =============================================================================
// Payload структуры
// =============================================================================

struct HandshakePayload {
    char     hostname[MAX_HOSTNAME_LEN] = {};
    char     username[MAX_USERNAME_LEN] = {};
    uint16_t screenWidth   = 0;
    uint16_t screenHeight  = 0;
    uint16_t agentVersion  = PROTOCOL_VERSION;
    uint16_t reserved      = 0;
};

struct HeartbeatPayload {
    uint32_t clientId      = 0;
    float    cpuUsage      = 0.0f;  // 0.0 — 100.0
    uint32_t memoryUsageMB = 0;
};

struct AckPayload {
    uint32_t ackedSequence = 0;     // Номер подтверждённого пакета
    uint16_t ackedType     = 0;     // Тип подтверждённого пакета
    uint16_t statusCode    = 0;     // 0 = OK, >0 = ошибка
};

struct MouseMovePayload {
    float normalizedX = 0.0f;       // 0.0 — 1.0
    float normalizedY = 0.0f;       // 0.0 — 1.0
};

struct MouseClickPayload {
    float    normalizedX = 0.0f;
    float    normalizedY = 0.0f;
    uint8_t  button      = 0;       // 0=left, 1=right, 2=middle
    uint8_t  action      = 0;       // 0=press, 1=release, 2=click
    uint16_t reserved    = 0;
};

struct MouseScrollPayload {
    float    normalizedX = 0.0f;
    float    normalizedY = 0.0f;
    int16_t  deltaX      = 0;
    int16_t  deltaY      = 0;
};

struct KeyPayload {
    uint16_t virtualKeyCode = 0;    // Win32 VK_* code
    uint16_t scanCode       = 0;
    uint32_t flags          = 0;    // KEYEVENTF_* flags
};

struct SetQualityPayload {
    uint8_t  quality  = 50;         // 1—100
    uint8_t  maxFps   = 30;         // 1—60
    uint16_t reserved = 0;
};


struct VideoFrameHeader {
    uint32_t clientId   = 0;
    uint64_t timestamp  = 0;        // Микросекунды с эпохи
    uint16_t width      = 0;
    uint16_t height     = 0;
    uint8_t  frameType  = 0;        // 0=I-frame, 1=P-frame
    uint8_t  quality    = 0;        // 1—100
    uint16_t reserved   = 0;
};

#pragma pack(pop)

// =============================================================================
// Утилиты сериализации
// =============================================================================

/// Создать полный пакет (header + payload) в виде байтового массива
template<typename PayloadT>
std::vector<uint8_t> makePacket(PacketType type, const PayloadT& payload, uint32_t seq = 0) {
    PacketHeader header;
    header.type        = static_cast<uint16_t>(type);
    header.sequence    = seq;
    header.payloadSize = static_cast<uint32_t>(sizeof(PayloadT));

    std::vector<uint8_t> buffer(sizeof(PacketHeader) + sizeof(PayloadT));
    std::memcpy(buffer.data(), &header, sizeof(PacketHeader));
    std::memcpy(buffer.data() + sizeof(PacketHeader), &payload, sizeof(PayloadT));
    return buffer;
}

/// Создать пакет с произвольными данными (для видеокадров)
std::vector<uint8_t> makeVideoPacket(const VideoFrameHeader& frameHeader,
                                      const uint8_t* frameData,
                                      size_t frameSize,
                                      uint32_t seq = 0);

/// Распарсить заголовок пакета. Возвращает true если magic корректен.
bool parseHeader(const uint8_t* data, size_t size, PacketHeader& outHeader);

/// Извлечь payload из буфера (после заголовка)
template<typename PayloadT>
bool parsePayload(const uint8_t* data, size_t size, PayloadT& outPayload) {
    if (size < sizeof(PacketHeader) + sizeof(PayloadT)) return false;
    std::memcpy(&outPayload, data + sizeof(PacketHeader), sizeof(PayloadT));
    return true;
}

} // namespace cm
