#pragma once
// =============================================================================
// ClassroomMonitor — Network Types
//
// Конфигурация сети: порты, адреса, таймауты.
// =============================================================================

#include <cstdint>
#include <string>

namespace cm {
namespace net {

// =============================================================================
// Порты
// =============================================================================

constexpr uint16_t VIDEO_PORT   = 9100;  // UDP — видеопоток
constexpr uint16_t COMMAND_PORT = 9101;  // TCP — управляющие команды

// =============================================================================
// Таймауты (миллисекунды)
// =============================================================================

constexpr uint32_t HEARTBEAT_INTERVAL_MS = 3000;   // Как часто агент шлёт heartbeat
constexpr uint32_t HEARTBEAT_TIMEOUT_MS  = 10000;  // Когда считать агента offline
constexpr uint32_t RECONNECT_DELAY_MS    = 5000;   // Задержка перед реконнектом
constexpr uint32_t TCP_CONNECT_TIMEOUT_MS = 5000;  // Таймаут TCP-соединения

// =============================================================================
// Лимиты
// =============================================================================

constexpr size_t   MAX_UDP_PACKET_SIZE  = 65507;   // Максимум для UDP
constexpr size_t   MAX_FRAME_SIZE       = 60000;   // Макс размер одного UDP-пакета с кадром
constexpr uint32_t MAX_CLIENTS          = 50;       // Макс студентов
constexpr uint32_t MAX_FPS              = 60;       // Max FPS
constexpr uint32_t DEFAULT_FPS          = 45;       // FPS default (45 frames/sec)


// =============================================================================
// Конфигурация по умолчанию
// =============================================================================

struct NetworkConfig {
    std::string serverAddress = "0.0.0.0";  // Адрес для серверных сокетов
    uint16_t    videoPort     = VIDEO_PORT;
    uint16_t    commandPort   = COMMAND_PORT;
    uint32_t    maxClients    = MAX_CLIENTS;
};

struct AgentConfig {
    std::string serverHost = "127.0.0.1";   // IP адрес TeacherPanel
    uint16_t    videoPort   = VIDEO_PORT;
    uint16_t    commandPort = COMMAND_PORT;
    uint32_t    targetFps   = DEFAULT_FPS;
    uint8_t     quality     = 50;            // 1—100
};

} // namespace net
} // namespace cm
