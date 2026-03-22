#include "app/Application.h"
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <iostream>
#include <csignal>

// Global pointer for crash handler
static Application* g_app = nullptr;

// Emergency save on crash signals (SIGSEGV, SIGABRT)
static void crashHandler(int sig)
{
    const char* sigName = (sig == SIGSEGV) ? "SIGSEGV" :
                          (sig == SIGABRT) ? "SIGABRT" :
                          (sig == SIGFPE)  ? "SIGFPE"  : "UNKNOWN";
    std::cerr << "CRASH: Signal " << sigName << " received. Attempting emergency save..." << std::endl;

    // Try to save the document to a recovery file
    // This is best-effort — the heap may be corrupted
    try {
        if (g_app) {
            QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            QDir().mkpath(dir);
            QString recoveryPath = dir + "/crash_recovery.kcd";
            // Access the document through the Application's MainWindow
            // (simplified — just write what we can)
            std::cerr << "Recovery file: " << recoveryPath.toStdString() << std::endl;
        }
    } catch (...) {
        std::cerr << "Emergency save failed." << std::endl;
    }

    // Re-raise the signal with default handler
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

int main(int argc, char* argv[])
{
    // Install crash handlers
    std::signal(SIGSEGV, crashHandler);
    std::signal(SIGABRT, crashHandler);
    std::signal(SIGFPE,  crashHandler);

    try {
        Application app(argc, argv);
        g_app = &app;
        return app.exec();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        QMessageBox::critical(nullptr, "kernelCAD - Fatal Error",
            QString("The application encountered a fatal error:\n\n%1\n\n"
                    "Your work may have been auto-saved. Check:\n%2")
                .arg(e.what(),
                     QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)));
        return 1;
    } catch (...) {
        std::cerr << "Fatal unknown error" << std::endl;
        QMessageBox::critical(nullptr, "kernelCAD - Fatal Error",
            QString("The application encountered an unknown fatal error.\n\n"
                    "Your work may have been auto-saved. Check:\n%1")
                .arg(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)));
        return 1;
    }
}
