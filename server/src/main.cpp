// =============================================================================
// ClassroomMonitor — Teacher Panel Entry Point
// =============================================================================

#include "server/MainWindow.h"
#include "server/ThemeManager.h"

#include <QApplication>
#include <QStyleFactory>
#include <QFont>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Шрифт Ubuntu как в HTML (или системный sans-serif при отсутствии)
    QFont appFont("Ubuntu", 10);
    appFont.setStyleHint(QFont::SansSerif);
    app.setFont(appFont);

    app.setStyle(QStyleFactory::create("Fusion"));

    cm::server::MainWindow window;
    window.show();

    return app.exec();
}
