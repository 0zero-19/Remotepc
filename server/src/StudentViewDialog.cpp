// =============================================================================
// ClassroomMonitor — Student View Dialog Implementation
// =============================================================================

#include "server/StudentViewDialog.h"

#include <QApplication>
#include <QMessageBox>
#include <QDebug>

namespace cm {
namespace server {

StudentViewDialog::StudentViewDialog(uint32_t clientId, ClientSession* session, QWidget* parent)
    : QDialog(parent)
    , m_clientId(clientId)
    , m_session(session)
{
    setupUi();

    QString title = QString("Экран студента: %1").arg(m_session ? m_session->hostname() : QString::number(clientId));
    if (m_session && !m_session->username().isEmpty()) {
        title += QString(" (%1)").arg(m_session->username());
    }
    setWindowTitle(title);
    resize(1200, 750);
}

void StudentViewDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Панель управления сверху
    auto* topBar = new QWidget(this);
    topBar->setStyleSheet("background-color: #1a1a2e; border-bottom: 1px solid #0f3460;");
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(12, 8, 12, 8);

    // Информация
    QString infoText = QString("💻 ПК: %1 | 👤 %2")
        .arg(m_session ? m_session->hostname() : "—")
        .arg(m_session ? m_session->username() : "—");
    m_infoLabel = new QLabel(infoText, topBar);
    m_infoLabel->setStyleSheet("color: #00e676; font-weight: bold; font-size: 14px;");
    topLayout->addWidget(m_infoLabel);

    topLayout->addStretch();

    // Кнопка управления
    m_controlBtn = new QPushButton("🖱️ Управление: ВЫКЛ", topBar);
    m_controlBtn->setStyleSheet(
        "QPushButton { background-color: #2c3e50; color: white; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #34495e; }"
    );
    connect(m_controlBtn, &QPushButton::clicked, this, &StudentViewDialog::onToggleControl);
    topLayout->addWidget(m_controlBtn);

    // Кнопка блокировки
    m_lockBtn = new QPushButton("🔒 Заблокировать", topBar);
    m_lockBtn->setStyleSheet(
        "QPushButton { background-color: #e94560; color: white; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #ff6b81; }"
    );
    connect(m_lockBtn, &QPushButton::clicked, this, &StudentViewDialog::onLockClicked);
    topLayout->addWidget(m_lockBtn);

    // Кнопка разблокировки
    m_unlockBtn = new QPushButton("🔓 Разблокировать", topBar);
    m_unlockBtn->setStyleSheet(
        "QPushButton { background-color: #4caf50; color: white; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #66bb6a; }"
    );
    connect(m_unlockBtn, &QPushButton::clicked, this, &StudentViewDialog::onUnlockClicked);
    topLayout->addWidget(m_unlockBtn);

    // Во весь экран
    m_fsBtn = new QPushButton("⛶ Во весь экран", topBar);
    m_fsBtn->setStyleSheet(
        "QPushButton { background-color: #0f3460; color: white; border-radius: 4px; padding: 6px 12px; }"
        "QPushButton:hover { background-color: #16213e; }"
    );
    connect(m_fsBtn, &QPushButton::clicked, this, &StudentViewDialog::onToggleFullscreen);
    topLayout->addWidget(m_fsBtn);

    // Закрыть
    auto* closeBtn = new QPushButton("✖", topBar);
    closeBtn->setStyleSheet(
        "QPushButton { background-color: transparent; color: #888; border-radius: 4px; padding: 6px 10px; font-size: 16px; }"
        "QPushButton:hover { background-color: #c0392b; color: white; }"
    );
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
    topLayout->addWidget(closeBtn);

    mainLayout->addWidget(topBar);

    // Область видео
    m_videoLabel = new QLabel(this);
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setStyleSheet("background-color: #000000;");
    m_videoLabel->setText("Получение видеопотока студента...");
    m_videoLabel->setMouseTracking(true);
    m_videoLabel->installEventFilter(this);

    mainLayout->addWidget(m_videoLabel, 1);
}

void StudentViewDialog::updateFrame(const QByteArray& frameData, uint16_t width, uint16_t height) {
    if (frameData.isEmpty()) return;

    m_origWidth = width;
    m_origHeight = height;

    QImage img;
    if (img.loadFromData(frameData, "JPEG") || img.loadFromData(frameData)) {
        m_lastImage = img;
        QPixmap pixmap = QPixmap::fromImage(img).scaled(
            m_videoLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        m_videoLabel->setPixmap(pixmap);
    }
}

void StudentViewDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
    if (!m_lastImage.isNull()) {
        QPixmap pixmap = QPixmap::fromImage(m_lastImage).scaled(
            m_videoLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        m_videoLabel->setPixmap(pixmap);
    }
}

void StudentViewDialog::onLockClicked() {
    if (m_session) {
        m_session->sendLock();
    }
}

void StudentViewDialog::onUnlockClicked() {
    if (m_session) {
        m_session->sendUnlock();
    }
}

void StudentViewDialog::onToggleControl() {
    m_remoteControlEnabled = !m_remoteControlEnabled;
    if (m_remoteControlEnabled) {
        m_controlBtn->setText("🟢 Управление: ВКЛ (Активно)");
        m_controlBtn->setStyleSheet(
            "QPushButton { background-color: #00e676; color: #000; border-radius: 4px; padding: 6px 14px; font-weight: bold; border: 2px solid #00c853; }"
        );
        m_videoLabel->setFocusPolicy(Qt::StrongFocus);
        m_videoLabel->setFocus();
        setFocus();
    } else {
        m_controlBtn->setText("🖱️ Управление: ВЫКЛ");
        m_controlBtn->setStyleSheet(
            "QPushButton { background-color: #2c3e50; color: white; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
            "QPushButton:hover { background-color: #34495e; }"
        );
    }
}

void StudentViewDialog::onToggleFullscreen() {
    if (isFullScreen()) {
        showNormal();
        m_fsBtn->setText("⛶ Во весь экран");
    } else {
        showFullScreen();
        m_fsBtn->setText("❐ Оконный режим");
    }
}

bool StudentViewDialog::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_videoLabel && m_remoteControlEnabled && m_session) {
        if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            sendRemoteMouse(me, 0); // move
            return true;
        } else if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            m_videoLabel->setFocus();
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
                QPoint angleDelta = we->angleDelta();
                int16_t deltaX = static_cast<int16_t>(angleDelta.x() / 120);
                int16_t deltaY = static_cast<int16_t>(angleDelta.y() / 120);
                m_session->sendMouseScroll(
                    static_cast<float>(normPos.x()),
                    static_cast<float>(normPos.y()),
                    deltaX, deltaY);
            }
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

QPointF StudentViewDialog::mapToImageNormalized(QPointF localPos) {
    if (m_lastImage.isNull() || m_videoLabel->width() == 0 || m_videoLabel->height() == 0) {
        return QPointF(-1.0, -1.0);
    }

    // Вычисляем реальную область масштабированного изображения внутри QLabel
    QSize labelSize = m_videoLabel->size();
    QSize imgSize = m_lastImage.size().scaled(labelSize, Qt::KeepAspectRatio);
    int offsetX = (labelSize.width() - imgSize.width()) / 2;
    int offsetY = (labelSize.height() - imgSize.height()) / 2;

    float imgX = static_cast<float>(localPos.x() - offsetX);
    float imgY = static_cast<float>(localPos.y() - offsetY);

    if (imgSize.width() <= 0 || imgSize.height() <= 0) {
        return QPointF(-1.0, -1.0);
    }

    float normX = imgX / static_cast<float>(imgSize.width());
    float normY = imgY / static_cast<float>(imgSize.height());

    normX = qBound(0.0f, normX, 1.0f);
    normY = qBound(0.0f, normY, 1.0f);

    return QPointF(normX, normY);
}

void StudentViewDialog::sendRemoteMouse(QMouseEvent* event, uint8_t action) {
    if (!m_session) return;

    QPointF normPos = mapToImageNormalized(event->position());
    if (normPos.x() < 0.0f) return;

    float normX = static_cast<float>(normPos.x());
    float normY = static_cast<float>(normPos.y());

    if (action == 0) {
        m_session->sendMouseMove(normX, normY);
    } else {
        uint8_t btn = 0; // 0=left, 1=right, 2=middle
        if (event->button() == Qt::RightButton) btn = 1;
        else if (event->button() == Qt::MiddleButton) btn = 2;

        m_session->sendMouseClick(normX, normY, btn, action == 1 ? 0 : 1);
    }
}

void StudentViewDialog::keyPressEvent(QKeyEvent* event) {
    if (m_remoteControlEnabled && m_session) {
        uint16_t vk = static_cast<uint16_t>(event->nativeVirtualKey());
        uint16_t scan = static_cast<uint16_t>(event->nativeScanCode());
        if (vk > 0) {
            m_session->sendKeyPress(vk, scan, 0);
            return;
        }
    }
    QDialog::keyPressEvent(event);
}

void StudentViewDialog::keyReleaseEvent(QKeyEvent* event) {
    if (m_remoteControlEnabled && m_session) {
        uint16_t vk = static_cast<uint16_t>(event->nativeVirtualKey());
        uint16_t scan = static_cast<uint16_t>(event->nativeScanCode());
        if (vk > 0) {
            m_session->sendKeyRelease(vk, scan, 0);
            return;
        }
    }
    QDialog::keyReleaseEvent(event);
}

} // namespace server
} // namespace cm

