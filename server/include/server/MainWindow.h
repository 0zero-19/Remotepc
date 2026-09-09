#pragma once
// =============================================================================
// ClassroomMonitor — Main Window (Teacher Panel)
//
// Главное окно Qt6 с сеткой экранов студентов.
// FRONTEND — этот файл редактирует frontend-разработчик.
// =============================================================================

#include <QMainWindow>
#include <QGridLayout>
#include <QMap>
#include <QTimer>

#include "server/StudentTile.h"
#include "server/ClientSession.h"
#include "server/ControlPanel.h"
#include "server/StudentViewDialog.h"

class QTcpServer;
class QUdpSocket;

namespace cm {
namespace server {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    /// Новое TCP-соединение от агента
    void onNewConnection();

    /// Получены UDP данные (видеокадры)
    void onVideoDataReady();

    /// Агент отключился
    void onClientDisconnected(uint32_t clientId);

    /// Heartbeat таймер — проверка живости агентов
    void onHeartbeatCheck();

    /// Клик по тайлу студента
    void onStudentTileClicked(uint32_t clientId);

    /// Команда от панели управления
    void onControlCommand(const QString& command);

private:
    void setupUi();
    void setupNetwork();
    void updateGrid();

    // Сеть
    QTcpServer*  m_tcpServer = nullptr;
    QUdpSocket*  m_udpSocket = nullptr;

    // Сессии студентов (clientId → session)
    QMap<uint32_t, ClientSession*> m_sessions;

    // UI
    QWidget*      m_centralWidget = nullptr;
    QGridLayout*  m_gridLayout    = nullptr;
    ControlPanel* m_controlPanel  = nullptr;

    // Тайлы студентов (clientId → tile)
    QMap<uint32_t, StudentTile*>       m_tiles;
    QMap<uint32_t, StudentViewDialog*> m_viewDialogs;

    // Heartbeat
    QTimer* m_heartbeatTimer = nullptr;

    // Счётчик для назначения clientId
    uint32_t m_nextClientId = 1;
    QString  m_localIps;
};

} // namespace server
} // namespace cm
