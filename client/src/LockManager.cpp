// =============================================================================
// ClassroomMonitor — Lock Manager Implementation
//
// Полноэкранный оверлей + низкоуровневые хуки для блокировки ввода.
// Защищён от обхода через Alt+F4, Alt+Tab, WinKey и закрытия окон.
// =============================================================================

#include "client/LockManager.h"
#include <iostream>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

namespace cm {
namespace client {

constexpr UINT WM_CM_UNLOCK = WM_USER + 1001;
static const wchar_t* OVERLAY_CLASS_NAME = L"CMOverlayClass";

LockManager* LockManager::s_instance = nullptr;

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

    // Запускаем оверлей и хуки в отдельном GUI-потоке с message loop
    m_overlayThread = std::thread(&LockManager::overlayThreadFunc, this);
}

void LockManager::unlock() {
    if (!m_locked.load()) return;

    m_locked.store(false);

    // Отправляем специальное сообщение разблокировки в GUI-поток оверлея
    if (m_overlayWnd) {
        PostMessage(m_overlayWnd, WM_CM_UNLOCK, 0, 0);
    }

    // Ждём завершения потока оверлея
    if (m_overlayThread.joinable()) {
        m_overlayThread.join();
    }
}

void LockManager::installHooks() {
    // Хуки устанавливаются в потоке с message loop (overlayThreadFunc)
    m_keyboardHook = SetWindowsHookExW(
        WH_KEYBOARD_LL, keyboardHookProc, GetModuleHandle(nullptr), 0);

    m_mouseHook = SetWindowsHookExW(
        WH_MOUSE_LL, mouseHookProc, GetModuleHandle(nullptr), 0);

    if (!m_keyboardHook || !m_mouseHook) {
        std::cerr << "[LockManager] Failed to install low-level hooks: " << GetLastError() << std::endl;
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
        // Блокируем абсолютно ВСЕ клавиши (включая Alt+F4, Alt+Tab, WinKey, Ctrl+Esc, Escape, etc.)
        // Возврат 1 запрещает Windows и приложениям обрабатывать нажатие.
        return 1;
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK LockManager::mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (s_instance && s_instance->m_locked.load() && nCode >= 0) {
        // Блокируем клики и перемещения мыши мимо оверлея
        return 1;
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK LockManager::overlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rect;
            GetClientRect(hwnd, &rect);

            // Тёмно-синий фон блокировки
            HBRUSH brush = CreateSolidBrush(RGB(15, 23, 42)); // Slate-900
            FillRect(hdc, &rect, brush);
            DeleteObject(brush);

            SetBkMode(hdc, TRANSPARENT);

            // Иконка замка
            HFONT iconFont = CreateFontW(
                72, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI Emoji"
            );
            HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, iconFont));
            SetTextColor(hdc, RGB(239, 68, 68)); // Red-500

            RECT iconRect = rect;
            iconRect.bottom = rect.bottom / 2;
            DrawTextW(hdc, L"🔒", -1, &iconRect, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);

            // Заголовок
            HFONT titleFont = CreateFontW(
                36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
            );
            SelectObject(hdc, titleFont);
            DeleteObject(iconFont);
            SetTextColor(hdc, RGB(255, 255, 255));

            RECT titleRect = rect;
            titleRect.top = rect.bottom / 2 + 10;
            titleRect.bottom = titleRect.top + 50;
            DrawTextW(hdc, L"Внимание! Доступ заблокирован", -1, &titleRect, DT_CENTER | DT_TOP | DT_SINGLELINE);

            // Подробный текст сообщения
            HFONT textFont = CreateFontW(
                22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
            );
            SelectObject(hdc, textFont);
            DeleteObject(titleFont);
            SetTextColor(hdc, RGB(148, 163, 184)); // Slate-400

            std::wstring wideMsg;
            if (s_instance && !s_instance->m_lockMessage.empty()) {
                int len = MultiByteToWideChar(CP_UTF8, 0,
                    s_instance->m_lockMessage.c_str(), -1, nullptr, 0);
                wideMsg.resize(len);
                MultiByteToWideChar(CP_UTF8, 0,
                    s_instance->m_lockMessage.c_str(), -1, &wideMsg[0], len);
            } else {
                wideMsg = L"Преподаватель временно ограничил работу за компьютером.";
            }

            RECT subRect = rect;
            subRect.top = titleRect.bottom + 10;
            DrawTextW(hdc, wideMsg.c_str(), -1, &subRect, DT_CENTER | DT_TOP | DT_SINGLELINE);

            SelectObject(hdc, oldFont);
            DeleteObject(textFont);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CLOSE:
            // Игнорируем Alt+F4 и любые попытки закрыть окно пока активна блокировка
            return 0;

        case WM_SYSCOMMAND:
            // Блокируем системные команды закрытия, минимизации, переключения
            switch (wParam & 0xFFF0) {
                case SC_CLOSE:
                case SC_MINIMIZE:
                case SC_MAXIMIZE:
                case SC_NEXTWINDOW:
                case SC_PREVWINDOW:
                case SC_TASKLIST:
                    return 0;
            }
            break;

        case WM_CM_UNLOCK:
            // Разблокировка по команде преподавателя
            KillTimer(hwnd, 1);
            DestroyWindow(hwnd);
            return 0;

        case WM_TIMER:
            // Периодически удерживаем оверлей поверх всех окон
            if (s_instance && s_instance->m_locked.load()) {
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                SetForegroundWindow(hwnd);
            }
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
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.lpszClassName = OVERLAY_CLASS_NAME;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    // Охватываем все мониторы (виртуальный экран)
    int screenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int screenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int screenW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int screenH = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (screenW <= 0 || screenH <= 0) {
        screenX = 0;
        screenY = 0;
        screenW = GetSystemMetrics(SM_CXSCREEN);
        screenH = GetSystemMetrics(SM_CYSCREEN);
    }

    m_overlayWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        OVERLAY_CLASS_NAME,
        L"ClassroomMonitor Lock",
        WS_POPUP,
        screenX, screenY, screenW, screenH,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr
    );

    // Полупрозрачность (95% непрозрачности)
    SetLayeredWindowAttributes(m_overlayWnd, 0, 242, LWA_ALPHA);

    ShowWindow(m_overlayWnd, SW_SHOW);
    SetWindowPos(m_overlayWnd, HWND_TOPMOST, screenX, screenY, screenW, screenH, SWP_SHOWWINDOW);
    UpdateWindow(m_overlayWnd);
    SetForegroundWindow(m_overlayWnd);

    // Таймер удержания фокуса каждые 200мс
    SetTimer(m_overlayWnd, 1, 200, nullptr);

    // Устанавливаем низкоуровневые хуки в текущем потоке с message loop
    installHooks();

    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Снимаем хуки при выходе из message loop
    removeHooks();

    m_overlayWnd = nullptr;
    UnregisterClassW(OVERLAY_CLASS_NAME, GetModuleHandle(nullptr));
}

} // namespace client
} // namespace cm
