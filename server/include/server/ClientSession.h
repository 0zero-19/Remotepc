#pragma once
// =============================================================================
// ClassroomMonitor — Client Session
//
// Управление TCP-соединением с одним агентом студента.
// Обработка HANDSHAKE, HEARTBEAT, отправка команд.
// =============================================================================

#include "common/Protocol.h"

#include <QObject>
#include <QTcpSocket>
#include <chrono>
#include <cstdint>

namespace cm {
namespace server {

class ClientSession : public QObject {
    Q_OBJECT

public:
    ClientSession(uint32_t clientId, QTcpSocket* socket, QObject* parent = nullptr);
    ~ClientSession() override;

    uint32_t clientId() const { return m_clientId; }
    QString  hostname() const { return m_hostname; }
    QString  username() const { return m_username; }
    uint16_t screenWidth() const { return m_screenWidth; }
    uint16_t screenHeight() const { return m_screenHeight; }
    QHostAddress peerAddress() const { return m_socket ? m_socket->peerAddress() : QHostAddress(); }

    std::chrono::steady_clock::time_point lastHeartbeat() const { return m_lastHeartbeat; }

    /// Отправить команду блокировки
    void sendLock();

    /// Отправить команду разблокировки
    void sendUnlock();

    /// Отправить команду перемещения мыши
    void sendMouseMove(float normalizedX, float normalizedY);

    /// Отправить клик мыши
    void sendMouseClick(float normalizedX, float normalizedY,
                        uint8_t button, uint8_t action);

    /// Отправить скролл мыши
    void sendMouseScroll(float normalizedX, float normalizedY,
                         int16_t deltaX, int16_t deltaY);

    /// Отправить нажатие клавиши
    void sendKeyPress(uint16_t vkCode, uint16_t scanCode, uint32_t flags);

    /// Отправить отпускание клавиши
    void sendKeyRelease(uint16_t vkCode, uint16_t scanCode, uint32_t flags);

    /// Установить качество потока
    void sendSetQuality(uint8_t quality, uint8_t maxFps);

signals:
    void disconnected(uint32_t clientId);
    void handshakeReceived(uint32_t clientId);
    void heartbeatReceived(uint32_t clientId);
    void videoFrameReceived(uint32_t clientId, const QByteArray& frameData, uint16_t width, uint16_t height);

private slots:
    void onDataReady();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);

private:
    void processPacket(const QByteArray& data);
    void sendPacket(const std::vector<uint8_t>& packet);

    uint32_t    m_clientId;
    QTcpSocket* m_socket;
    uint32_t    m_sequence = 0;

    // Данные из HANDSHAKE
    QString  m_hostname;
    QString  m_username;
    uint16_t m_screenWidth  = 0;
    uint16_t m_screenHeight = 0;

    std::chrono::steady_clock::time_point m_lastHeartbeat;

    // Буфер для накопления TCP данных
    QByteArray m_receiveBuffer;
};

} // namespace server
} // namespace cm
