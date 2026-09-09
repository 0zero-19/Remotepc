#pragma once
// =============================================================================
// ClassroomMonitor — Student View Dialog
//
// Большое окно детального просмотра экрана студента в высоком разрешении
// и удалённого управления мышью и клавиатурой.
// =============================================================================

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>

#include "server/ClientSession.h"

namespace cm {
namespace server {

class StudentViewDialog : public QDialog {
    Q_OBJECT

public:
    explicit StudentViewDialog(uint32_t clientId, ClientSession* session, QWidget* parent = nullptr);
    ~StudentViewDialog() override = default;

public slots:
    /// Обновить изображение экрана
    void updateFrame(const QByteArray& frameData, uint16_t width, uint16_t height);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private slots:
    void onLockClicked();
    void onUnlockClicked();
    void onToggleControl();
    void onToggleFullscreen();

private:
    void setupUi();
    void sendRemoteMouse(QMouseEvent* event, uint8_t action);

    uint32_t       m_clientId;
    ClientSession* m_session;

    QLabel*        m_videoLabel = nullptr;
    QLabel*        m_infoLabel  = nullptr;
    QPushButton*   m_controlBtn = nullptr;
    QPushButton*   m_lockBtn    = nullptr;
    QPushButton*   m_unlockBtn  = nullptr;
    QPushButton*   m_fsBtn      = nullptr;

    bool           m_remoteControlEnabled = false;
    QImage         m_lastImage;
    uint16_t       m_origWidth = 1920;
    uint16_t       m_origHeight = 1080;
};

} // namespace server
} // namespace cm
