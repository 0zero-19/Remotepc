// =============================================================================
// ClassroomMonitor — Client Session Implementation
// =============================================================================

#include "server/ClientSession.h"

#include <QDebug>
#include <cstring>

namespace cm {
namespace server {

ClientSession::ClientSession(uint32_t clientId, QTcpSocket* socket, QObject* parent)
    : QObject(parent)
    , m_clientId(clientId)
    , m_socket(socket)
    , m_lastHeartbeat(std::chrono::steady_clock::now())
{
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientSession::onDataReady);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientSession::onDisconnected);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &ClientSession::onError);
}

ClientSession::~ClientSession() {
    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
    }
}

void ClientSession::onDataReady() {
    m_receiveBuffer.append(m_socket->readAll());

    // Пытаемся извлечь полные пакеты из буфера
    while (m_receiveBuffer.size() >= static_cast<int>(sizeof(PacketHeader))) {
        PacketHeader header;
        if (!parseHeader(reinterpret_cast<const uint8_t*>(m_receiveBuffer.constData()),
                         m_receiveBuffer.size(), header)) {
            // Невалидные данные — очищаем буфер
            m_receiveBuffer.clear();
            break;
        }

        int totalPacketSize = static_cast<int>(sizeof(PacketHeader) + header.payloadSize);
        if (m_receiveBuffer.size() < totalPacketSize) {
            break;  // Пакет ещё не полностью получен
        }

        // Извлекаем пакет
        QByteArray packet = m_receiveBuffer.left(totalPacketSize);
        m_receiveBuffer.remove(0, totalPacketSize);

        processPacket(packet);
    }
}

void ClientSession::processPacket(const QByteArray& data) {
    PacketHeader header;
    if (!parseHeader(reinterpret_cast<const uint8_t*>(data.constData()),
                     data.size(), header)) {
        return;
    }

    auto type = static_cast<PacketType>(header.type);

    switch (type) {
        case PacketType::HANDSHAKE: {
            HandshakePayload hs;
            if (parsePayload(reinterpret_cast<const uint8_t*>(data.constData()),
                              data.size(), hs)) {
                m_hostname     = QString::fromUtf8(hs.hostname);
                m_username     = QString::fromUtf8(hs.username);
                m_screenWidth  = hs.screenWidth;
                m_screenHeight = hs.screenHeight;

                qDebug() << "[Session]" << m_clientId << "HANDSHAKE:"
                         << m_hostname << m_username
                         << m_screenWidth << "x" << m_screenHeight;

                emit handshakeReceived(m_clientId);
            }
            break;
        }

        case PacketType::HEARTBEAT: {
            m_lastHeartbeat = std::chrono::steady_clock::now();
            emit heartbeatReceived(m_clientId);
            break;
        }

        case PacketType::ACK: {
            // ACK обработка (для подтверждения команд)
            break;
        }

        default:
            qDebug() << "[Session]" << m_clientId
                     << "Unknown packet type:" << header.type;
            break;
    }
}

void ClientSession::onDisconnected() {
    qDebug() << "[Session]" << m_clientId << "disconnected";
    emit disconnected(m_clientId);
}

void ClientSession::onError(QAbstractSocket::SocketError error) {
    qDebug() << "[Session]" << m_clientId << "socket error:" << error
             << m_socket->errorString();
}

void ClientSession::sendPacket(const std::vector<uint8_t>& packet) {
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(reinterpret_cast<const char*>(packet.data()),
                        static_cast<qint64>(packet.size()));
        m_socket->flush();
    }
}

void ClientSession::sendLock() {
    PacketHeader header;
    header.type = static_cast<uint16_t>(PacketType::LOCK_INPUT);
    header.sequence = m_sequence++;
    header.payloadSize = 0;

    std::vector<uint8_t> packet(sizeof(PacketHeader));
    std::memcpy(packet.data(), &header, sizeof(PacketHeader));
    sendPacket(packet);
}

void ClientSession::sendUnlock() {
    PacketHeader header;
    header.type = static_cast<uint16_t>(PacketType::UNLOCK_INPUT);
    header.sequence = m_sequence++;
    header.payloadSize = 0;

    std::vector<uint8_t> packet(sizeof(PacketHeader));
    std::memcpy(packet.data(), &header, sizeof(PacketHeader));
    sendPacket(packet);
}

void ClientSession::sendMouseMove(float normalizedX, float normalizedY) {
    MouseMovePayload payload;
    payload.normalizedX = normalizedX;
    payload.normalizedY = normalizedY;
    sendPacket(makePacket(PacketType::MOUSE_MOVE, payload, m_sequence++));
}

void ClientSession::sendMouseClick(float normalizedX, float normalizedY,
                                    uint8_t button, uint8_t action) {
    MouseClickPayload payload;
    payload.normalizedX = normalizedX;
    payload.normalizedY = normalizedY;
    payload.button = button;
    payload.action = action;
    sendPacket(makePacket(PacketType::MOUSE_CLICK, payload, m_sequence++));
}

void ClientSession::sendKeyPress(uint16_t vkCode, uint16_t scanCode, uint32_t flags) {
    KeyPayload payload;
    payload.virtualKeyCode = vkCode;
    payload.scanCode = scanCode;
    payload.flags = flags;
    sendPacket(makePacket(PacketType::KEY_PRESS, payload, m_sequence++));
}

void ClientSession::sendKeyRelease(uint16_t vkCode, uint16_t scanCode, uint32_t flags) {
    KeyPayload payload;
    payload.virtualKeyCode = vkCode;
    payload.scanCode = scanCode;
    payload.flags = flags | 0x0002; // KEYEVENTF_KEYUP
    sendPacket(makePacket(PacketType::KEY_RELEASE, payload, m_sequence++));
}

void ClientSession::sendSetQuality(uint8_t quality, uint8_t maxFps) {
    SetQualityPayload payload;
    payload.quality = quality;
    payload.maxFps = maxFps;
    sendPacket(makePacket(PacketType::SET_QUALITY, payload, m_sequence++));
}

} // namespace server
} // namespace cm
