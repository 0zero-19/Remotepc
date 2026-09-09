// =============================================================================
// ClassroomMonitor — Control Panel Implementation
// =============================================================================

#include "server/ControlPanel.h"

#include <QHBoxLayout>

namespace cm {
namespace server {

ControlPanel::ControlPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

ControlPanel::~ControlPanel() = default;

void ControlPanel::setupUi() {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);

    // Кнопка блокировки
    m_lockBtn = new QPushButton("🔒 Заблокировать всех", this);
    m_lockBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #e94560;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 8px 16px;"
        "  font-size: 13px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #c73e54; }"
        "QPushButton:pressed { background-color: #a33548; }"
    );
    connect(m_lockBtn, &QPushButton::clicked, this, &ControlPanel::onLockAll);
    layout->addWidget(m_lockBtn);

    // Кнопка разблокировки
    m_unlockBtn = new QPushButton("🔓 Разблокировать всех", this);
    m_unlockBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #4caf50;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 8px 16px;"
        "  font-size: 13px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #43a047; }"
        "QPushButton:pressed { background-color: #388e3c; }"
    );
    connect(m_unlockBtn, &QPushButton::clicked, this, &ControlPanel::onUnlockAll);
    layout->addWidget(m_unlockBtn);

    layout->addStretch();

    // Статус
    m_statusLabel = new QLabel("Готов", this);
    m_statusLabel->setStyleSheet("color: #888; font-size: 12px;");
    layout->addWidget(m_statusLabel);

    // Стиль панели
    setStyleSheet(
        "ControlPanel {"
        "  background-color: #0f3460;"
        "  border-radius: 8px;"
        "}"
    );
    setFixedHeight(50);
}

void ControlPanel::onLockAll() {
    m_statusLabel->setText("🔒 Заблокировано");
    m_statusLabel->setStyleSheet("color: #e94560; font-size: 12px;");
    emit commandIssued("lock_all");
}

void ControlPanel::onUnlockAll() {
    m_statusLabel->setText("🔓 Разблокировано");
    m_statusLabel->setStyleSheet("color: #4caf50; font-size: 12px;");
    emit commandIssued("unlock_all");
}

} // namespace server
} // namespace cm
