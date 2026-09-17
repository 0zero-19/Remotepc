#pragma once
// =============================================================================
// ClassroomMonitor — Theme Manager
//
// Управление темами оформления по минималистичному HTML макету:
// Gruvbox, Nord, Dracula, One Dark, Light, Solarized.
// =============================================================================

#include <QString>
#include <QColor>
#include <QMap>

namespace cm {
namespace server {

struct Theme {
    QString name;
    QString displayName;
    QString description;
    QString bg;
    QString surface;
    QString surface2;
    QString surface3;
    QString border;
    QString text;
    QString textMuted;
    QString green;
    QString greenHover;
    QString red;
    QString redHover;
    QString yellow;
    QString orange;
};

class ThemeManager {
public:
    static ThemeManager& instance();

    const Theme& currentTheme() const { return m_currentTheme; }
    QString currentThemeName() const { return m_currentTheme.name; }

    void setTheme(const QString& name);
    QString generateStyleSheet(const QString& themeName = "") const;

    QList<Theme> allThemes() const;

private:
    ThemeManager();
    void initThemes();

    QMap<QString, Theme> m_themes;
    Theme m_currentTheme;
};

} // namespace server
} // namespace cm
