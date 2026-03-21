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
        /* ── Smoother fonts ────────────────────────────────────────── */
        * { font-family: "Segoe UI", "SF Pro Display", "Helvetica Neue", Arial, sans-serif; }

        QToolTip { color: #e0e0e0; background-color: #2a2a2a; border: 1px solid #3a3a3a;
                   padding: 4px 6px; font-size: 11px; }
        QMenuBar { background-color: #2d2d2d; color: #ccc; border: none; }
        QMenuBar::item { padding: 5px 10px; }
        QMenuBar::item:selected { background-color: #094771; color: white; }
        QMenu { background: #252526; border: 1px solid #3a3a3a; padding: 4px 0; }
        QMenu::item { padding: 6px 30px 6px 20px; color: #ccc; }
        QMenu::item:selected { background: #094771; color: white; }
        QMenu::separator { height: 1px; background: #3a3a3a; margin: 4px 10px; }

        /* ── Default toolbar (sketch bar, etc.) ─────────────────────── */
        QToolBar { background-color: #2d2d2d; border: none; spacing: 2px; }
        QToolBar QToolButton { background: transparent; border: 1px solid transparent;
                               padding: 3px 6px; color: #ccc; }
        QToolBar QToolButton:hover { background: #383838; border: 1px solid transparent;
                                     border-radius: 3px; }
        QToolBar QToolButton:pressed { background: #094771; }

        /* ── Ribbon toolbar wrapper (no extra chrome) ───────────────── */
        QToolBar#RibbonToolBar { background: transparent; border: none; padding: 0; margin: 0; }

        /* ── Quick-access bar ───────────────────────────────────────── */
        QWidget#QuickAccessBar { background-color: #2d2d2d; }
        QToolButton#QuickAccessButton {
            background: transparent; border: none; border-radius: 2px;
            font-size: 10px; color: #aaa;
        }
        QToolButton#QuickAccessButton:hover {
            background: #383838; color: #ddd;
        }
        QToolButton#QuickAccessButton:pressed { background: #094771; color: white; }

        /* ── Ribbon tab widget ──────────────────────────────────────── */
        QTabWidget#Ribbon::pane {
            background: #2d2d2d; border: none; border-top: 1px solid #3a3a3a;
        }
        QTabWidget#Ribbon > QTabBar::tab {
            background: #2d2d2d; color: #888; padding: 4px 14px;
            border: none; border-bottom: 2px solid transparent;
            font-weight: 600; font-size: 11px; letter-spacing: 1px;
        }
        QTabWidget#Ribbon > QTabBar::tab:selected {
            background: #2d2d2d; color: #fff;
            border-bottom: 2px solid #0078d4;
        }
        QTabWidget#Ribbon > QTabBar::tab:hover:!selected { color: #bbb; }

        /* ── Ribbon buttons ─────────────────────────────────────────── */
        QToolButton#RibbonButton {
            background: transparent; border: 1px solid transparent;
            border-radius: 4px; color: #aaa; font-size: 9px;
        }
        QToolButton#RibbonButton:hover {
            background: #383838; border: 1px solid transparent;
        }
        QToolButton#RibbonButton:pressed { background: #094771; }

        /* ── Ribbon group label ─────────────────────────────────────── */
        QLabel#RibbonGroupLabel {
            font-size: 10px; color: #777; padding: 0; margin: 0;
        }

        /* ── Ribbon separator ───────────────────────────────────────── */
        QFrame#RibbonSeparator { color: #3a3a3a; }

        /* ── Dock widgets ──────────────────────────────────────────── */
        QDockWidget { color: #ccc; }
        QDockWidget::title {
            background: #2a2a2a; text-align: left; padding: 6px 8px;
            font-size: 11px; font-weight: 600; color: #999;
            border-bottom: 1px solid #3a3a3a;
        }
        QDockWidget::close-button, QDockWidget::float-button { icon-size: 0px; }

        /* ── Status bar (colored accent) ───────────────────────────── */
        QStatusBar { background-color: #007acc; color: white; font-size: 11px; }
        QStatusBar::item { border: none; }
        QStatusBar QLabel { color: white; }

        /* ── Tree widgets ──────────────────────────────────────────── */
        QTreeWidget {
            background: #1e1e1e; color: #d4d4d4; border: none;
            font-size: 12px; outline: none;
        }
        QTreeWidget::item { height: 26px; padding: 2px 4px; border: none; }
        QTreeWidget::item:hover { background: #2a2d2e; }
        QTreeWidget::item:selected { background: #094771; color: white; }
        QTreeWidget::branch { background: #1e1e1e; }

        QHeaderView::section { background-color: #2a2a2a; color: #ccc;
                               border: none; border-bottom: 1px solid #3a3a3a; padding: 4px; }

        /* ── Scrollbars (thin, subtle) ─────────────────────────────── */
        QScrollBar:vertical { background: #1e1e1e; width: 8px; margin: 0; }
        QScrollBar::handle:vertical { background: #555; border-radius: 4px; min-height: 30px; }
        QScrollBar::handle:vertical:hover { background: #666; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal { background: #1e1e1e; height: 8px; margin: 0; }
        QScrollBar::handle:horizontal { background: #555; border-radius: 4px; min-width: 30px; }
        QScrollBar::handle:horizontal:hover { background: #666; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QSplitter::handle { background: #3a3a3a; }

        /* ── Tabs (generic, used by browser tab bar) ───────────────── */
        QTabWidget::pane { border: none; }
        QTabBar::tab {
            background: transparent; color: #888; padding: 8px 14px;
            border: none; font-size: 11px; font-weight: 600;
            border-bottom: 2px solid transparent;
        }
        QTabBar::tab:selected { color: #fff; border-bottom-color: #0078d4; }
        QTabBar::tab:hover:!selected { color: #bbb; }
    )");
}
