#pragma once
// =============================================================================
// ClassroomMonitor — Lock Manager
//
// Блокировка клавиатуры и мыши студента с отображением оверлея.
// Использует низкоуровневые хуки Windows (SetWindowsHookEx).
// =============================================================================

#include <Windows.h>
#include <cstdint>
#include <atomic>
#include <thread>
#include <string>

namespace cm {
namespace client {

class LockManager {
public:
    LockManager();
    ~LockManager();

    LockManager(const LockManager&) = delete;
    LockManager& operator=(const LockManager&) = delete;

    /// Заблокировать ввод и показать оверлей
    /// @param message — текст на оверлее (напр. "Экран заблокирован преподавателем")
    void lock(const std::string& message = "");

    /// Разблокировать ввод и скрыть оверлей
    void unlock();

    /// Проверить, заблокирован ли ввод
    bool isLocked() const { return m_locked.load(); }

private:
    void overlayThreadFunc();
    void installHooks();
    void removeHooks();

    static LRESULT CALLBACK keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK overlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    std::atomic<bool> m_locked{false};
    std::thread       m_overlayThread;

    HHOOK m_keyboardHook = nullptr;
    HHOOK m_mouseHook    = nullptr;
    HWND  m_overlayWnd   = nullptr;

    std::string m_lockMessage = "Экран заблокирован преподавателем";

    // Статический указатель для callback-ов
    static LockManager* s_instance;
};

} // namespace client
} // namespace cm
