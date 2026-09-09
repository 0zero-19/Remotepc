// =============================================================================
// ClassroomMonitor — Lock Manager Implementation
//
// Полноэкранный оверлей + низкоуровневые хуки для блокировки ввода.
// =============================================================================

#include "client/LockManager.h"
#include <iostream>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

namespace cm {
namespace client {

LockManager* LockManager::s_instance = nullptr;

static const wchar_t* OVERLAY_CLASS_NAME = L"CMOverlayClass";

LockManager::LockManager() {
    s_instance = this;
}

LockManager::~LockManager() {
    unlock();
    s_instance = nullptr;
}

void LockManager::lock(const std::string& message) {
    if (m_locked.load()) return;

    if (!message.empty()) {
        m_lockMessage = message;
    }

    m_locked.store(true);

    // Устанавливаем хуки в текущем потоке
    installHooks();

    // Запускаем оверлей в отдельном потоке (нужен свой message loop)
    m_overlayThread = std::thread(&LockManager::overlayThreadFunc, this);
}

void LockManager::unlock() {
    if (!m_locked.load()) return;

    m_locked.store(false);

    // Закрываем оверлей
    if (m_overlayWnd) {
        PostMessage(m_overlayWnd, WM_CLOSE, 0, 0);
    }

    // Ждём завершения потока оверлея
    if (m_overlayThread.joinable()) {
        m_overlayThread.join();
    }

    // Снимаем хуки
    removeHooks();
}

void LockManager::installHooks() {
    m_keyboardHook = SetWindowsHookExW(
        WH_KEYBOARD_LL, keyboardHookProc, GetModuleHandle(nullptr), 0);

    m_mouseHook = SetWindowsHookExW(
        WH_MOUSE_LL, mouseHookProc, GetModuleHandle(nullptr), 0);

    if (!m_keyboardHook || !m_mouseHook) {
        std::cerr << "[LockManager] Failed to install hooks" << std::endl;
    }
}

void LockManager::removeHooks() {
    if (m_keyboardHook) {
        UnhookWindowsHookEx(m_keyboardHook);
        m_keyboardHook = nullptr;
    }
    if (m_mouseHook) {
        UnhookWindowsHookEx(m_mouseHook);
        m_mouseHook = nullptr;
    }
}

LRESULT CALLBACK LockManager::keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (s_instance && s_instance->m_locked.load() && nCode >= 0) {
        // Блокируем все клавиши кроме Ctrl+Alt+Del (который нельзя перехватить)
        return 1;  // Не передаём дальше
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK LockManager::mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (s_instance && s_instance->m_locked.load() && nCode >= 0) {
        return 1;  // Блокируем мышь
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK LockManager::overlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Тёмный полупрозрачный фон
            RECT rect;
            GetClientRect(hwnd, &rect);

            HBRUSH brush = CreateSolidBrush(RGB(30, 30, 40));
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);

            // Текст по центру
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));

            HFONT font = CreateFontW(
                48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
            );
            HFONT oldFont = (HFONT)SelectObject(hdc, font);

            // Конвертируем сообщение в wide string
            std::wstring wideMsg;
            if (s_instance) {
                int len = MultiByteToWideChar(CP_UTF8, 0,
                    s_instance->m_lockMessage.c_str(), -1, nullptr, 0);
                wideMsg.resize(len);
                MultiByteToWideChar(CP_UTF8, 0,
                    s_instance->m_lockMessage.c_str(), -1, &wideMsg[0], len);
            } else {
                wideMsg = L"Экран заблокирован";
            }

            DrawTextW(hdc, wideMsg.c_str(), -1, &rect,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, oldFont);
            DeleteObject(font);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void LockManager::overlayThreadFunc() {
    // Регистрируем класс окна
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc   = overlayWndProc;
    wc.hInstance      = GetModuleHandle(nullptr);
    wc.lpszClassName  = OVERLAY_CLASS_NAME;
    wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    // Создаём полноэкранное окно поверх всего
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    m_overlayWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        OVERLAY_CLASS_NAME,
        L"ClassroomMonitor Lock",
        WS_POPUP,
        0, 0, screenW, screenH,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr
    );

    // Полупрозрачность (200/255 ≈ 78%)
    SetLayeredWindowAttributes(m_overlayWnd, 0, 200, LWA_ALPHA);

    ShowWindow(m_overlayWnd, SW_SHOW);
    UpdateWindow(m_overlayWnd);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    m_overlayWnd = nullptr;
    UnregisterClassW(OVERLAY_CLASS_NAME, GetModuleHandle(nullptr));
}

} // namespace client
} // namespace cm
