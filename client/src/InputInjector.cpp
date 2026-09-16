// =============================================================================
// ClassroomMonitor — Input Injector Implementation
// =============================================================================

#include "client/InputInjector.h"
#include <iostream>

namespace cm {
namespace client {

InputInjector::InputInjector() {
    m_screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    m_screenHeight = GetSystemMetrics(SM_CYSCREEN);
}

InputInjector::~InputInjector() = default;

void InputInjector::normalizedToAbsolute(float nx, float ny, int& outX, int& outY) {
    // Clamp к [0, 1]
    nx = (nx < 0.0f) ? 0.0f : (nx > 1.0f) ? 1.0f : nx;
    ny = (ny < 0.0f) ? 0.0f : (ny > 1.0f) ? 1.0f : ny;

    // SendInput использует абсолютные координаты в диапазоне [0, 65535]
    outX = static_cast<int>(nx * 65535.0f);
    outY = static_cast<int>(ny * 65535.0f);
}

void InputInjector::moveMouse(float normalizedX, float normalizedY) {
    normalizedX = (normalizedX < 0.0f) ? 0.0f : (normalizedX > 1.0f) ? 1.0f : normalizedX;
    normalizedY = (normalizedY < 0.0f) ? 0.0f : (normalizedY > 1.0f) ? 1.0f : normalizedY;

    m_screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    m_screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int screenW = (m_screenWidth > 0) ? m_screenWidth : 1920;
    int screenH = (m_screenHeight > 0) ? m_screenHeight : 1080;

    int screenX = static_cast<int>(normalizedX * (screenW - 1));
    int screenY = static_cast<int>(normalizedY * (screenH - 1));

    // 1. Прямое перемещение курсора через SetCursorPos (100% надёжность)
    SetCursorPos(screenX, screenY);

    // 2. Генерация события через SendInput для приложений с обработкой raw mouse
    int absX, absY;
    normalizedToAbsolute(normalizedX, normalizedY, absX, absY);

    INPUT input = {};
    input.type           = INPUT_MOUSE;
    input.mi.dx          = absX;
    input.mi.dy          = absY;
    input.mi.dwFlags     = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;

    SendInput(1, &input, sizeof(INPUT));
}

void InputInjector::clickMouse(float normalizedX, float normalizedY,
                                uint8_t button, uint8_t action) {
    normalizedX = (normalizedX < 0.0f) ? 0.0f : (normalizedX > 1.0f) ? 1.0f : normalizedX;
    normalizedY = (normalizedY < 0.0f) ? 0.0f : (normalizedY > 1.0f) ? 1.0f : normalizedY;

    m_screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    m_screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int screenW = (m_screenWidth > 0) ? m_screenWidth : 1920;
    int screenH = (m_screenHeight > 0) ? m_screenHeight : 1080;

    int screenX = static_cast<int>(normalizedX * (screenW - 1));
    int screenY = static_cast<int>(normalizedY * (screenH - 1));

    // Перемещаем курсор в точку клика
    SetCursorPos(screenX, screenY);

    int absX, absY;
    normalizedToAbsolute(normalizedX, normalizedY, absX, absY);

    DWORD downFlag = 0, upFlag = 0;
    switch (button) {
        case 0: downFlag = MOUSEEVENTF_LEFTDOWN;   upFlag = MOUSEEVENTF_LEFTUP;   break;
        case 1: downFlag = MOUSEEVENTF_RIGHTDOWN;  upFlag = MOUSEEVENTF_RIGHTUP;  break;
        case 2: downFlag = MOUSEEVENTF_MIDDLEDOWN; upFlag = MOUSEEVENTF_MIDDLEUP; break;
        default: return;
    }

    auto sendMouseEvent = [&](DWORD flags) {
        INPUT input = {};
        input.type       = INPUT_MOUSE;
        input.mi.dx      = absX;
        input.mi.dy      = absY;
        input.mi.dwFlags = flags | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
        SendInput(1, &input, sizeof(INPUT));
    };

    switch (action) {
        case 0: sendMouseEvent(downFlag); break;                             // Press
        case 1: sendMouseEvent(upFlag); break;                               // Release
        case 2: sendMouseEvent(downFlag); sendMouseEvent(upFlag); break;    // Click
    }
}

void InputInjector::scrollMouse(float normalizedX, float normalizedY,
                                 int16_t deltaX, int16_t deltaY) {
    moveMouse(normalizedX, normalizedY);

    if (deltaY != 0) {
        INPUT input = {};
        input.type           = INPUT_MOUSE;
        input.mi.dwFlags     = MOUSEEVENTF_WHEEL;
        input.mi.mouseData   = static_cast<DWORD>(deltaY * WHEEL_DELTA);
        SendInput(1, &input, sizeof(INPUT));
    }

    if (deltaX != 0) {
        INPUT input = {};
        input.type           = INPUT_MOUSE;
        input.mi.dwFlags     = MOUSEEVENTF_HWHEEL;
        input.mi.mouseData   = static_cast<DWORD>(deltaX * WHEEL_DELTA);
        SendInput(1, &input, sizeof(INPUT));
    }
}

void InputInjector::pressKey(uint16_t virtualKeyCode, uint16_t scanCode, uint32_t flags) {
    INPUT input = {};
    input.type        = INPUT_KEYBOARD;
    input.ki.wVk      = virtualKeyCode;
    input.ki.wScan    = scanCode;
    input.ki.dwFlags  = flags;

    SendInput(1, &input, sizeof(INPUT));
}

void InputInjector::releaseKey(uint16_t virtualKeyCode, uint16_t scanCode, uint32_t flags) {
    INPUT input = {};
    input.type        = INPUT_KEYBOARD;
    input.ki.wVk      = virtualKeyCode;
    input.ki.wScan    = scanCode;
    input.ki.dwFlags  = flags | KEYEVENTF_KEYUP;

    SendInput(1, &input, sizeof(INPUT));
}

void InputInjector::processCommand(const PacketHeader& header, const uint8_t* payload, size_t size) {
    auto type = static_cast<PacketType>(header.type);

    switch (type) {
        case PacketType::MOUSE_MOVE: {
            MouseMovePayload data;
            if (size >= sizeof(MouseMovePayload)) {
                std::memcpy(&data, payload, sizeof(MouseMovePayload));
                moveMouse(data.normalizedX, data.normalizedY);
            }
            break;
        }
        case PacketType::MOUSE_CLICK: {
            MouseClickPayload data;
            if (size >= sizeof(MouseClickPayload)) {
                std::memcpy(&data, payload, sizeof(MouseClickPayload));
                clickMouse(data.normalizedX, data.normalizedY, data.button, data.action);
            }
            break;
        }
        case PacketType::MOUSE_SCROLL: {
            MouseScrollPayload data;
            if (size >= sizeof(MouseScrollPayload)) {
                std::memcpy(&data, payload, sizeof(MouseScrollPayload));
                scrollMouse(data.normalizedX, data.normalizedY, data.deltaX, data.deltaY);
            }
            break;
        }
        case PacketType::KEY_PRESS: {
            KeyPayload data;
            if (size >= sizeof(KeyPayload)) {
                std::memcpy(&data, payload, sizeof(KeyPayload));
                pressKey(data.virtualKeyCode, data.scanCode, data.flags);
            }
            break;
        }
        case PacketType::KEY_RELEASE: {
            KeyPayload data;
            if (size >= sizeof(KeyPayload)) {
                std::memcpy(&data, payload, sizeof(KeyPayload));
                releaseKey(data.virtualKeyCode, data.scanCode, data.flags);
            }
            break;
        }
        default:
            break;
    }
}

} // namespace client
} // namespace cm

