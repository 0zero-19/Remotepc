// =============================================================================
// ClassroomMonitor — Theme Manager Implementation
// =============================================================================

#include "server/ThemeManager.h"

namespace cm {
namespace server {

ThemeManager& ThemeManager::instance() {
    static ThemeManager mgr;
    return mgr;
}

ThemeManager::ThemeManager() {
    initThemes();
    setTheme("gruvbox");
}

void ThemeManager::initThemes() {
    // 1. Gruvbox (По умолчанию)
    Theme gruvbox;
    gruvbox.name = "gruvbox";
    gruvbox.displayName = "Gruvbox";
    gruvbox.description = "Тёмная, тёплая";
    gruvbox.bg = "#282828";
    gruvbox.surface = "#32302f";
    gruvbox.surface2 = "#3c3836";
    gruvbox.surface3 = "#45403d";
    gruvbox.border = "#504945";
    gruvbox.text = "#ebdbb2";
    gruvbox.textMuted = "#a89984";
    gruvbox.green = "#98971a";
    gruvbox.greenHover = "#b8bb26";
    gruvbox.red = "#cc241d";
    gruvbox.redHover = "#fb4934";
    gruvbox.yellow = "#d79921";
    gruvbox.orange = "#d65d0e";
    m_themes.insert(gruvbox.name, gruvbox);

    // 2. Nord
    Theme nord;
    nord.name = "nord";
    nord.displayName = "Nord";
    nord.description = "Холодная тёмная";
    nord.bg = "#2e3440";
    nord.surface = "#3b4252";
    nord.surface2 = "#434c5e";
    nord.surface3 = "#4c566a";
    nord.border = "#616b7d";
    nord.text = "#eceff4";
    nord.textMuted = "#aeb8c8";
    nord.green = "#a3be8c";
    nord.greenHover = "#b7d59e";
    nord.red = "#bf616a";
    nord.redHover = "#d0878f";
    nord.yellow = "#ebcb8b";
    nord.orange = "#d08770";
    m_themes.insert(nord.name, nord);

    // 3. Dracula
    Theme dracula;
    dracula.name = "dracula";
    dracula.displayName = "Dracula";
    dracula.description = "Тёмная контрастная";
    dracula.bg = "#282a36";
    dracula.surface = "#303341";
    dracula.surface2 = "#44475a";
    dracula.surface3 = "#50536a";
    dracula.border = "#6272a4";
    dracula.text = "#f8f8f2";
    dracula.textMuted = "#a9adc1";
    dracula.green = "#50fa7b";
    dracula.greenHover = "#69ff8f";
    dracula.red = "#ff5555";
    dracula.redHover = "#ff7070";
    dracula.yellow = "#f1fa8c";
    dracula.orange = "#ffb86c";
    m_themes.insert(dracula.name, dracula);

    // 4. One Dark
    Theme onedark;
    onedark.name = "onedark";
    onedark.displayName = "One Dark";
    onedark.description = "Тёмная нейтральная";
    onedark.bg = "#282c34";
    onedark.surface = "#2f343d";
    onedark.surface2 = "#3e444d";
    onedark.surface3 = "#4b525c";
    onedark.border = "#545c68";
    onedark.text = "#abb2bf";
    onedark.textMuted = "#7f8794";
    onedark.green = "#98c379";
    onedark.greenHover = "#b0d994";
    onedark.red = "#e06c75";
    onedark.redHover = "#ef858d";
    onedark.yellow = "#e5c07b";
    onedark.orange = "#d19a66";
    m_themes.insert(onedark.name, onedark);

    // 5. Light
    Theme light;
    light.name = "light";
    light.displayName = "Light";
    light.description = "Светлая";
    light.bg = "#eeeeee";
    light.surface = "#ffffff";
    light.surface2 = "#e5e5e5";
    light.surface3 = "#d8d8d8";
    light.border = "#cccccc";
    light.text = "#303030";
    light.textMuted = "#707070";
    light.green = "#5f7f32";
    light.greenHover = "#739b3e";
    light.red = "#b83b3b";
    light.redHover = "#d24a4a";
    light.yellow = "#a87816";
    light.orange = "#b35b18";
    m_themes.insert(light.name, light);

    // 6. Solarized
    Theme solarized;
    solarized.name = "solarized";
    solarized.displayName = "Solarized";
    solarized.description = "Светлая тёплая";
    solarized.bg = "#fdf6e3";
    solarized.surface = "#eee8d5";
    solarized.surface2 = "#e5dfca";
    solarized.surface3 = "#d9d2bd";
    solarized.border = "#c9c1a8";
    solarized.text = "#586e75";
    solarized.textMuted = "#839496";
    solarized.green = "#859900";
    solarized.greenHover = "#9aaa00";
    solarized.red = "#dc322f";
    solarized.redHover = "#ed4643";
    solarized.yellow = "#b58900";
    solarized.orange = "#cb4b16";
    m_themes.insert(solarized.name, solarized);
}

void ThemeManager::setTheme(const QString& name) {
    if (m_themes.contains(name)) {
        m_currentTheme = m_themes[name];
    }
}

QList<Theme> ThemeManager::allThemes() const {
    return m_themes.values();
}

QString ThemeManager::generateStyleSheet(const QString& themeName) const {
    const Theme& t = themeName.isEmpty() ? m_currentTheme : m_themes.value(themeName, m_currentTheme);

    return QString(R"(
        QWidget {
            font-family: "Ubuntu", "Segoe UI", sans-serif;
            color: %1;
            background: transparent;
        }

        QMainWindow, QDialog {
            background-color: %2;
        }

        /* Боковая панель клиентов */
        QFrame#clientsPanel {
            background-color: %3;
            border: 1px solid %4;
            border-radius: 7px;
        }

        QWidget#clientsTop {
            border-bottom: 1px solid %4;
            padding: 10px;
        }

        /* Кнопки "Все" / "Снять" */
        QPushButton.smallButton {
            height: 32px;
            padding: 0 11px;
            background-color: %5;
            border: 1px solid %4;
            border-radius: 5px;
            color: %6;
            font-size: 13px;
        }
        QPushButton.smallButton:hover {
            background-color: %7;
            color: %1;
        }

        /* Список клиентов */
        QScrollArea#clientsScroll {
            border: none;
            background: transparent;
        }

        /* Карточка клиента в списке */
        QFrame.clientItem {
            background-color: transparent;
            border: 1px solid transparent;
            border-radius: 5px;
            padding: 6px;
        }
        QFrame.clientItem:hover {
            background-color: %5;
        }
        QFrame.clientItem[active="true"] {
            background-color: %5;
            border-color: %4;
        }

        /* Чекбокс */
        QCheckBox {
            spacing: 6px;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid %4;
            border-radius: 4px;
            background: %5;
        }
        QCheckBox::indicator:checked {
            background: %8;
            border-color: %8;
        }

        /* Верхняя панель информации (Info bar) */
        QFrame#infoBar {
            background-color: %3;
            border: 1px solid %4;
            border-radius: 7px;
            min-height: 42px;
            max-height: 42px;
        }

        QPushButton.viewButton {
            width: 32px;
            height: 28px;
            background-color: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            color: %6;
            font-weight: bold;
            font-size: 12px;
        }
        QPushButton.viewButton:hover, QPushButton.viewButton[active="true"] {
            background-color: %5;
            border-color: %4;
            color: %1;
        }

        /* Экранная область (Screen wrapper) */
        QFrame#screenWrapper {
            background-color: #1f1e1d;
            border: 1px solid %4;
            border-radius: 7px;
        }

        /* Нижняя панель управления (Control bar) */
        QFrame#controlBar {
            background-color: %3;
            border: 1px solid %4;
            border-radius: 7px;
            min-height: 52px;
            max-height: 52px;
        }

        /* Кнопка Заблокировать (Красная) */
        QPushButton#lockButton {
            height: 36px;
            padding: 0 14px;
            background-color: %9;
            border: none;
            border-radius: 5px;
            color: #ffffff;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton#lockButton:hover {
            background-color: %10;
        }

        /* Кнопка Разблокировать (Зелёная) */
        QPushButton#unlockButton {
            height: 36px;
            padding: 0 14px;
            background-color: %8;
            border: none;
            border-radius: 5px;
            color: #ffffff;
            font-size: 13px;
            font-weight: bold;
        }
        QPushButton#unlockButton:hover {
            background-color: %11;
        }

        /* Второстепенные кнопки (Сообщение, Скриншот, Во весь экран) */
        QPushButton.secondaryButton {
            height: 36px;
            padding: 0 12px;
            background-color: %5;
            border: 1px solid %4;
            border-radius: 5px;
            color: %1;
            font-size: 13px;
        }
        QPushButton.secondaryButton:hover {
            background-color: %7;
        }

        /* Кнопка настроек */
        QPushButton#settingsButton {
            width: 36px;
            height: 36px;
            background-color: %5;
            border: 1px solid %4;
            border-radius: 5px;
            color: %6;
            font-size: 15px;
        }
        QPushButton#settingsButton:hover {
            background-color: %7;
            color: %1;
        }

        /* Скроллбары */
        QScrollBar:vertical {
            border: none;
            background: transparent;
            width: 6px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: %4;
            min-height: 20px;
            border-radius: 3px;
        }
        QScrollBar::handle:vertical:hover {
            background: %6;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }

        /* Диалоги и модалки */
        QTextEdit, QPlainTextEdit {
            background-color: %5;
            color: %1;
            border: 1px solid %4;
            border-radius: 5px;
            padding: 8px;
        }
        QTextEdit:focus {
            border-color: %12;
        }
    )")
    .arg(t.text)        // 1
    .arg(t.bg)          // 2
    .arg(t.surface)     // 3
    .arg(t.border)      // 4
    .arg(t.surface2)    // 5
    .arg(t.textMuted)   // 6
    .arg(t.surface3)    // 7
    .arg(t.green)       // 8
    .arg(t.red)         // 9
    .arg(t.redHover)    // 10
    .arg(t.greenHover)  // 11
    .arg(t.yellow);     // 12
}

} // namespace server
} // namespace cm
