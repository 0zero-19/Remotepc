// =============================================================================
// ClassroomMonitor — Student Agent Entry Point
//
// Главный цикл агента:
//   1. Определяет IP адрес TeacherPanel (аргументы / файл server_ip.txt / ввод)
//   2. Подключается к TeacherPanel по TCP
//   3. Отправляет HANDSHAKE
//   4. Запускает цикл: захват экрана (DXGI) -> кодирование -> отправка по UDP
//   5. Слушает TCP-команды от TeacherPanel
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

#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

using namespace cm;
using namespace cm::client;

// =============================================================================
// Глобальные объекты
// =============================================================================

static std::atomic<bool> g_running{true};
static std::atomic<uint32_t> g_clientId{0};
static std::atomic<uint32_t> g_sequence{0};

// =============================================================================
// Отправка видеопотока по UDP
// =============================================================================

void videoStreamThread(SOCKET udpSocket, const sockaddr_in& serverAddr,
                       net::AgentConfig& config) {
    ScreenCapturer capturer;
    VideoEncoder encoder;

    if (!capturer.initialize()) {
        std::cerr << "[Agent] Ошибка инициализации захвата экрана (DXGI)" << std::endl;
        return;
    }

    video::EncoderConfig encConfig;
    encConfig.width   = capturer.getScreenWidth();
    encConfig.height  = capturer.getScreenHeight();
    encConfig.fps     = config.targetFps;
    encConfig.bitrate = video::DEFAULT_BITRATE;

    if (!encoder.initialize(encConfig)) {
        std::cerr << "[Agent] Ошибка инициализации видео-компрессии (WIC)" << std::endl;
        return;
    }

    std::cout << "[Agent] Стриминг экрана запущен: "
              << encConfig.width << "x" << encConfig.height
              << " @ " << encConfig.fps << " FPS" << std::endl;

    auto frameInterval = std::chrono::milliseconds(1000 / config.targetFps);

    while (g_running.load()) {
        auto frameStart = std::chrono::steady_clock::now();

        // Захватываем кадр
        video::RawFrame rawFrame;
        if (capturer.captureFrame(rawFrame)) {
            // Кодируем
            video::EncodedFrame encoded;
            if (encoder.encodeFrame(rawFrame, encoded)) {
                if (encoded.data.size() < net::MAX_FRAME_SIZE) {
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

                    // Отправляем по UDP
                    sendto(udpSocket,
                           reinterpret_cast<const char*>(packet.data()),
                           static_cast<int>(packet.size()),
                           0,
                           reinterpret_cast<const sockaddr*>(&serverAddr),
                           sizeof(serverAddr));
                }
            }
        }

        // Ограничиваем FPS
        auto frameEnd = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart);
        if (elapsed < frameInterval) {
            std::this_thread::sleep_for(frameInterval - elapsed);
        }
    }

    capturer.shutdown();
    encoder.shutdown();
}

// =============================================================================
// Обработка TCP-команд
// =============================================================================

void commandThread(SOCKET tcpSocket, InputInjector& injector, LockManager& lockMgr) {
    std::vector<uint8_t> buffer(4096);

    while (g_running.load()) {
        int received = recv(tcpSocket,
                            reinterpret_cast<char*>(buffer.data()),
                            static_cast<int>(buffer.size()), 0);

        if (received <= 0) {
            if (received == 0) {
                std::cout << "[Agent] Сервер закрыл TCP-соединение" << std::endl;
                g_running.store(false);
                break;
            }

            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK) {
                // Обычный таймаут ожидания входящих команд от преподавателя - продолжаем слушать
                continue;
            }

            if (err != WSAECONNRESET) {
                std::cerr << "[Agent] Ошибка TCP-соединения: " << err << std::endl;
            }
            g_running.store(false);
            break;
        }

        // Парсим заголовок
        PacketHeader header;
        if (!parseHeader(buffer.data(), received, header)) {
            continue;
        }

        auto type = static_cast<PacketType>(header.type);
        const uint8_t* payload = buffer.data() + sizeof(PacketHeader);
        size_t payloadSize = header.payloadSize;

        switch (type) {
            case PacketType::LOCK_INPUT:
                lockMgr.lock();
                std::cout << "[Agent] >>> Экран ЗАБЛОКИРОВАН преподавателем" << std::endl;
                break;

            case PacketType::UNLOCK_INPUT:
                lockMgr.unlock();
                std::cout << "[Agent] >>> Экран РАЗБЛОКИРОВАН преподавателем" << std::endl;
                break;

            case PacketType::MOUSE_MOVE:
            case PacketType::MOUSE_CLICK:
            case PacketType::MOUSE_SCROLL:
            case PacketType::KEY_PRESS:
            case PacketType::KEY_RELEASE:
                injector.processCommand(header, payload, payloadSize);
                break;

            case PacketType::SHUTDOWN_AGENT:
                std::cout << "[Agent] Запрошено завершение работы от сервера" << std::endl;
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
                std::cerr << "[Agent] Ошибка отправки Heartbeat: " << err << std::endl;
                g_running.store(false);
                break;
            }
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(net::HEARTBEAT_INTERVAL_MS));
    }
}

// =============================================================================
// Вспомогательные функции для определения IP сервера
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

// =============================================================================
// Main
// =============================================================================

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    // Включаем консоль для вывода информации и ввода IP
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::cout << "=====================================================" << std::endl;
    std::cout << "       ClassroomMonitor — Агент Студента            " << std::endl;
    std::cout << "=====================================================" << std::endl;

    // Инициализация Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[Agent] Ошибка инициализации Winsock (WSAStartup)" << std::endl;
        system("pause");
        return 1;
    }

    net::AgentConfig config;

    // 1. Проверяем аргументы командной строки
    std::string cmdArg = trimString(lpCmdLine ? lpCmdLine : "");
    if (!cmdArg.empty()) {
        // Если передан ключ --server или просто IP
        if (cmdArg.find("--server") != std::string::npos) {
            size_t pos = cmdArg.find("--server");
            cmdArg = trimString(cmdArg.substr(pos + 8));
        }
        config.serverHost = cmdArg;
    } else {
        // 2. Проверяем файл server_ip.txt
        std::string savedIp = readServerIpFromFile();
        if (!savedIp.empty()) {
            std::cout << "[i] Найден сохраненный IP сервера: " << savedIp << std::endl;
            config.serverHost = savedIp;
        } else {
            // 3. Запрашиваем ввод у пользователя
            std::cout << "\nВведите IP-адрес компьютера преподавателя (Enter для 127.0.0.1): ";
            std::string inputIp;
            std::getline(std::cin, inputIp);
            inputIp = trimString(inputIp);
            if (inputIp.empty()) {
                config.serverHost = "127.0.0.1";
            } else {
                config.serverHost = inputIp;
                saveServerIpToFile(inputIp);
            }
        }
    }

    // Цикл подключения и работы
    while (true) {
        std::cout << "\n[Agent] Подключение к " << config.serverHost
                  << ":" << config.commandPort << "..." << std::endl;

        SOCKET tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (tcpSocket == INVALID_SOCKET) {
            std::cerr << "[Agent] Ошибка создания TCP сокета: " << WSAGetLastError() << std::endl;
            break;
        }

        // Устанавливаем таймаут подключения
        DWORD timeout = net::TCP_CONNECT_TIMEOUT_MS;
        setsockopt(tcpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(tcpSocket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

        sockaddr_in serverAddr = {};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port   = htons(config.commandPort);
        inet_pton(AF_INET, config.serverHost.c_str(), &serverAddr.sin_addr);

        if (connect(tcpSocket, reinterpret_cast<sockaddr*>(&serverAddr),
                    sizeof(serverAddr)) == SOCKET_ERROR) {
            int err = WSAGetLastError();
            std::cerr << "\n[!] Не удалось подключиться к серверу (" << config.serverHost << ":" << config.commandPort << ")" << std::endl;
            std::cerr << "    Код ошибки Winsock: " << err << std::endl;
            std::cerr << "\n    Проверьте:" << std::endl;
            std::cerr << "    1. Запущена ли программа TeacherPanel на компьютере преподавателя?" << std::endl;
            std::cerr << "    2. Правильный ли IP-адрес указан? (IP преподавателя отображается в окне TeacherPanel)" << std::endl;
            std::cerr << "    3. Находятся ли компьютеры в одной локальной сети / Wi-Fi?" << std::endl;
            std::cerr << "    4. Не блокирует ли Брандмауэр Windows (Firewall) порт 9101?" << std::endl;

            closesocket(tcpSocket);

            std::cout << "\nЧто сделать?" << std::endl;
            std::cout << "  [1] Повторить попытку подключения" << std::endl;
            std::cout << "  [2] Ввести другой IP-адрес" << std::endl;
            std::cout << "  [3] Выйти" << std::endl;
            std::cout << "Выберите (1/2/3): ";

            std::string choice;
            std::getline(std::cin, choice);
            choice = trimString(choice);

            if (choice == "2") {
                std::cout << "Введите новый IP-адрес преподавателя: ";
                std::string newIp;
                std::getline(std::cin, newIp);
                newIp = trimString(newIp);
                if (!newIp.empty()) {
                    config.serverHost = newIp;
                    saveServerIpToFile(newIp);
                }
                continue;
            } else if (choice == "3") {
                break;
            } else {
                continue;
            }
        }

        std::cout << "[Agent] Успешно подключено к серверу!" << std::endl;

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

        std::cout << "[Agent] Отправка данных: ПК '" << handshake.hostname
                  << "', Пользователь '" << handshake.username
                  << "', Экран " << handshake.screenWidth << "x" << handshake.screenHeight << std::endl;

        auto hsPacket = makePacket(PacketType::HANDSHAKE, handshake, g_sequence.fetch_add(1));
        send(tcpSocket, reinterpret_cast<const char*>(hsPacket.data()),
             static_cast<int>(hsPacket.size()), 0);

        g_clientId.store(1);
        g_running.store(true);

        // --- UDP сокет для видео ---
        SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udpSocket == INVALID_SOCKET) {
            std::cerr << "[Agent] Ошибка создания UDP сокета" << std::endl;
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
        std::thread videoThread(videoStreamThread, udpSocket, udpServerAddr, std::ref(config));
        std::thread cmdThread(commandThread, tcpSocket, std::ref(injector), std::ref(lockMgr));
        std::thread hbThread(heartbeatThread, tcpSocket);

        std::cout << "[Agent] Стриминг и мониторинг активны. Окно должно оставаться открытым." << std::endl;

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

        std::cout << "\n[Agent] Связь с сервером потеряна." << std::endl;
        std::cout << "Нажмите Enter для повторной попытки или закройте окно..." << std::endl;
        std::string dummy;
        std::getline(std::cin, dummy);
    }

    WSACleanup();
    std::cout << "[Agent] Завершение работы." << std::endl;
    return 0;
}
