#pragma once
// =============================================================================
// ClassroomMonitor — Input Injector
//
// Приём команд мыши и клавиатуры от TeacherPanel
// и инъекция через Win32 SendInput API.
// =============================================================================

#include "common/Protocol.h"

#include <Windows.h>
#include <cstdint>

namespace cm {
namespace client {

class InputInjector {
public:
    InputInjector();
    ~InputInjector();

    /// Переместить курсор мыши (нормализованные координаты 0.0-1.0)
    void moveMouse(float normalizedX, float normalizedY);

    /// Клик мыши
    /// @param button 0=left, 1=right, 2=middle
    /// @param action 0=press, 1=release, 2=click (press+release)
    void clickMouse(float normalizedX, float normalizedY,
                    uint8_t button, uint8_t action);

    /// Прокрутка колёсика мыши
    void scrollMouse(float normalizedX, float normalizedY,
                     int16_t deltaX, int16_t deltaY);

    /// Нажатие клавиши
    void pressKey(uint16_t virtualKeyCode, uint16_t scanCode, uint32_t flags);

    /// Отпускание клавиши
    void releaseKey(uint16_t virtualKeyCode, uint16_t scanCode, uint32_t flags);

    /// Обработать входящий пакет управления
    void processCommand(const PacketHeader& header, const uint8_t* payload, size_t size);

private:
    /// Преобразование нормализованных координат в абсолютные
    void normalizedToAbsolute(float nx, float ny, int& outX, int& outY);

    int m_screenWidth  = 0;
    int m_screenHeight = 0;
};

} // namespace client
} // namespace cm
