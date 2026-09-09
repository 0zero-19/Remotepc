#pragma once
// =============================================================================
// ClassroomMonitor — Control Panel
//
// Панель с кнопками управления: блокировка, разблокировка, качество.
// =============================================================================

#include <QWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>

namespace cm {
namespace server {

class ControlPanel : public QWidget {
    Q_OBJECT

public:
    explicit ControlPanel(QWidget* parent = nullptr);
    ~ControlPanel() override;

signals:
    /// Издаёт команду: "lock_all", "unlock_all", и т.д.
    void commandIssued(const QString& command);

private slots:
    void onLockAll();
    void onUnlockAll();

private:
    void setupUi();

    QPushButton* m_lockBtn    = nullptr;
    QPushButton* m_unlockBtn  = nullptr;
    QLabel*      m_statusLabel = nullptr;
};

} // namespace server
} // namespace cm
