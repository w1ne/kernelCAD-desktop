#pragma once
#include <QApplication>
#include "ui/MainWindow.h"

class Application : public QApplication
{
    Q_OBJECT
public:
    Application(int& argc, char* argv[]);

private:
    MainWindow* m_mainWindow = nullptr;
};
