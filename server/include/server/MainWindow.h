#pragma once
// =============================================================================
// ClassroomMonitor — Main Window (Teacher Panel)
//
// Минималистичный интерфейс по HTML-макету:
// - Боковая панель списка ПК с чекбоксами (width 245px)
// - Верхняя строка информации (ПК, статус, пинг, разрешение, переключатель 1/4)
// - Центральный экран (одиночный просмотр с оверлеем блокировки / сетка)
// - Нижняя панель управления (Заблокировать, Разблокировать, Сообщение, Скриншот, Во весь экран, Настройки тем)
// =============================================================================

#include <QMainWindow>
#include <QGridLayout>
#include <QStackedWidget>
#include <QMap>
#include <QTimer>
#include <QLabel>
#include <QPushButton>

#include <QMouseEvent>
#include <QWheelEvent>
#include <QPointF>

#include "server/ClientSession.h"
#include "server/StudentTile.h"
#include "server/ClientSidebar.h"
#include "server/StudentViewDialog.h"
#include "server/SettingsDialog.h"

class QTcpServer;
class QUdpSocket;

namespace cm {
namespace server {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onNewConnection();
    void onVideoDataReady();
    void onClientDisconnected(uint32_t clientId);
    void onHeartbeatCheck();

    void onClientSelected(uint32_t clientId);
    void onClientDoubleClicked(uint32_t clientId);

    void onLockClicked();
    void onUnlockClicked();
    void onToggleControl();
    void onScreenshotClicked();
    void onToggleFullscreen();
    void onSettingsClicked();

    void onSetSingleView();
    void onSetGridView();

    void applyTheme(const QString& themeName);

private:
    void setupUi();
    void setupNetwork();
    void updateGrid();
    void setStatusText(const QString& text, const QString& statusClass = "");
    void sendRemoteMouse(QMouseEvent* event, uint8_t action);
    QPointF mapToImageNormalized(QPointF localPos);

    // Сеть
    QTcpServer*  m_tcpServer = nullptr;
    QUdpSocket*  m_udpSocket = nullptr;

    // Сессии
    QMap<uint32_t, ClientSession*> m_sessions;

    // UI
    ClientSidebar*   m_sidebar = nullptr;

    // Info Bar
    QLabel*          m_infoName = nullptr;
    QLabel*          m_infoOnline = nullptr;
    QLabel*          m_infoPing = nullptr;
    QLabel*          m_infoResolution = nullptr;
    QPushButton*     m_singleViewBtn = nullptr;
    QPushButton*     m_gridViewBtn = nullptr;

    // Screen Wrapper
    QStackedWidget*  m_screenStack = nullptr;
    QWidget*         m_singleScreenWidget = nullptr;
    QLabel*          m_singleScreenLabel = nullptr;
    QWidget*         m_lockOverlay = nullptr;
    QLabel*          m_connectionBadge = nullptr;
    QWidget*         m_controlBanner = nullptr;
    QLabel*          m_controlBannerText = nullptr;

    QWidget*         m_gridWidget = nullptr;
    QGridLayout*     m_gridLayout = nullptr;

    // Control Bar
    QPushButton*     m_lockBtn = nullptr;
    QPushButton*     m_unlockBtn = nullptr;
    QPushButton*     m_controlBtn = nullptr;
    QPushButton*     m_screenshotBtn = nullptr;
    QPushButton*     m_fullscreenBtn = nullptr;
    QLabel*          m_statusLabel = nullptr;
    QPushButton*     m_settingsBtn = nullptr;

    // Тайлы и диалоги
    QMap<uint32_t, StudentTile*>       m_tiles;
    QMap<uint32_t, StudentViewDialog*> m_viewDialogs;

    QTimer*  m_heartbeatTimer = nullptr;
    uint32_t m_nextClientId = 1;
    uint32_t m_activeClientId = 0;
    QString  m_localIps;

    // Состояние управления
    bool     m_controlEnabled = false;
    QImage   m_lastSingleImage;
};

} // namespace server
} // namespace cm
