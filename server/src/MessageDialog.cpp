// =============================================================================
// ClassroomMonitor — Message Dialog Implementation
// =============================================================================

#include "server/MessageDialog.h"
#include "server/ThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace cm {
namespace server {

MessageDialog::MessageDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setFixedWidth(380);
    setupUi();
}

void MessageDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    auto* title = new QLabel("Сообщение", this);
    title->setStyleSheet("font-size: 15px; font-weight: bold;");
    layout->addWidget(title);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setPlaceholderText("Текст сообщения для выбранных учеников...");
    m_textEdit->setMinimumHeight(110);
    layout->addWidget(m_textEdit);

    auto* actions = new QHBoxLayout();
    actions->addStretch();

    auto* cancelBtn = new QPushButton("Отмена", this);
    cancelBtn->setProperty("class", "secondaryButton");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    actions->addWidget(cancelBtn);

    auto* sendBtn = new QPushButton("Отправить", this);
    sendBtn->setObjectName("unlockButton");
    sendBtn->setCursor(Qt::PointingHandCursor);
    connect(sendBtn, &QPushButton::clicked, this, [this]() {
        QString text = messageText().trimmed();
        if (!text.isEmpty()) {
            emit messageSent(text);
            accept();
        }
    });
    actions->addWidget(sendBtn);

    layout->addLayout(actions);
}

QString MessageDialog::messageText() const {
    return m_textEdit ? m_textEdit->toPlainText() : QString();
}

} // namespace server
} // namespace cm
