// =============================================================================
// ClassroomMonitor — Main Window Implementation
// =============================================================================

#include "server/MainWindow.h"

#include "common/Protocol.h"
#include "common/NetworkTypes.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QNetworkInterface>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QMessageBox>
#include <QDebug>

namespace cm {
namespace server {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    setupNetwork();

    // Heartbeat проверка каждые 3 секунды
    m_heartbeatTimer = new QTimer(this);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &MainWindow::onHeartbeatCheck);
    m_heartbeatTimer->start(net::HEARTBEAT_INTERVAL_MS);

    setWindowTitle(QString("ClassroomMonitor — Панель преподавателя [%1]").arg(m_localIps));
    resize(1280, 800);

    statusBar()->showMessage(
        QString("IP для подключения студентов: %1 | Порт TCP: %2, UDP: %3 | Ожидание...")
            .arg(m_localIps).arg(net::COMMAND_PORT).arg(net::VIDEO_PORT));
}

MainWindow::~MainWindow() {
    // Сессии удаляются автоматически (QObject parent)
}

void MainWindow::setupUi() {
    // Центральный виджет
    m_centralWidget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(m_centralWidget);

    // Панель управления сверху
    m_controlPanel = new ControlPanel(this);
    connect(m_controlPanel, &ControlPanel::commandIssued,
            this, &MainWindow::onControlCommand);
    mainLayout->addWidget(m_controlPanel);

    // Скроллируемая область с сеткой студентов
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);

    auto* gridContainer = new QWidget();
    m_gridLayout = new QGridLayout(gridContainer);
    m_gridLayout->setSpacing(8);
    m_gridLayout->setContentsMargins(8, 8, 8, 8);

    scrollArea->setWidget(gridContainer);
    mainLayout->addWidget(scrollArea, 1); // stretch=1, занимает всё пространство

    setCentralWidget(m_centralWidget);

    // Меню
    auto* fileMenu = menuBar()->addMenu("&Файл");
    fileMenu->addAction("&Выход", this, &QWidget::close, QKeySequence::Quit);

    auto* viewMenu = menuBar()->addMenu("&Вид");
    viewMenu->addAction("Обновить сетку", this, &MainWindow::updateGrid);
}

void MainWindow::setupNetwork() {
    // Определяем локальные IPv4 адреса компьютера преподавателя
    QStringList ipList;
    for (const auto& address : QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
            ipList.append(address.toString());
        }
    }
    m_localIps = ipList.isEmpty() ? "127.0.0.1" : ipList.join(", ");

    // TCP сервер для управляющих команд
    m_tcpServer = new QTcpServer(this);
    connect(m_tcpServer, &QTcpServer::newConnection,
            this, &MainWindow::onNewConnection);

    if (!m_tcpServer->listen(QHostAddress::Any, net::COMMAND_PORT)) {
        QMessageBox::critical(this, "Ошибка",
            QString("Не удалось запустить TCP сервер на порту %1:\n%2")
                .arg(net::COMMAND_PORT)
                .arg(m_tcpServer->errorString()));
        return;
    }

    // UDP сокет для видеопотока
    m_udpSocket = new QUdpSocket(this);
    if (!m_udpSocket->bind(QHostAddress::Any, net::VIDEO_PORT)) {
        QMessageBox::critical(this, "Ошибка",
            QString("Не удалось привязать UDP сокет к порту %1")
                .arg(net::VIDEO_PORT));
        return;
    }

    connect(m_udpSocket, &QUdpSocket::readyRead,
            this, &MainWindow::onVideoDataReady);

    qDebug() << "[Server] Local IPs:" << m_localIps
             << "Listening on TCP:" << net::COMMAND_PORT
             << " UDP:" << net::VIDEO_PORT;
}

void MainWindow::onNewConnection() {
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket* socket = m_tcpServer->nextPendingConnection();

        uint32_t clientId = m_nextClientId++;
        auto* session = new ClientSession(clientId, socket, this);

        connect(session, &ClientSession::disconnected,
                this, &MainWindow::onClientDisconnected);

        m_sessions.insert(clientId, session);

        // Создаём тайл для студента
        auto* tile = new StudentTile(clientId, this);
        connect(tile, &StudentTile::clicked,
                this, &MainWindow::onStudentTileClicked);
        m_tiles.insert(clientId, tile);

        // При получении HANDSHAKE обновляем имя в карточке студента
        connect(session, &ClientSession::handshakeReceived, this, [this, session, tile](uint32_t) {
            QString displayName = session->hostname();
            if (!session->username().isEmpty()) {
                displayName += QString(" (%1)").arg(session->username());
            }
            tile->setStudentName(displayName);
            tile->setOnline(true);
            statusBar()->showMessage(
                QString("Подключен: %1 (всего: %2) | IP преподавателя: %3")
                    .arg(displayName)
                    .arg(m_sessions.size())
                    .arg(m_localIps));
        });

        // При получении видеокадров по TCP обновляем тайл и диалог
        connect(session, &ClientSession::videoFrameReceived, this, [this, clientId, tile](uint32_t, const QByteArray& frameData, uint16_t w, uint16_t h) {
            tile->updateFrame(reinterpret_cast<const uint8_t*>(frameData.constData()),
                              frameData.size(), w, h);
            if (auto* dlg = m_viewDialogs.value(clientId, nullptr)) {
                dlg->updateFrame(frameData, w, h);
            }
        });

        updateGrid();

        statusBar()->showMessage(
            QString("Подключение от %1... (всего: %2) | IP преподавателя: %3")
                .arg(socket->peerAddress().toString())
                .arg(m_sessions.size())
                .arg(m_localIps));

        qDebug() << "[Server] New client:" << clientId
                 << "from" << socket->peerAddress().toString();
    }
}

void MainWindow::onVideoDataReady() {
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        m_udpSocket->readDatagram(datagram.data(), datagram.size());

        // Парсим заголовок
        PacketHeader header;
        if (!parseHeader(reinterpret_cast<const uint8_t*>(datagram.constData()),
                         datagram.size(), header)) {
            continue;
        }

        if (static_cast<PacketType>(header.type) != PacketType::VIDEO_FRAME) {
            continue;
        }

        // Парсим заголовок видеокадра
        if (datagram.size() < static_cast<int>(sizeof(PacketHeader) + sizeof(VideoFrameHeader))) {
            continue;
        }

        VideoFrameHeader frameHeader;
        std::memcpy(&frameHeader,
                     datagram.constData() + sizeof(PacketHeader),
                     sizeof(VideoFrameHeader));

        // Находим тайл студента и обновляем изображение
        auto tileIt = m_tiles.find(frameHeader.clientId);
        if (tileIt != m_tiles.end()) {
            const uint8_t* frameData = reinterpret_cast<const uint8_t*>(
                datagram.constData() + sizeof(PacketHeader) + sizeof(VideoFrameHeader));
            size_t frameSize = datagram.size() - sizeof(PacketHeader) - sizeof(VideoFrameHeader);

            tileIt.value()->updateFrame(frameData, frameSize,
                                         frameHeader.width, frameHeader.height);

            if (auto* dlg = m_viewDialogs.value(frameHeader.clientId, nullptr)) {
                dlg->updateFrame(QByteArray(reinterpret_cast<const char*>(frameData), static_cast<int>(frameSize)),
                                 frameHeader.width, frameHeader.height);
            }
        }
    }
}

void MainWindow::onClientDisconnected(uint32_t clientId) {
    qDebug() << "[Server] Client disconnected:" << clientId;

    if (auto* dlg = m_viewDialogs.value(clientId, nullptr)) {
        dlg->close();
        m_viewDialogs.remove(clientId);
    }

    // Удаляем тайл
    auto tileIt = m_tiles.find(clientId);
    if (tileIt != m_tiles.end()) {
        tileIt.value()->deleteLater();
        m_tiles.erase(tileIt);
    }

    // Удаляем сессию
    auto sessionIt = m_sessions.find(clientId);
    if (sessionIt != m_sessions.end()) {
        sessionIt.value()->deleteLater();
        m_sessions.erase(sessionIt);
    }

    updateGrid();

    statusBar()->showMessage(
        QString("Отключен студент (осталось: %1)").arg(m_sessions.size()));
}

void MainWindow::onHeartbeatCheck() {
    auto now = std::chrono::steady_clock::now();

    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        auto* session = it.value();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - session->lastHeartbeat()).count();

        auto* tile = m_tiles.value(it.key(), nullptr);
        if (tile) {
            tile->setOnline(elapsed < net::HEARTBEAT_TIMEOUT_MS);
        }
    }
}

void MainWindow::onStudentTileClicked(uint32_t clientId) {
    qDebug() << "[Server] Tile clicked:" << clientId;
    auto* session = m_sessions.value(clientId, nullptr);
    if (!session) return;

    if (auto* existingDlg = m_viewDialogs.value(clientId, nullptr)) {
        existingDlg->raise();
        existingDlg->activateWindow();
        return;
    }

    auto* dlg = new StudentViewDialog(clientId, session, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    m_viewDialogs.insert(clientId, dlg);

    connect(dlg, &QObject::destroyed, this, [this, clientId]() {
        m_viewDialogs.remove(clientId);
    });

    dlg->show();
}

void MainWindow::onControlCommand(const QString& command) {
    if (command == "lock_all") {
        for (auto* session : m_sessions) {
            session->sendLock();
        }
        statusBar()->showMessage("Все экраны заблокированы");
    } else if (command == "unlock_all") {
        for (auto* session : m_sessions) {
            session->sendUnlock();
        }
        statusBar()->showMessage("Все экраны разблокированы");
    }
}

void MainWindow::updateGrid() {
    // Рассчитываем оптимальную сетку (4 столбца)
    const int columns = 4;
    int row = 0, col = 0;

    for (auto it = m_tiles.begin(); it != m_tiles.end(); ++it) {
        m_gridLayout->addWidget(it.value(), row, col);
        col++;
        if (col >= columns) {
            col = 0;
            row++;
        }
    }
}

} // namespace server
} // namespace cm
