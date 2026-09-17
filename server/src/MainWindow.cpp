// =============================================================================
// ClassroomMonitor — Main Window Implementation (Minimalist Theme)
// =============================================================================

#include "server/MainWindow.h"
#include "server/ThemeManager.h"
#include "common/Protocol.h"
#include "common/NetworkTypes.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QNetworkInterface>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QStyle>
#include <QDebug>

namespace cm {
namespace server {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    setupNetwork();

    // Heartbeat таймер
    m_heartbeatTimer = new QTimer(this);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &MainWindow::onHeartbeatCheck);
    m_heartbeatTimer->start(net::HEARTBEAT_INTERVAL_MS);

    setWindowTitle(QString("Classroom Monitor — [%1]").arg(m_localIps));
    resize(1200, 750);
    setMinimumSize(950, 600);

    applyTheme(ThemeManager::instance().currentThemeName());
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
    auto* centralWidget = new QWidget(this);
    auto* mainAppLayout = new QHBoxLayout(centralWidget);
    mainAppLayout->setContentsMargins(14, 14, 14, 14);
    mainAppLayout->setSpacing(14);

    // =========================================================================
    // 1. Боковая панель клиентов (Sidebar, width 245px)
    // =========================================================================
    m_sidebar = new ClientSidebar(this);
    connect(m_sidebar, &ClientSidebar::clientSelected, this, &MainWindow::onClientSelected);
    connect(m_sidebar, &ClientSidebar::clientDoubleClicked, this, &MainWindow::onClientDoubleClicked);
    mainAppLayout->addWidget(m_sidebar);

    // =========================================================================
    // 2. Основная рабочая область (.main)
    // =========================================================================
    auto* mainCol = new QVBoxLayout();
    mainCol->setSpacing(10);

    // --- 2.1 Info Bar (.info-bar, height 42px) ---
    auto* infoBar = new QFrame(this);
    infoBar->setObjectName("infoBar");
    auto* infoLayout = new QHBoxLayout(infoBar);
    infoLayout->setContentsMargins(12, 0, 12, 0);
    infoLayout->setSpacing(18);

    m_infoName = new QLabel("PC-01", infoBar);
    m_infoName->setStyleSheet("font-weight: 500; font-size: 14px;");
    infoLayout->addWidget(m_infoName);

    m_infoOnline = new QLabel("Online", infoBar);
    m_infoOnline->setStyleSheet("color: " + ThemeManager::instance().currentTheme().greenHover + "; font-size: 12px;");
    infoLayout->addWidget(m_infoOnline);

    m_infoPing = new QLabel("12 ms", infoBar);
    m_infoPing->setStyleSheet("color: " + ThemeManager::instance().currentTheme().textMuted + "; font-size: 12px;");
    infoLayout->addWidget(m_infoPing);

    m_infoResolution = new QLabel("1920 × 1080", infoBar);
    m_infoResolution->setStyleSheet("color: " + ThemeManager::instance().currentTheme().textMuted + "; font-size: 12px;");
    infoLayout->addWidget(m_infoResolution);

    infoLayout->addStretch();

    // Кнопки переключения вида: 1 (Single) / 4 (Grid)
    auto* viewButtonsLayout = new QHBoxLayout();
    viewButtonsLayout->setSpacing(4);

    m_singleViewBtn = new QPushButton("1", infoBar);
    m_singleViewBtn->setProperty("class", "viewButton");
    m_singleViewBtn->setProperty("active", true);
    m_singleViewBtn->setToolTip("Один экран");
    m_singleViewBtn->setCursor(Qt::PointingHandCursor);
    connect(m_singleViewBtn, &QPushButton::clicked, this, &MainWindow::onSetSingleView);
    viewButtonsLayout->addWidget(m_singleViewBtn);

    m_gridViewBtn = new QPushButton("4", infoBar);
    m_gridViewBtn->setProperty("class", "viewButton");
    m_gridViewBtn->setProperty("active", false);
    m_gridViewBtn->setToolTip("Сетка");
    m_gridViewBtn->setCursor(Qt::PointingHandCursor);
    connect(m_gridViewBtn, &QPushButton::clicked, this, &MainWindow::onSetGridView);
    viewButtonsLayout->addWidget(m_gridViewBtn);

    infoLayout->addLayout(viewButtonsLayout);
    mainCol->addWidget(infoBar);

    // --- 2.2 Screen Wrapper (.screen-wrapper) ---
    auto* screenWrapper = new QFrame(this);
    screenWrapper->setObjectName("screenWrapper");
    auto* wrapperLayout = new QVBoxLayout(screenWrapper);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);

    m_screenStack = new QStackedWidget(screenWrapper);

    // Вкладка 0: Одиночный экран
    m_singleScreenWidget = new QWidget(m_screenStack);
    auto* singleLayout = new QVBoxLayout(m_singleScreenWidget);
    singleLayout->setContentsMargins(0, 0, 0, 0);

    m_singleScreenLabel = new QLabel(m_singleScreenWidget);
    m_singleScreenLabel->setAlignment(Qt::AlignCenter);
    m_singleScreenLabel->setStyleSheet("color: #a89984; font-size: 14px;");
    m_singleScreenLabel->setText("🖥️ Экран ПК\nЗдесь будет трансляция экрана ученика");
    m_singleScreenLabel->setMouseTracking(true);
    m_singleScreenLabel->setFocusPolicy(Qt::StrongFocus);
    m_singleScreenLabel->installEventFilter(this);
    singleLayout->addWidget(m_singleScreenLabel);

    // Плавающий бейдж активного управления
    m_controlBanner = new QWidget(m_singleScreenWidget);
    m_controlBanner->setStyleSheet(
        "background-color: rgba(20, 83, 45, 0.94);"
        "border: 1px solid #22c55e;"
        "border-radius: 6px;"
    );
    auto* bannerLayout = new QHBoxLayout(m_controlBanner);
    bannerLayout->setContentsMargins(12, 6, 12, 6);
    bannerLayout->setSpacing(10);

    m_controlBannerText = new QLabel("🟢 Управление активно — мышь перехвачена (Esc для выхода)", m_controlBanner);
    m_controlBannerText->setStyleSheet("color: #ffffff; font-weight: bold; font-size: 12px; background: transparent;");
    bannerLayout->addWidget(m_controlBannerText);

    auto* stopControlBtn = new QPushButton("✖ Отключить", m_controlBanner);
    stopControlBtn->setStyleSheet(
        "QPushButton { background-color: #dc2626; color: #ffffff; border: none; border-radius: 4px; padding: 3px 8px; font-size: 11px; font-weight: bold; }"
        "QPushButton:hover { background-color: #ef4444; }"
    );
    stopControlBtn->setCursor(Qt::PointingHandCursor);
    connect(stopControlBtn, &QPushButton::clicked, this, &MainWindow::onToggleControl);
    bannerLayout->addWidget(stopControlBtn);

    m_controlBanner->hide();

    // Оверлей блокировки (по центру)
    m_lockOverlay = new QWidget(m_singleScreenWidget);
    m_lockOverlay->setStyleSheet("background-color: rgba(31, 30, 29, 0.88);");
    auto* lockLayout = new QVBoxLayout(m_lockOverlay);
    lockLayout->setAlignment(Qt::AlignCenter);
    auto* lockTitle = new QLabel("Экран заблокирован", m_lockOverlay);
    lockTitle->setStyleSheet("font-size: 19px; font-weight: 500; color: #ebdbb2;");
    lockTitle->setAlignment(Qt::AlignCenter);
    auto* lockSub = new QLabel("Доступ временно ограничен преподавателем", m_lockOverlay);
    lockSub->setStyleSheet("font-size: 12px; color: #a89984;");
    lockSub->setAlignment(Qt::AlignCenter);
    lockLayout->addWidget(lockTitle);
    lockLayout->addWidget(lockSub);
    m_lockOverlay->hide();

    m_screenStack->addWidget(m_singleScreenWidget);

    // Вкладка 1: Сетка экранов
    m_gridWidget = new QWidget(m_screenStack);
    auto* gridScroll = new QScrollArea(m_gridWidget);
    gridScroll->setWidgetResizable(true);
    gridScroll->setStyleSheet("border: none; background: #1f1e1d;");

    auto* gridContainer = new QWidget();
    gridContainer->setStyleSheet("background: #1f1e1d;");
    m_gridLayout = new QGridLayout(gridContainer);
    m_gridLayout->setSpacing(8);
    m_gridLayout->setContentsMargins(8, 8, 8, 8);

    gridScroll->setWidget(gridContainer);
    auto* gridContainerLayout = new QVBoxLayout(m_gridWidget);
    gridContainerLayout->setContentsMargins(0, 0, 0, 0);
    gridContainerLayout->addWidget(gridScroll);

    m_screenStack->addWidget(m_gridWidget);

    wrapperLayout->addWidget(m_screenStack);
    mainCol->addWidget(screenWrapper, 1);

    // --- 2.3 Control Bar (.control-bar, height 52px) ---
    auto* controlBar = new QFrame(this);
    controlBar->setObjectName("controlBar");
    auto* controlLayout = new QHBoxLayout(controlBar);
    controlLayout->setContentsMargins(8, 0, 8, 0);
    controlLayout->setSpacing(8);

    m_lockBtn = new QPushButton("Заблокировать", controlBar);
    m_lockBtn->setObjectName("lockButton");
    m_lockBtn->setCursor(Qt::PointingHandCursor);
    connect(m_lockBtn, &QPushButton::clicked, this, &MainWindow::onLockClicked);
    controlLayout->addWidget(m_lockBtn);

    m_unlockBtn = new QPushButton("Разблокировать", controlBar);
    m_unlockBtn->setObjectName("unlockButton");
    m_unlockBtn->setCursor(Qt::PointingHandCursor);
    connect(m_unlockBtn, &QPushButton::clicked, this, &MainWindow::onUnlockClicked);
    controlLayout->addWidget(m_unlockBtn);

    // Разделитель
    auto* sep = new QFrame(controlBar);
    sep->setFrameShape(QFrame::VLine);
    sep->setStyleSheet("color: " + ThemeManager::instance().currentTheme().border + ";");
    controlLayout->addWidget(sep);

    m_controlBtn = new QPushButton("🖱️ Управление", controlBar);
    m_controlBtn->setObjectName("controlButton");
    m_controlBtn->setProperty("class", "secondaryButton");
    m_controlBtn->setProperty("active", false);
    m_controlBtn->setCursor(Qt::PointingHandCursor);
    m_controlBtn->setToolTip("Перехватить управление мышью (Горячая клавиша: C / Esc)");
    connect(m_controlBtn, &QPushButton::clicked, this, &MainWindow::onToggleControl);
    controlLayout->addWidget(m_controlBtn);

    m_screenshotBtn = new QPushButton("Скриншот", controlBar);
    m_screenshotBtn->setProperty("class", "secondaryButton");
    m_screenshotBtn->setCursor(Qt::PointingHandCursor);
    connect(m_screenshotBtn, &QPushButton::clicked, this, &MainWindow::onScreenshotClicked);
    controlLayout->addWidget(m_screenshotBtn);

    m_fullscreenBtn = new QPushButton("Во весь экран", controlBar);
    m_fullscreenBtn->setProperty("class", "secondaryButton");
    m_fullscreenBtn->setCursor(Qt::PointingHandCursor);
    connect(m_fullscreenBtn, &QPushButton::clicked, this, &MainWindow::onToggleFullscreen);
    controlLayout->addWidget(m_fullscreenBtn);

    controlLayout->addStretch();

    m_statusLabel = new QLabel("Готов", controlBar);
    m_statusLabel->setStyleSheet("font-size: 12px; color: " + ThemeManager::instance().currentTheme().textMuted + "; padding: 0 8px;");
    controlLayout->addWidget(m_statusLabel);

    m_settingsBtn = new QPushButton("⚙", controlBar);
    m_settingsBtn->setObjectName("settingsButton");
    m_settingsBtn->setToolTip("Настройки тем");
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    controlLayout->addWidget(m_settingsBtn);

    mainCol->addWidget(controlBar);

    mainAppLayout->addLayout(mainCol, 1);
    setCentralWidget(centralWidget);

    menuBar()->hide();
    statusBar()->hide();
}

void MainWindow::applyTheme(const QString& themeName) {
    ThemeManager::instance().setTheme(themeName);
    setStyleSheet(ThemeManager::instance().generateStyleSheet(themeName));

    const auto& t = ThemeManager::instance().currentTheme();
    m_infoOnline->setStyleSheet("color: " + t.greenHover + "; font-size: 12px;");
    m_infoPing->setStyleSheet("color: " + t.textMuted + "; font-size: 12px;");
    m_infoResolution->setStyleSheet("color: " + t.textMuted + "; font-size: 12px;");
    m_statusLabel->setStyleSheet("font-size: 12px; color: " + t.textMuted + "; padding: 0 8px;");
}

void MainWindow::setupNetwork() {
    QStringList ipList;
    for (const auto& address : QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
            ipList.append(address.toString());
        }
    }
    m_localIps = ipList.isEmpty() ? "127.0.0.1" : ipList.join(", ");

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

    setStatusText("Ожидание подключения...");
}

void MainWindow::onNewConnection() {
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket* socket = m_tcpServer->nextPendingConnection();

        uint32_t clientId = m_nextClientId++;
        auto* session = new ClientSession(clientId, socket, this);

        connect(session, &ClientSession::disconnected,
                this, &MainWindow::onClientDisconnected);

        m_sessions.insert(clientId, session);

        QString defaultName = QString("PC-%1").arg(clientId, 2, 10, QChar('0'));
        m_sidebar->addClient(clientId, defaultName, socket->peerAddress().toString());

        auto* tile = new StudentTile(clientId, this);
        tile->setStudentName(defaultName);
        connect(tile, &StudentTile::clicked, this, &MainWindow::onClientSelected);
        connect(tile, &StudentTile::doubleClicked, this, &MainWindow::onClientDoubleClicked);
        m_tiles.insert(clientId, tile);

        connect(session, &ClientSession::handshakeReceived, this, [this, session, tile, clientId](uint32_t) {
            QString displayName = session->hostname();
            if (!session->username().isEmpty()) {
                displayName += QString(" (%1)").arg(session->username());
            }
            tile->setStudentName(displayName);
            m_sidebar->updateClient(clientId, displayName, session->peerAddress().toString(), true);

            if (m_activeClientId == clientId) {
                m_infoName->setText(displayName);
                m_infoResolution->setText(QString("%1 × %2").arg(session->screenWidth()).arg(session->screenHeight()));
            }
            setStatusText(QString("Подключен: %1").arg(displayName));
        });

        connect(session, &ClientSession::videoFrameReceived, this, [this, tile, clientId](uint32_t, const QByteArray& frameData, uint16_t w, uint16_t h) {
            if (frameData.isEmpty()) return;

            // Декодируем JPEG ровно один раз для всех компонентов
            QImage img;
            if (!img.loadFromData(frameData, "JPEG") && !img.loadFromData(frameData)) {
                return;
            }

            tile->updateImage(img, w, h);

            if (m_activeClientId == clientId) {
                m_lastSingleImage = img;
                QPixmap pixmap = QPixmap::fromImage(img).scaled(
                    m_singleScreenLabel->size(),
                    Qt::KeepAspectRatio,
                    Qt::FastTransformation
                );
                m_singleScreenLabel->setPixmap(pixmap);
                m_singleScreenLabel->setText("");
            }

            if (auto* dlg = m_viewDialogs.value(clientId, nullptr)) {
                dlg->updateImage(img, w, h);
            }
        });

        updateGrid();

        if (m_activeClientId == 0) {
            onClientSelected(clientId);
        }
    }
}

void MainWindow::onVideoDataReady() {
    // Резервный слот (видеопоток передается надёжно по TCP с TCP_NODELAY)
}

void MainWindow::onClientDisconnected(uint32_t clientId) {
    auto tileIt = m_tiles.find(clientId);
    if (tileIt != m_tiles.end()) {
        tileIt.value()->deleteLater();
        m_tiles.erase(tileIt);
    }

    auto sessionIt = m_sessions.find(clientId);
    if (sessionIt != m_sessions.end()) {
        sessionIt.value()->deleteLater();
        m_sessions.erase(sessionIt);
    }

    m_sidebar->removeClient(clientId);

    if (m_activeClientId == clientId) {
        if (m_controlEnabled) {
            onToggleControl();
        }
        m_activeClientId = m_sidebar->activeClientId();
        if (m_activeClientId != 0) {
            onClientSelected(m_activeClientId);
        } else {
            m_infoName->setText("Нет ПК");
            m_infoOnline->setText("Offline");
            m_lastSingleImage = QImage();
            m_singleScreenLabel->setPixmap(QPixmap());
            m_singleScreenLabel->setText("🖥️ Экран ПК\nЗдесь будет трансляция экрана ученика");
        }
    }

    updateGrid();
    setStatusText("Студент отключен");
}

void MainWindow::onHeartbeatCheck() {
    auto now = std::chrono::steady_clock::now();

    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        auto* session = it.value();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - session->lastHeartbeat()).count();

        bool isOnline = (elapsed < net::HEARTBEAT_TIMEOUT_MS);
        m_sidebar->updateClient(it.key(), "", "", isOnline);

        if (it.key() == m_activeClientId) {
            m_infoOnline->setText(isOnline ? "Online" : "Offline");
            const auto& t = ThemeManager::instance().currentTheme();
            m_infoOnline->setStyleSheet(QString("color: %1; font-size: 12px;")
                .arg(isOnline ? t.greenHover : t.redHover));
        }
    }
}

void MainWindow::onClientSelected(uint32_t clientId) {
    m_activeClientId = clientId;
    m_sidebar->setActiveClient(clientId);

    for (auto it = m_tiles.begin(); it != m_tiles.end(); ++it) {
        it.value()->setSelected(it.key() == clientId);
    }

    auto* session = m_sessions.value(clientId, nullptr);
    auto* tile = m_tiles.value(clientId, nullptr);
    if (!tile) return;

    QString name = tile->studentName();
    m_infoName->setText(name);

    if (session) {
        m_infoResolution->setText(QString("%1 × %2").arg(session->screenWidth()).arg(session->screenHeight()));
    }

    if (!tile->currentPixmap().isNull()) {
        m_lastSingleImage = tile->currentPixmap().toImage();
        QPixmap pixmap = tile->currentPixmap().scaled(
            m_singleScreenLabel->size(),
            Qt::KeepAspectRatio,
            Qt::FastTransformation
        );
        m_singleScreenLabel->setPixmap(pixmap);
        m_singleScreenLabel->setText("");
    } else {
        m_lastSingleImage = QImage();
        m_singleScreenLabel->setPixmap(QPixmap());
        m_singleScreenLabel->setText(QString("Ожидание трансляции %1...").arg(name));
    }

    if (m_controlEnabled) {
        if (m_controlBannerText) {
            m_controlBannerText->setText(QString("🟢 Управление: %1 — мышь перехвачена (Esc для выхода)").arg(name));
        }
        setStatusText(QString("Перехват управления мышью: %1").arg(name), "unlocked");
    } else {
        setStatusText(QString("Просмотр %1").arg(name));
    }
}

void MainWindow::onClientDoubleClicked(uint32_t clientId) {
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

void MainWindow::onLockClicked() {
    auto ids = m_sidebar->checkedClientIds();
    if (ids.isEmpty() && m_activeClientId != 0) {
        ids.append(m_activeClientId);
    }

    if (ids.isEmpty()) {
        setStatusText("Выберите ПК");
        return;
    }

    for (uint32_t id : ids) {
        if (auto* session = m_sessions.value(id, nullptr)) {
            session->sendLock();
        }
    }

    if (ids.contains(m_activeClientId)) {
        if (m_controlEnabled) {
            onToggleControl();
        }
        m_lockOverlay->setGeometry(m_singleScreenWidget->rect());
        m_lockOverlay->show();
    }

    setStatusText(ids.size() == 1 ? "ПК заблокирован" : "Все ПК заблокированы", "locked");
}

void MainWindow::onUnlockClicked() {
    auto ids = m_sidebar->checkedClientIds();
    if (ids.isEmpty() && m_activeClientId != 0) {
        ids.append(m_activeClientId);
    }

    if (ids.isEmpty()) {
        setStatusText("Выберите ПК");
        return;
    }

    for (uint32_t id : ids) {
        if (auto* session = m_sessions.value(id, nullptr)) {
            session->sendUnlock();
        }
    }

    if (ids.contains(m_activeClientId)) {
        m_lockOverlay->hide();
    }

    setStatusText(ids.size() == 1 ? "ПК разблокирован" : "Все ПК разблокированы", "unlocked");
}

void MainWindow::onToggleControl() {
    if (m_activeClientId == 0 || !m_sessions.contains(m_activeClientId)) {
        setStatusText("Выберите ПК для управления");
        return;
    }

    // Если экран был заблокирован оверлеем — разблокируем для управления
    if (m_lockOverlay && m_lockOverlay->isVisible()) {
        m_lockOverlay->hide();
        if (auto* session = m_sessions.value(m_activeClientId, nullptr)) {
            session->sendUnlock();
        }
    }

    // Если в режиме сетки — переключаемся на одиночный просмотр
    if (m_screenStack->currentIndex() != 0) {
        onSetSingleView();
    }

    m_controlEnabled = !m_controlEnabled;

    if (m_controlEnabled) {
        m_controlBtn->setText("🟢 Управление: ВКЛ");
        m_controlBtn->setProperty("active", true);
        m_controlBtn->style()->unpolish(m_controlBtn);
        m_controlBtn->style()->polish(m_controlBtn);

        m_singleScreenLabel->setCursor(Qt::CrossCursor);
        m_singleScreenLabel->setFocus();
        setFocus();

        QString name = m_infoName->text();
        if (m_controlBannerText) {
            m_controlBannerText->setText(QString("🟢 Управление: %1 — мышь перехвачена (Esc для выхода)").arg(name));
        }
        if (m_controlBanner) {
            m_controlBanner->adjustSize();
            int bx = (m_singleScreenWidget->width() - m_controlBanner->width()) / 2;
            m_controlBanner->move(qMax(10, bx), 12);
            m_controlBanner->show();
            m_controlBanner->raise();
        }

        setStatusText(QString("Перехват управления мышью активен: %1").arg(name), "unlocked");
    } else {
        m_controlBtn->setText("🖱️ Управление");
        m_controlBtn->setProperty("active", false);
        m_controlBtn->style()->unpolish(m_controlBtn);
        m_controlBtn->style()->polish(m_controlBtn);

        m_singleScreenLabel->setCursor(Qt::ArrowCursor);
        if (m_controlBanner) {
            m_controlBanner->hide();
        }

        setStatusText("Управление мышью отключено");
    }
}

void MainWindow::onScreenshotClicked() {
    if (m_singleScreenLabel->pixmap().isNull()) {
        setStatusText("Нет изображения для снимка");
        return;
    }

    QString fileName = QString("screenshot_%1_%2.png")
        .arg(m_infoName->text())
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));

    QString savePath = QFileDialog::getSaveFileName(this, "Сохранить скриншот", fileName, "Images (*.png *.jpg)");
    if (!savePath.isEmpty()) {
        m_singleScreenLabel->pixmap().save(savePath);
        setStatusText("Скриншот сохранён");
    }
}

void MainWindow::onToggleFullscreen() {
    if (isFullScreen()) {
        showNormal();
        m_fullscreenBtn->setText("Во весь экран");
    } else {
        showFullScreen();
        m_fullscreenBtn->setText("Выйти из полноэкранного");
    }
}

void MainWindow::onSettingsClicked() {
    SettingsDialog dlg(this);
    connect(&dlg, &SettingsDialog::themeChanged, this, &MainWindow::applyTheme);
    dlg.exec();
}

void MainWindow::onSetSingleView() {
    m_screenStack->setCurrentIndex(0);
    m_singleViewBtn->setProperty("active", true);
    m_gridViewBtn->setProperty("active", false);
    m_singleViewBtn->style()->unpolish(m_singleViewBtn);
    m_singleViewBtn->style()->polish(m_singleViewBtn);
    m_gridViewBtn->style()->unpolish(m_gridViewBtn);
    m_gridViewBtn->style()->polish(m_gridViewBtn);
    setStatusText("Одиночный просмотр");
}

void MainWindow::onSetGridView() {
    if (m_controlEnabled) {
        onToggleControl();
    }
    m_screenStack->setCurrentIndex(1);
    m_singleViewBtn->setProperty("active", false);
    m_gridViewBtn->setProperty("active", true);
    m_singleViewBtn->style()->unpolish(m_singleViewBtn);
    m_singleViewBtn->style()->polish(m_singleViewBtn);
    m_gridViewBtn->style()->unpolish(m_gridViewBtn);
    m_gridViewBtn->style()->polish(m_gridViewBtn);
    setStatusText("Вид сетки");
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (m_controlEnabled && m_activeClientId != 0) {
        if (event->key() == Qt::Key_Escape) {
            onToggleControl();
            return;
        }
        if (auto* session = m_sessions.value(m_activeClientId, nullptr)) {
            uint16_t vk = static_cast<uint16_t>(event->nativeVirtualKey());
            uint16_t scan = static_cast<uint16_t>(event->nativeScanCode());
            if (vk > 0) {
                session->sendKeyPress(vk, scan, 0);
                return;
            }
        }
    }

    if (event->key() == Qt::Key_Escape) {
        if (isFullScreen()) {
            showNormal();
            m_fullscreenBtn->setText("Во весь экран");
        }
        return;
    }
    if (event->key() == Qt::Key_F11) {
        onToggleFullscreen();
        return;
    }
    if (event->key() == Qt::Key_C) {
        onToggleControl();
        return;
    }
    if (event->key() == Qt::Key_L) {
        onLockClicked();
        return;
    }
    if (event->key() == Qt::Key_U) {
        onUnlockClicked();
        return;
    }
    if (event->key() == Qt::Key_S) {
        onScreenshotClicked();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent* event) {
    if (m_controlEnabled && m_activeClientId != 0) {
        if (auto* session = m_sessions.value(m_activeClientId, nullptr)) {
            uint16_t vk = static_cast<uint16_t>(event->nativeVirtualKey());
            uint16_t scan = static_cast<uint16_t>(event->nativeScanCode());
            if (vk > 0) {
                session->sendKeyRelease(vk, scan, 0);
                return;
            }
        }
    }
    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    if (m_lockOverlay && m_lockOverlay->isVisible() && m_singleScreenWidget) {
        m_lockOverlay->setGeometry(m_singleScreenWidget->rect());
    }
    if (m_controlBanner && m_controlBanner->isVisible() && m_singleScreenWidget) {
        int bx = (m_singleScreenWidget->width() - m_controlBanner->width()) / 2;
        m_controlBanner->move(qMax(10, bx), 12);
        m_controlBanner->raise();
    }
    if (!m_lastSingleImage.isNull() && m_screenStack && m_screenStack->currentIndex() == 0 && m_singleScreenLabel) {
        QPixmap pixmap = QPixmap::fromImage(m_lastSingleImage).scaled(
            m_singleScreenLabel->size(),
            Qt::KeepAspectRatio,
            Qt::FastTransformation
        );
        m_singleScreenLabel->setPixmap(pixmap);
    }
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_singleScreenLabel && m_controlEnabled && m_activeClientId != 0) {
        if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            sendRemoteMouse(me, 0); // move
            return true;
        } else if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            m_singleScreenLabel->setFocus();
            sendRemoteMouse(me, 1); // press
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto* me = static_cast<QMouseEvent*>(event);
            sendRemoteMouse(me, 2); // release
            return true;
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            auto* me = static_cast<QMouseEvent*>(event);
            sendRemoteMouse(me, 1); // press
            sendRemoteMouse(me, 2); // release
            return true;
        } else if (event->type() == QEvent::Wheel) {
            auto* we = static_cast<QWheelEvent*>(event);
            QPointF normPos = mapToImageNormalized(we->position());
            if (normPos.x() >= 0.0f) {
                if (auto* session = m_sessions.value(m_activeClientId, nullptr)) {
                    QPoint angleDelta = we->angleDelta();
                    int16_t deltaX = static_cast<int16_t>(angleDelta.x() / 120);
                    int16_t deltaY = static_cast<int16_t>(angleDelta.y() / 120);
                    session->sendMouseScroll(
                        static_cast<float>(normPos.x()),
                        static_cast<float>(normPos.y()),
                        deltaX, deltaY);
                }
            }
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

QPointF MainWindow::mapToImageNormalized(QPointF localPos) {
    if (m_lastSingleImage.isNull() || !m_singleScreenLabel || m_singleScreenLabel->width() == 0 || m_singleScreenLabel->height() == 0) {
        return QPointF(-1.0, -1.0);
    }

    // Вычисляем реальную область масштабированного изображения внутри QLabel
    QSize labelSize = m_singleScreenLabel->size();
    QSize imgSize = m_lastSingleImage.size().scaled(labelSize, Qt::KeepAspectRatio);
    int offsetX = (labelSize.width() - imgSize.width()) / 2;
    int offsetY = (labelSize.height() - imgSize.height()) / 2;

    float imgX = static_cast<float>(localPos.x() - offsetX);
    float imgY = static_cast<float>(localPos.y() - offsetY);

    if (imgSize.width() <= 0 || imgSize.height() <= 0) {
        return QPointF(-1.0, -1.0);
    }

    // Проверяем попадание курсора в область изображения
    if (imgX < 0.0f || imgX > imgSize.width() || imgY < 0.0f || imgY > imgSize.height()) {
        return QPointF(-1.0, -1.0);
    }

    float normX = imgX / static_cast<float>(imgSize.width());
    float normY = imgY / static_cast<float>(imgSize.height());

    normX = qBound(0.0f, normX, 1.0f);
    normY = qBound(0.0f, normY, 1.0f);

    return QPointF(normX, normY);
}

void MainWindow::sendRemoteMouse(QMouseEvent* event, uint8_t action) {
    if (!m_controlEnabled || m_activeClientId == 0) return;
    auto* session = m_sessions.value(m_activeClientId, nullptr);
    if (!session) return;

    QPointF normPos = mapToImageNormalized(event->position());
    if (normPos.x() < 0.0f) return;

    float normX = static_cast<float>(normPos.x());
    float normY = static_cast<float>(normPos.y());

    if (action == 0) { // MouseMove
        auto now = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_lastMouseMoveTime).count();

        // Ограничиваем частоту отправки движения мыши ~60 Гц (раз в 15 мс)
        // либо если курсор сдвинулся значительно, чтобы не перегружать сеть и TCP сокет
        float dx = std::abs(normX - static_cast<float>(m_lastNormPos.x()));
        float dy = std::abs(normY - static_cast<float>(m_lastNormPos.y()));

        if (elapsedMs < 15 && (dx < 0.003f && dy < 0.003f)) {
            return;
        }

        m_lastMouseMoveTime = now;
        m_lastNormPos = QPointF(normX, normY);
        session->sendMouseMove(normX, normY);
    } else {
        m_lastMouseMoveTime = std::chrono::steady_clock::now();
        m_lastNormPos = QPointF(normX, normY);

        uint8_t btn = 0; // 0=left, 1=right, 2=middle
        if (event->button() == Qt::RightButton) btn = 1;
        else if (event->button() == Qt::MiddleButton) btn = 2;

        session->sendMouseClick(normX, normY, btn, action == 1 ? 0 : 1);
    }
}

void MainWindow::setStatusText(const QString& text, const QString& statusClass) {
    m_statusLabel->setText(text);
    const auto& t = ThemeManager::instance().currentTheme();
    if (statusClass == "locked") {
        m_statusLabel->setStyleSheet(QString("font-size: 12px; color: %1; padding: 0 8px; font-weight: bold;").arg(t.redHover));
    } else if (statusClass == "unlocked") {
        m_statusLabel->setStyleSheet(QString("font-size: 12px; color: %1; padding: 0 8px; font-weight: bold;").arg(t.greenHover));
    } else {
        m_statusLabel->setStyleSheet(QString("font-size: 12px; color: %1; padding: 0 8px;").arg(t.textMuted));
    }
}

void MainWindow::updateGrid() {
    const int columns = 2; // Как в HTML: repeat(2, 1fr)
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
