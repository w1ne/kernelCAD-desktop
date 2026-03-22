#include "app/Application.h"
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <iostream>
#include <csignal>

static Application* g_app = nullptr;

static void crashHandler(int sig)
{
    const char* sigName = (sig == SIGSEGV) ? "SIGSEGV" :
                          (sig == SIGABRT) ? "SIGABRT" :
                          (sig == SIGFPE)  ? "SIGFPE"  : "UNKNOWN";
    std::cerr << "CRASH: Signal " << sigName << " received." << std::endl;

    try {
        if (g_app) {
            QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            QDir().mkpath(dir);
            std::cerr << "Recovery dir: " << dir.toStdString() << std::endl;
        }
    } catch (...) {}

    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

int main(int argc, char* argv[])
{
    // Suppress harmless Qt internal warnings (QAction connect nullptr)
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext&, const QString& msg) {
        if (type == QtWarningMsg && msg.contains("invalid nullptr parameter"))
            return;
        fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    });

    std::signal(SIGSEGV, crashHandler);
    std::signal(SIGABRT, crashHandler);
    std::signal(SIGFPE,  crashHandler);

    try {
        Application app(argc, argv);
        g_app = &app;
        return app.exec();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Fatal unknown error" << std::endl;
        return 1;
    }
}
