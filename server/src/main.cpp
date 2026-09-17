// =============================================================================
// ClassroomMonitor — Teacher Panel Entry Point
// =============================================================================

#include "server/MainWindow.h"
#include "server/ThemeManager.h"

#include <QApplication>
#include <QStyleFactory>
#include <QFont>
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

    // Write to log_run.txt
    QFile logFile("log_run.txt");
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&logFile);
        stream << logLine;
        logFile.close();
    }

    // Duplicate to console if enabled
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

    // Шрифт Ubuntu как в HTML (или системный sans-serif при отсутствии)
    QFont appFont("Ubuntu", 10);
    appFont.setStyleHint(QFont::SansSerif);
    app.setFont(appFont);

    app.setStyle(QStyleFactory::create("Fusion"));

    cm::server::MainWindow window;
    window.show();

    return app.exec();
}
