#pragma once
// =============================================================================
// ClassroomMonitor — Message Dialog
//
// Модальное окно отправки сообщения ученикам по HTML-макету.
// =============================================================================

#include <QDialog>
#include <QTextEdit>

namespace cm {
namespace server {

class MessageDialog : public QDialog {
    Q_OBJECT

public:
    explicit MessageDialog(QWidget* parent = nullptr);
    ~MessageDialog() override = default;

    QString messageText() const;

signals:
    void messageSent(const QString& text);

private:
    void setupUi();
    QTextEdit* m_textEdit = nullptr;
};

} // namespace server
} // namespace cm
