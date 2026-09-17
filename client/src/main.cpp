// =============================================================================
// ClassroomMonitor — Student Agent Entry Point
//
// Главный цикл агента:
//   1. Логирование всех событий и ошибок в файл log_run.txt
//   2. Фоновый тихий режим без всплывающей консоли (консоль по флагу --console)
//   3. Иконка в системном трее + защита от закрытия мастер-паролем (admin)
//   4. Автоматическое переподключение к серверу в фоне
//   5. Захват экрана (DXGI + GDI fallback) -> сжатие (WIC JPEG 30 FPS) -> отправка
// =============================================================================

#include "client/ScreenCapturer.h"
#include "client/VideoEncoder.h"
#include "client/InputInjector.h"
#include "client/LockManager.h"

#include "common/Protocol.h"
#include "common/NetworkTypes.h"
#include "common/VideoTypes.h"

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <shellapi.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>
#include <mutex>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")

using namespace cm;
using namespace cm::client;

// =============================================================================
// Глобальные объекты и флаги
// =============================================================================

static std::atomic<bool> g_running{true};
static std::atomic<uint32_t> g_clientId{0};
static std::atomic<uint32_t> g_sequence{0};
static std::mutex g_logMutex;
static bool g_hasConsole = false;

static HWND g_trayWnd = nullptr;
static NOTIFYICONDATAW g_nid = {};
static const wchar_t* TRAY_WND_CLASS = L"CMStudentAgentTrayClass";
static const wchar_t* PWD_WND_CLASS  = L"CMPasswordDialogClass";

constexpr UINT WM_TRAYICON     = WM_USER + 200;
constexpr UINT IDM_TRAY_TITLE  = 1001;
constexpr UINT IDM_TRAY_LOG    = 1002;
constexpr UINT IDM_TRAY_CONSOLE= 1003;
constexpr UINT IDM_TRAY_EXIT   = 1004;

// =============================================================================
// Логирование в log_run.txt
// =============================================================================

static std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    struct tm tm_buf;
    localtime_s(&tm_buf, &in_time_t);
    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void logMessage(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::string line = "[" + getCurrentTimestamp() + "] " + msg;

    // 1. Запись в log_run.txt
    std::ofstream logFile("log_run.txt", std::ios::app);
    if (logFile.is_open()) {
        logFile << line << std::endl;
    }

    // 2. Вывод в консоль если консоль активна
    if (g_hasConsole) {
        std::cout << line << std::endl;
    }
}

// =============================================================================
// Вспомогательные строковые функции
// =============================================================================

static std::string trimString(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static std::string readServerIpFromFile() {
    std::ifstream file("server_ip.txt");
    if (file.is_open()) {
        std::string ip;
        if (std::getline(file, ip)) {
            return trimString(ip);
        }
    }
    return "";
}

static void saveServerIpToFile(const std::string& ip) {
    std::ofstream file("server_ip.txt");
    if (file.is_open()) {
        file << ip << std::endl;
    }
}

static std::string getAdminPassword() {
    std::ifstream file("admin_password.txt");
    if (file.is_open()) {
        std::string pass;
        if (std::getline(file, pass)) {
            pass = trimString(pass);
            if (!pass.empty()) return pass;
        }
    }
    return "admin"; // Мастер-пароль по умолчанию
}

// =============================================================================
// Управление консолью (включение / выключение)
// =============================================================================

void toggleConsole() {
    if (g_hasConsole) {
        FreeConsole();
        g_hasConsole = false;
        logMessage("[Agent] Консоль скрыта");
    } else {
        AllocConsole();
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        g_hasConsole = true;
        logMessage("[Agent] Консоль включена");
    }
}

// =============================================================================
// Окно ввода пароля для защиты от несанкционированного закрытия
// =============================================================================

struct PasswordDialogState {
    bool authenticated = false;
    HWND hEdit = nullptr;
    HWND hDlg  = nullptr;
};

static PasswordDialogState g_pwdState;

LRESULT CALLBACK PasswordWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Метка
            CreateWindowExW(
                0, L"STATIC", L"Для закрытия программы введите пароль администратора:",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                20, 20, 360, 25, hwnd, nullptr, GetModuleHandle(nullptr), nullptr
            );

            // Поле пароля
            g_pwdState.hEdit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL | WS_TABSTOP,
                20, 50, 345, 26, hwnd, (HMENU)101, GetModuleHandle(nullptr), nullptr
            );

            // Кнопка ОК
            CreateWindowExW(
                0, L"BUTTON", L"Подтвердить",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
                150, 95, 105, 30, hwnd, (HMENU)IDOK, GetModuleHandle(nullptr), nullptr
            );

            // Кнопка Отмена
            CreateWindowExW(
                0, L"BUTTON", L"Отмена",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                265, 95, 100, 30, hwnd, (HMENU)IDCANCEL, GetModuleHandle(nullptr), nullptr
            );

            // Шрифт Segoe UI
            HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
            EnumChildWindows(hwnd, [](HWND child, LPARAM font) -> BOOL {
                SendMessage(child, WM_SETFONT, (WPARAM)font, TRUE);
                return TRUE;
            }, (LPARAM)hFont);

            SetFocus(g_pwdState.hEdit);
            return 0;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == IDOK) {
                wchar_t enteredPass[128] = {};
                GetWindowTextW(g_pwdState.hEdit, enteredPass, 127);

                int len = WideCharToMultiByte(CP_UTF8, 0, enteredPass, -1, nullptr, 0, nullptr, nullptr);
                std::string enteredStr(len ? len - 1 : 0, '\0');
                if (len > 1) {
                    WideCharToMultiByte(CP_UTF8, 0, enteredPass, -1, &enteredStr[0], len, nullptr, nullptr);
                }

                std::string correctPass = getAdminPassword();
                if (enteredStr == correctPass) {
                    g_pwdState.authenticated = true;
                    DestroyWindow(hwnd);
                } else {
                    MessageBoxW(hwnd,
                        L"Введен неверный пароль администратора!\nДоступ к закрытию программы запрещён.",
                        L"Ошибка авторизации", MB_OK | MB_ICONERROR);
                    SetWindowTextW(g_pwdState.hEdit, L"");
                    SetFocus(g_pwdState.hEdit);
                }
                return 0;
            } else if (wmId == IDCANCEL) {
                g_pwdState.authenticated = false;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            g_pwdState.authenticated = false;
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool promptAdminPassword() {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc   = PasswordWndProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.lpszClassName = PWD_WND_CLASS;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);

    g_pwdState.authenticated = false;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int dlgW = 400;
    int dlgH = 180;
    int posX = (screenW - dlgW) / 2;
    int posY = (screenH - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
        PWD_WND_CLASS,
        L"ClassroomMonitor — Авторизация администратора",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        posX, posY, dlgW, dlgH,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr
    );

    g_pwdState.hDlg = hDlg;
    SetForegroundWindow(hDlg);
    SetFocus(g_pwdState.hEdit);

    // Модальный message loop для окна пароля
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            SendMessage(hDlg, WM_COMMAND, IDOK, 0);
            continue;
        } else if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterClassW(PWD_WND_CLASS, GetModuleHandle(nullptr));
    return g_pwdState.authenticated;
}

// =============================================================================
// Фоновое окно трея и обработка событий
// =============================================================================

LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TRAYICON: {
            if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONDBLCLK) {
                POINT pt;
                GetCursorPos(&pt);
                SetForegroundWindow(hwnd);

                HMENU hMenu = CreatePopupMenu();
                AppendMenuW(hMenu, MF_STRING | MF_GRAYED, IDM_TRAY_TITLE, L"💻 ClassroomMonitor — Агент (30 FPS)");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, IDM_TRAY_LOG, L"📄 Открыть журнал (log_run.txt)");
                AppendMenuW(hMenu, MF_STRING, IDM_TRAY_CONSOLE,
                            g_hasConsole ? L"📟 Скрыть консоль" : L"📟 Показать консоль");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT, L"🔒 Выход (Пароль администратора)");

                TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_RIGHTALIGN,
                               pt.x, pt.y, 0, hwnd, nullptr);
                DestroyMenu(hMenu);
            }
            return 0;
        }

        case WM_HOTKEY: {
            if (wParam == 1) { // Секретная комбинация Ctrl+Alt+Shift+F12
                logMessage("[Agent] Вызван диалог авторизации по горячим клавишам");
                if (promptAdminPassword()) {
                    logMessage("[Agent] Пароль администратора принят. Остановка агента...");
                    g_running.store(false);
                    PostQuitMessage(0);
                }
            }
            return 0;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case IDM_TRAY_LOG:
                    ShellExecuteW(nullptr, L"open", L"log_run.txt", nullptr, nullptr, SW_SHOW);
                    break;

                case IDM_TRAY_CONSOLE:
                    toggleConsole();
                    break;

                case IDM_TRAY_EXIT:
                    if (promptAdminPassword()) {
                        logMessage("[Agent] Пароль администратора принят. Остановка агента...");
                        g_running.store(false);
                        PostQuitMessage(0);
                    }
                    break;
            }
            return 0;
        }

        case WM_DESTROY:
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void trayThreadFunc() {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc   = TrayWndProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.lpszClassName = TRAY_WND_CLASS;
    RegisterClassExW(&wc);

    g_trayWnd = CreateWindowExW(
        0, TRAY_WND_CLASS, L"ClassroomMonitorAgentTray",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr
    );

    // Иконка в системном трее
    g_nid.cbSize           = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd             = g_trayWnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"ClassroomMonitor — Агент Студента (Активен)");

    Shell_NotifyIconW(NIM_ADD, &g_nid);

    // Регистрируем глобальный секретный хоткей Ctrl+Alt+Shift+F12
    RegisterHotKey(g_trayWnd, 1, MOD_CONTROL | MOD_ALT | MOD_SHIFT, VK_F12);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterHotKey(g_trayWnd, 1);
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    UnregisterClassW(TRAY_WND_CLASS, GetModuleHandle(nullptr));
}

// =============================================================================
// Отправка видеопотока (30 FPS)
// =============================================================================

void videoStreamThread(SOCKET tcpSocket, SOCKET udpSocket, const sockaddr_in& serverAddr,
                       net::AgentConfig& config) {
    ScreenCapturer capturer;
    VideoEncoder encoder;

    if (!capturer.initialize()) {
        logMessage("[Agent] [ERROR] Ошибка инициализации захвата экрана");
        return;
    }

    video::EncoderConfig encConfig;
    encConfig.width   = capturer.getScreenWidth();
    encConfig.height  = capturer.getScreenHeight();
    encConfig.fps     = config.targetFps;
    encConfig.bitrate = video::DEFAULT_BITRATE;

    if (!encoder.initialize(encConfig)) {
        logMessage("[Agent] [ERROR] Ошибка инициализации видео-компрессии WIC");
        return;
    }

    std::stringstream ss;
    ss << "[Agent] Стриминг экрана запущен: "
       << encConfig.width << "x" << encConfig.height
       << " @ " << encConfig.fps << " FPS";
    logMessage(ss.str());

    auto frameIntervalUs = std::chrono::microseconds(1000000 / config.targetFps);

    while (g_running.load()) {
        auto frameStart = std::chrono::steady_clock::now();

        // Захватываем кадр
        video::RawFrame rawFrame;
        if (capturer.captureFrame(rawFrame)) {
            // Кодируем кадр в JPEG
            video::EncodedFrame encoded;
            if (encoder.encodeFrame(rawFrame, encoded)) {
                // Формируем пакет
                VideoFrameHeader frameHeader;
                frameHeader.clientId  = g_clientId.load();
                frameHeader.timestamp = encoded.timestamp;
                frameHeader.width     = static_cast<uint16_t>(encoded.width);
                frameHeader.height    = static_cast<uint16_t>(encoded.height);
                frameHeader.frameType = static_cast<uint8_t>(encoded.type);
                frameHeader.quality   = config.quality;

                auto packet = makeVideoPacket(
                    frameHeader,
                    encoded.data.data(),
                    encoded.data.size(),
                    g_sequence.fetch_add(1)
                );

                // Отправляем кадр по TCP (надёжная доставка)
                send(tcpSocket,
                     reinterpret_cast<const char*>(packet.data()),
                     static_cast<int>(packet.size()),
                     0);

                // Также дублируем по UDP для минимальной задержки (если кадр маленький)
                if (packet.size() <= net::MAX_UDP_PACKET_SIZE) {
                    sendto(udpSocket,
                           reinterpret_cast<const char*>(packet.data()),
                           static_cast<int>(packet.size()),
                           0,
                           reinterpret_cast<const sockaddr*>(&serverAddr),
                           sizeof(serverAddr));
                }
            }
        }

        // Точное ограничение FPS с использованием yield вместо sleep
        // (Windows sleep может спать на 15мс дольше заданного)
        auto targetEnd = frameStart + frameIntervalUs;
        while (std::chrono::steady_clock::now() < targetEnd) {
            std::this_thread::yield();
        }
    }

    capturer.shutdown();
    encoder.shutdown();
}

// =============================================================================
// Обработка TCP-команд
// =============================================================================

void commandThread(SOCKET tcpSocket, InputInjector& injector, LockManager& lockMgr) {
    std::vector<uint8_t> recvBuf(8192);
    std::vector<uint8_t> accumBuffer;

    while (g_running.load()) {
        int received = recv(tcpSocket,
                            reinterpret_cast<char*>(recvBuf.data()),
                            static_cast<int>(recvBuf.size()), 0);

        if (received <= 0) {
            if (received == 0) {
                logMessage("[Agent] Сервер закрыл TCP-соединение");
                g_running.store(false);
                break;
            }

            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK) {
                continue;
            }

            if (err != WSAECONNRESET) {
                logMessage("[Agent] [ERROR] Ошибка TCP-соединения: " + std::to_string(err));
            }
            g_running.store(false);
            break;
        }

        accumBuffer.insert(accumBuffer.end(), recvBuf.begin(), recvBuf.begin() + received);

        while (accumBuffer.size() >= sizeof(PacketHeader)) {
            PacketHeader header;
            if (!parseHeader(accumBuffer.data(), accumBuffer.size(), header)) {
                bool found = false;
                for (size_t i = 1; i <= accumBuffer.size() - sizeof(uint32_t); ++i) {
                    uint32_t val;
                    std::memcpy(&val, accumBuffer.data() + i, sizeof(uint32_t));
                    if (val == PROTOCOL_MAGIC) {
                        accumBuffer.erase(accumBuffer.begin(), accumBuffer.begin() + i);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    accumBuffer.clear();
                    break;
                }
                continue;
            }

            size_t totalPacketSize = sizeof(PacketHeader) + header.payloadSize;
            if (accumBuffer.size() < totalPacketSize) {
                break;
            }

            auto type = static_cast<PacketType>(header.type);
            const uint8_t* payload = accumBuffer.data() + sizeof(PacketHeader);
            size_t payloadSize = header.payloadSize;

            switch (type) {
                case PacketType::LOCK_INPUT:
                    lockMgr.lock();
                    logMessage("[Agent] >>> Экран ЗАБЛОКИРОВАН преподавателем");
                    break;

                case PacketType::UNLOCK_INPUT:
                    lockMgr.unlock();
                    logMessage("[Agent] >>> Экран РАЗБЛОКИРОВАН преподавателем");
                    break;

                case PacketType::MOUSE_MOVE:
                case PacketType::MOUSE_CLICK:
                case PacketType::MOUSE_SCROLL:
                case PacketType::KEY_PRESS:
                case PacketType::KEY_RELEASE:
                    injector.processCommand(header, payload, payloadSize);
                    break;

                case PacketType::SHUTDOWN_AGENT:
                    logMessage("[Agent] Запрошено завершение работы от сервера преподавателя");
                    g_running.store(false);
                    break;

                default:
                    break;
            }

            // Отправляем ACK
            AckPayload ack;
            ack.ackedSequence = header.sequence;
            ack.ackedType     = header.type;
            ack.statusCode    = 0;
            auto ackPacket = makePacket(PacketType::ACK, ack, g_sequence.fetch_add(1));
            send(tcpSocket, reinterpret_cast<const char*>(ackPacket.data()),
                 static_cast<int>(ackPacket.size()), 0);

            accumBuffer.erase(accumBuffer.begin(), accumBuffer.begin() + totalPacketSize);
        }
    }
}

// =============================================================================
// Heartbeat
// =============================================================================

void heartbeatThread(SOCKET tcpSocket) {
    while (g_running.load()) {
        HeartbeatPayload hb;
        hb.clientId = g_clientId.load();
        hb.cpuUsage      = 0.0f;
        hb.memoryUsageMB = 0;

        auto packet = makePacket(PacketType::HEARTBEAT, hb, g_sequence.fetch_add(1));
        int res = send(tcpSocket, reinterpret_cast<const char*>(packet.data()),
                       static_cast<int>(packet.size()), 0);
        if (res <= 0) {
            int err = WSAGetLastError();
            if (err != 0 && err != WSAEWOULDBLOCK) {
                logMessage("[Agent] Ошибка отправки Heartbeat: " + std::to_string(err));
                g_running.store(false);
                break;
            }
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(net::HEARTBEAT_INTERVAL_MS));
    }
}

// =============================================================================
// WinMain
// =============================================================================

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    std::string cmdLineStr = lpCmdLine ? lpCmdLine : "";

    // Проверяем флаг запроса консоли (--console или файл show_console.txt)
    if (cmdLineStr.find("--console") != std::string::npos ||
        cmdLineStr.find("--debug") != std::string::npos ||
        cmdLineStr.find("-c") != std::string::npos ||
        GetFileAttributesW(L"show_console.txt") != INVALID_FILE_ATTRIBUTES) {
        AllocConsole();
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        g_hasConsole = true;
    }

    logMessage("=====================================================");
    logMessage("       ClassroomMonitor — Агент Студента            ");
    logMessage("       Сборка: " + std::string(__DATE__) + " " + std::string(__TIME__));
    logMessage("       Частота: 30 FPS | Лог: log_run.txt");
    logMessage("=====================================================");

    // Инициализация Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        logMessage("[Agent] [FATAL] Ошибка инициализации Winsock (WSAStartup)");
        return 1;
    }

    net::AgentConfig config;
    config.targetFps = 30;

    // 1. Проверяем аргументы командной строки на наличие IP
    std::string cmdArg = trimString(cmdLineStr);
    if (!cmdArg.empty() && cmdArg.find("--console") == std::string::npos) {
        if (cmdArg.find("--server") != std::string::npos) {
            size_t pos = cmdArg.find("--server");
            cmdArg = trimString(cmdArg.substr(pos + 8));
        }
        config.serverHost = cmdArg;
    } else {
        // 2. Проверяем сохранённый IP в server_ip.txt
        std::string savedIp = readServerIpFromFile();
        if (!savedIp.empty()) {
            config.serverHost = savedIp;
        } else {
            config.serverHost = "127.0.0.1";
        }
    }

    logMessage("[Agent] Целевой IP преподавателя: " + config.serverHost + ":" + std::to_string(config.commandPort));

    // Запускаем фоновый поток системного трея
    std::thread trayThread(trayThreadFunc);

    // Главный цикл подключения и работы
    while (true) {
        logMessage("[Agent] Попытка подключения к " + config.serverHost + ":" + std::to_string(config.commandPort) + "...");

        SOCKET tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (tcpSocket == INVALID_SOCKET) {
            logMessage("[Agent] [ERROR] Ошибка создания TCP сокета: " + std::to_string(WSAGetLastError()));
            break;
        }

        DWORD timeout = net::TCP_CONNECT_TIMEOUT_MS;
        setsockopt(tcpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(tcpSocket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

        // Отключаем Nagle's algorithm — критично для стриминга в реальном времени
        // Без этого TCP может буферизировать маленькие пакеты до 200мс
        BOOL tcpNoDelay = TRUE;
        setsockopt(tcpSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&tcpNoDelay, sizeof(tcpNoDelay));

        sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port   = htons(config.commandPort);
        inet_pton(AF_INET, config.serverHost.c_str(), &serverAddr.sin_addr);

        if (connect(tcpSocket, reinterpret_cast<sockaddr*>(&serverAddr),
                    sizeof(serverAddr)) == SOCKET_ERROR) {
            int err = WSAGetLastError();
            closesocket(tcpSocket);

            logMessage("[Agent] Не удалось подключиться к серверу (" + config.serverHost + ":" +
                       std::to_string(config.commandPort) + "). Код: " + std::to_string(err));
            logMessage("[Agent] Повторная попытка через 5 секунд...");

            // В фоновом режиме просто ждём 5 секунд и повторяем попытку
            for (int i = 0; i < 50 && g_running.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            if (!g_running.load()) {
                break;
            }
            continue;
        }

        logMessage("[Agent] Успешно подключено к серверу преподавателя!");

        // --- Отправляем HANDSHAKE ---
        HandshakePayload handshake = {};
        char hostname[MAX_HOSTNAME_LEN] = {};
        DWORD hostnameLen = MAX_HOSTNAME_LEN;
        GetComputerNameA(hostname, &hostnameLen);
        strncpy_s(handshake.hostname, hostname, MAX_HOSTNAME_LEN - 1);

        char username[MAX_USERNAME_LEN] = {};
        DWORD usernameLen = MAX_USERNAME_LEN;
        GetUserNameA(username, &usernameLen);
        strncpy_s(handshake.username, username, MAX_USERNAME_LEN - 1);

        handshake.screenWidth  = static_cast<uint16_t>(GetSystemMetrics(SM_CXSCREEN));
        handshake.screenHeight = static_cast<uint16_t>(GetSystemMetrics(SM_CYSCREEN));

        std::stringstream ssHs;
        ssHs << "[Agent] Отправка данных: ПК '" << handshake.hostname
             << "', Пользователь '" << handshake.username
             << "', Экран " << handshake.screenWidth << "x" << handshake.screenHeight;
        logMessage(ssHs.str());

        auto hsPacket = makePacket(PacketType::HANDSHAKE, handshake, g_sequence.fetch_add(1));
        send(tcpSocket, reinterpret_cast<const char*>(hsPacket.data()),
             static_cast<int>(hsPacket.size()), 0);

        g_clientId.store(1);
        g_running.store(true);

        // --- UDP сокет для видео ---
        SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udpSocket == INVALID_SOCKET) {
            logMessage("[Agent] [ERROR] Ошибка создания UDP сокета");
            closesocket(tcpSocket);
            break;
        }

        sockaddr_in udpServerAddr = {};
        udpServerAddr.sin_family = AF_INET;
        udpServerAddr.sin_port   = htons(config.videoPort);
        inet_pton(AF_INET, config.serverHost.c_str(), &udpServerAddr.sin_addr);

        // --- Объекты управления ---
        InputInjector injector;
        LockManager lockMgr;

        // --- Запускаем рабочие потоки ---
        std::thread videoThread(videoStreamThread, tcpSocket, udpSocket, udpServerAddr, std::ref(config));
        std::thread cmdThread(commandThread, tcpSocket, std::ref(injector), std::ref(lockMgr));
        std::thread hbThread(heartbeatThread, tcpSocket);

        logMessage("[Agent] Стриминг экрана (30 FPS) и приём команд активны");

        // Ждём пока работает агент
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        // Завершаем рабочие потоки
        if (videoThread.joinable()) videoThread.join();
        if (cmdThread.joinable()) cmdThread.join();
        if (hbThread.joinable()) hbThread.join();

        closesocket(udpSocket);
        closesocket(tcpSocket);

        logMessage("[Agent] Сессия с сервером завершена.");

        // Если выход был запрошен администратором — завершаем цикл
        if (!g_running.load()) {
            break;
        }
    }

    if (g_trayWnd) {
        PostMessage(g_trayWnd, WM_CLOSE, 0, 0);
    }
    if (trayThread.joinable()) {
        trayThread.join();
    }

    WSACleanup();
    logMessage("[Agent] Работа Агента Студента полностью завершена.");
    return 0;
}
