#pragma once
// =============================================================================
// ClassroomMonitor — Settings Dialog
//
// Модальное окно выбора тем интерфейса (Gruvbox, Nord, Dracula, One Dark, Light, Solarized)
// в точном соответствии с HTML-макетом.
// =============================================================================

#include <QDialog>
#include "server/ThemeManager.h"

namespace cm {
namespace server {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

signals:
    void themeChanged(const QString& themeName);

private:
    void setupUi();
};

} // namespace server
} // namespace cm
