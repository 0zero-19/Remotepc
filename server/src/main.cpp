// =============================================================================
// ClassroomMonitor — Teacher Panel Entry Point
// =============================================================================

#include "server/MainWindow.h"

#include <QApplication>
#include <QStyleFactory>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>

#include <Windows.h>
#include <iostream>

static QMutex g_logMutex;
static bool g_hasConsole = false;

void customLogMessageHandler(QtMsgType type, const QMessageLogContext& /*context*/, const QString& msg) {
    QMutexLocker locker(&g_logMutex);

    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    QString typeStr;

    switch (type) {
        case QtDebugMsg:    typeStr = "DEBUG"; break;
        case QtInfoMsg:     typeStr = "INFO "; break;
        case QtWarningMsg:  typeStr = "WARN "; break;
        case QtCriticalMsg: typeStr = "ERROR"; break;
        case QtFatalMsg:    typeStr = "FATAL"; break;
    }

    QString logLine = QString("[%1] [%2] [Teacher] %3\n").arg(timeStr, typeStr, msg);

    // Запись в файл log_run.txt
    QFile logFile("log_run.txt");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&logFile);
        stream << logLine;
        logFile.close();
    }

    // Если включена консоль — дублируем вывод в неё
    if (g_hasConsole) {
        std::wcout << logLine.toStdWString();
    }
}

int main(int argc, char* argv[]) {
    // Проверяем флаг запуска консоли (--console или наличие файла show_console.txt)
    bool requestConsole = false;
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--console" || arg == "-c" || arg == "--debug") {
            requestConsole = true;
            break;
        }
    }

    if (QFile::exists("show_console.txt")) {
        requestConsole = true;
    }

    if (requestConsole) {
        AllocConsole();
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        g_hasConsole = true;
    }

    // Устанавливаем логгер
    qInstallMessageHandler(customLogMessageHandler);

    qDebug() << "=====================================================";
    qDebug() << "    ClassroomMonitor — Панель Преподавателя";
    qDebug() << "    Сборка:" << __DATE__ << __TIME__;
    qDebug() << "=====================================================";

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
