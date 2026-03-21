#include "Application.h"

Application::Application(int& argc, char* argv[])
    : QApplication(argc, argv)
{
    setApplicationName("kernelCAD");
    setApplicationVersion("0.1.0");
    setOrganizationName("kernelCAD");

    m_mainWindow = new MainWindow();
    m_mainWindow->show();
}
