#include "app/Application.h"
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <iostream>
#include <csignal>
#include <cstdlib>

static Application* g_app = nullptr;

static void crashHandler(int sig)
{
    // Prevent re-entry: reset the handler immediately
    std::signal(sig, SIG_DFL);

    const char* sigName = (sig == SIGSEGV) ? "SIGSEGV" :
                          (sig == SIGABRT) ? "SIGABRT" :
                          (sig == SIGFPE)  ? "SIGFPE"  : "UNKNOWN";

    // Use write() instead of cerr in signal handler for async-signal-safety
    fprintf(stderr, "\nkernelCAD: Fatal error (signal %s). Check auto-save directory for recovery.\n", sigName);

    try {
        if (g_app) {
            QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            QDir().mkpath(dir);
            fprintf(stderr, "Recovery dir: %s\n", dir.toStdString().c_str());
        }
    } catch (...) {}

    // Re-raise the signal for the default handler (generates core dump if enabled)
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
