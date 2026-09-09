// =============================================================================
// ClassroomMonitor — Student Agent Entry Point
//
// Главный цикл агента:
//   1. Подключается к TeacherPanel по TCP
//   2. Отправляет HANDSHAKE
//   3. Запускает цикл: захват экрана → кодирование → отправка по UDP
//   4. Слушает TCP-команды от TeacherPanel
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
#include <thread>
#include <atomic>
#include <chrono>
#include <string>

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
        std::cerr << "[Agent] Screen capture init failed" << std::endl;
        return;
    }

    video::EncoderConfig encConfig;
    encConfig.width   = capturer.getScreenWidth();
    encConfig.height  = capturer.getScreenHeight();
    encConfig.fps     = config.targetFps;
    encConfig.bitrate = video::DEFAULT_BITRATE;

    if (!encoder.initialize(encConfig)) {
        std::cerr << "[Agent] Video encoder init failed" << std::endl;
        return;
    }

    std::cout << "[Agent] Streaming started: "
              << encConfig.width << "x" << encConfig.height
              << " @ " << encConfig.fps << " fps" << std::endl;

    auto frameInterval = std::chrono::milliseconds(1000 / config.targetFps);

    while (g_running.load()) {
        auto frameStart = std::chrono::steady_clock::now();

        // Захватываем кадр
        video::RawFrame rawFrame;
        if (capturer.captureFrame(rawFrame)) {
            // Кодируем
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

                // Отправляем по UDP
                sendto(udpSocket,
                       reinterpret_cast<const char*>(packet.data()),
                       static_cast<int>(packet.size()),
                       0,
                       reinterpret_cast<const sockaddr*>(&serverAddr),
                       sizeof(serverAddr));
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
                std::cout << "[Agent] Server disconnected" << std::endl;
            } else {
                int err = WSAGetLastError();
                if (err != WSAETIMEDOUT) {
                    std::cerr << "[Agent] recv error: " << err << std::endl;
                }
            }
            continue;
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
                std::cout << "[Agent] Input LOCKED" << std::endl;
                break;

            case PacketType::UNLOCK_INPUT:
                lockMgr.unlock();
                std::cout << "[Agent] Input UNLOCKED" << std::endl;
                break;

            case PacketType::MOUSE_MOVE:
            case PacketType::MOUSE_CLICK:
            case PacketType::MOUSE_SCROLL:
            case PacketType::KEY_PRESS:
            case PacketType::KEY_RELEASE:
                injector.processCommand(header, payload, payloadSize);
                break;

            case PacketType::SHUTDOWN_AGENT:
                std::cout << "[Agent] Shutdown requested" << std::endl;
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
        // TODO: реальные метрики CPU/RAM
        hb.cpuUsage      = 0.0f;
        hb.memoryUsageMB = 0;

        auto packet = makePacket(PacketType::HEARTBEAT, hb, g_sequence.fetch_add(1));
        send(tcpSocket, reinterpret_cast<const char*>(packet.data()),
             static_cast<int>(packet.size()), 0);

        std::this_thread::sleep_for(
            std::chrono::milliseconds(net::HEARTBEAT_INTERVAL_MS));
    }
}

// =============================================================================
// Main
// =============================================================================

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // Для отладки — показываем консоль
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);

    std::cout << "=== ClassroomMonitor Student Agent ===" << std::endl;

    // Инициализация Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[Agent] WSAStartup failed" << std::endl;
        return 1;
    }

    net::AgentConfig config;
    // TODO: читать из конфига или аргументов командной строки
    // config.serverHost = "192.168.1.100";

    // --- TCP соединение ---
    SOCKET tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcpSocket == INVALID_SOCKET) {
        std::cerr << "[Agent] TCP socket creation failed" << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port   = htons(config.commandPort);
    inet_pton(AF_INET, config.serverHost.c_str(), &serverAddr.sin_addr);

    std::cout << "[Agent] Connecting to " << config.serverHost
              << ":" << config.commandPort << "..." << std::endl;

    if (connect(tcpSocket, reinterpret_cast<sockaddr*>(&serverAddr),
                sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[Agent] TCP connect failed: " << WSAGetLastError() << std::endl;
        closesocket(tcpSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "[Agent] Connected!" << std::endl;

    // --- Отправляем HANDSHAKE ---
    HandshakePayload handshake;
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

    auto hsPacket = makePacket(PacketType::HANDSHAKE, handshake, g_sequence.fetch_add(1));
    send(tcpSocket, reinterpret_cast<const char*>(hsPacket.data()),
         static_cast<int>(hsPacket.size()), 0);

    // Ждём clientId от сервера (ACK с clientId)
    // TODO: реализовать назначение clientId сервером
    g_clientId.store(1);

    // --- UDP сокет ---
    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET) {
        std::cerr << "[Agent] UDP socket creation failed" << std::endl;
        closesocket(tcpSocket);
        WSACleanup();
        return 1;
    }

    sockaddr_in udpServerAddr = {};
    udpServerAddr.sin_family = AF_INET;
    udpServerAddr.sin_port   = htons(config.videoPort);
    inet_pton(AF_INET, config.serverHost.c_str(), &udpServerAddr.sin_addr);

    // --- Объекты управления ---
    InputInjector injector;
    LockManager lockMgr;

    // --- Запускаем потоки ---
    std::thread videoThread(videoStreamThread, udpSocket, udpServerAddr, std::ref(config));
    std::thread cmdThread(commandThread, tcpSocket, std::ref(injector), std::ref(lockMgr));
    std::thread hbThread(heartbeatThread, tcpSocket);

    // Ждём завершения
    videoThread.join();
    cmdThread.join();
    hbThread.join();

    // Cleanup
    closesocket(udpSocket);
    closesocket(tcpSocket);
    WSACleanup();

    std::cout << "[Agent] Shutdown complete" << std::endl;
    return 0;
}
