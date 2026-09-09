// =============================================================================
// ClassroomMonitor — Teacher Panel Entry Point
// =============================================================================

#include "server/MainWindow.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Тёмная тема
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window,          QColor(22, 33, 62));
    darkPalette.setColor(QPalette::WindowText,      QColor(224, 224, 224));
    darkPalette.setColor(QPalette::Base,             QColor(15, 52, 96));
    darkPalette.setColor(QPalette::AlternateBase,    QColor(22, 33, 62));
    darkPalette.setColor(QPalette::ToolTipBase,      QColor(255, 255, 255));
    darkPalette.setColor(QPalette::ToolTipText,      QColor(255, 255, 255));
    darkPalette.setColor(QPalette::Text,             QColor(224, 224, 224));
    darkPalette.setColor(QPalette::Button,           QColor(15, 52, 96));
    darkPalette.setColor(QPalette::ButtonText,       QColor(224, 224, 224));
    darkPalette.setColor(QPalette::BrightText,       QColor(233, 69, 96));
    darkPalette.setColor(QPalette::Link,             QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight,        QColor(233, 69, 96));
    darkPalette.setColor(QPalette::HighlightedText,  QColor(255, 255, 255));

    app.setPalette(darkPalette);

    // Глобальный стиль
    app.setStyleSheet(
        "QToolTip { color: #e0e0e0; background-color: #16213e; "
        "border: 1px solid #0f3460; padding: 4px; }"
        "QMenuBar { background-color: #0f3460; color: #e0e0e0; }"
        "QMenuBar::item:selected { background-color: #e94560; }"
        "QMenu { background-color: #16213e; color: #e0e0e0; }"
        "QMenu::item:selected { background-color: #e94560; }"
        "QStatusBar { background-color: #0f3460; color: #aaa; }"
        "QScrollArea { border: none; }"
    );

    cm::server::MainWindow window;
    window.show();

    return app.exec();
}
