#include "Application.h"
#include <QPalette>
#include <QStyleFactory>

Application::Application(int& argc, char* argv[])
    : QApplication(argc, argv)
{
    setApplicationName("kernelCAD");
    setApplicationVersion("0.1.0");
    setOrganizationName("kernelCAD");

    setupDarkTheme();

    m_mainWindow = new MainWindow();
    m_mainWindow->show();
}

void Application::setupDarkTheme()
{
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(45, 45, 45));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(35, 35, 35));
    darkPalette.setColor(QPalette::AlternateBase, QColor(50, 50, 50));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(60, 60, 60));
    darkPalette.setColor(QPalette::ToolTipText, Qt::white);
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(55, 55, 55));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(82, 148, 226));
    darkPalette.setColor(QPalette::Highlight, QColor(82, 148, 226));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);

    // Disabled colors
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));

    setPalette(darkPalette);
    setStyle(QStyleFactory::create("Fusion"));

    // Global stylesheet for consistent dark theme across all widgets
    setStyleSheet(R"(
        QToolTip { color: #e0e0e0; background-color: #3c3f41; border: 1px solid #555; }
        QMenuBar { background-color: #2d2d2d; color: #e0e0e0; }
        QMenuBar::item:selected { background-color: #5294e2; }
        QMenu { background-color: #2d2d2d; color: #e0e0e0; border: 1px solid #555; }
        QMenu::item:selected { background-color: #5294e2; }
        QMenu::separator { height: 1px; background: #555; margin: 4px 8px; }
        QToolBar { background-color: #2d2d2d; border: none; spacing: 2px; }
        QToolBar QToolButton { background: transparent; border: 1px solid transparent;
                               padding: 3px 6px; color: #e0e0e0; }
        QToolBar QToolButton:hover { background: #3c3f41; border: 1px solid #666;
                                     border-radius: 3px; }
        QToolBar QToolButton:pressed { background: #5294e2; }
        QDockWidget { color: #e0e0e0; }
        QDockWidget::title { background-color: #2d2d2d; padding: 4px; }
        QStatusBar { background-color: #2d2d2d; color: #aaa; }
        QTreeWidget { background-color: #252525; color: #e0e0e0; border: none; }
        QTreeWidget::item:selected { background-color: #5294e2; }
        QTreeWidget::item:hover { background-color: #383838; }
        QHeaderView::section { background-color: #2d2d2d; color: #e0e0e0;
                               border: 1px solid #555; padding: 3px; }
        QScrollBar:vertical { background: #2d2d2d; width: 10px; }
        QScrollBar::handle:vertical { background: #555; border-radius: 4px; min-height: 20px; }
        QScrollBar::handle:vertical:hover { background: #777; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal { background: #2d2d2d; height: 10px; }
        QScrollBar::handle:horizontal { background: #555; border-radius: 4px; min-width: 20px; }
        QScrollBar::handle:horizontal:hover { background: #777; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QSplitter::handle { background: #555; }
        QTabWidget::pane { border: 1px solid #555; }
        QTabBar::tab { background: #2d2d2d; color: #e0e0e0; padding: 6px 12px;
                       border: 1px solid #555; }
        QTabBar::tab:selected { background: #3c3f41; border-bottom: 2px solid #5294e2; }
        QTabBar::tab:hover { background: #383838; }
    )");
}
