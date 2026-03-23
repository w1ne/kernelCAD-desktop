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
        QMenuBar { background-color: #2d2d2d; color: #e0e0e0; border: none; }
        QMenuBar::item { padding: 5px 10px; color: #e0e0e0; background: transparent; }
        QMenuBar::item:selected { background-color: #094771; color: white; }
        QMenuBar::item:!selected { color: #e0e0e0; }
        QMenu { background-color: #252526; border: 1px solid #3a3a3a; padding: 4px 0; }
        QMenu::item { padding: 6px 30px 6px 20px; color: #e0e0e0; background: transparent; }
        QMenu::item:selected { background-color: #094771; color: white; }
        QMenu::item:disabled { color: #666666; }
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

        /* ── Ribbon tab widget ──────────────────────────────────────── */
        QTabWidget#Ribbon::pane {
            background: #333333; border: none;
            border-top: 1px solid #3a3a3a;
            border-bottom: 1px solid #3a3a3a;
        }
        QTabWidget#Ribbon > QTabBar::tab {
            background: transparent; color: #888; padding: 6px 16px;
            border: none; border-bottom: 2px solid transparent;
            font-weight: 600; font-size: 11px; letter-spacing: 1px;
        }
        QTabWidget#Ribbon > QTabBar::tab:selected {
            color: #fff; border-bottom: 2px solid #0078d4;
        }
        QTabWidget#Ribbon > QTabBar::tab:hover:!selected { color: #bbb; }

        /* ── Ribbon buttons ─────────────────────────────────────────── */
        QToolButton#RibbonButton {
            background: transparent; border: none;
            border-bottom: 2px solid transparent;
            border-radius: 0; padding: 4px;
        }
        QToolButton#RibbonButton:hover {
            background: rgba(255, 255, 255, 0.06);
            border-bottom: 2px solid transparent;
        }
        QToolButton#RibbonButton:checked {
            background: rgba(255, 255, 255, 0.08);
            border-bottom: 2px solid #0078d4;
        }
        QToolButton#RibbonButton:pressed {
            background: rgba(255, 255, 255, 0.12);
        }

        /* ── Ribbon group label ─────────────────────────────────────── */
        QPushButton#RibbonGroupLabel {
            font-size: 10px; font-weight: 500; color: #777;
            letter-spacing: 0.5px; padding: 2px 4px 4px 4px;
            border: none; background: transparent;
            min-height: 14px; max-height: 16px;
        }
        QPushButton#RibbonGroupLabel:hover { color: #aaa; background: transparent; border: none; }

        /* ── Ribbon separator ───────────────────────────────────────── */
        QFrame#RibbonSeparator { background: #3a3a3a; border: none; }

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

        /* ── Dialogs & Forms ─────────────────────────────────────── */
        QDialog { background: #2d2d2d; color: #e0e0e0; }
        QDialog QLabel { color: #e0e0e0; }

        /* ── Input widgets (global) ──────────────────────────────── */
        QDoubleSpinBox, QSpinBox {
            background: #3c3f41; color: #e0e0e0; border: 1px solid #555;
            border-radius: 3px; padding: 3px 6px; min-height: 22px;
        }
        QDoubleSpinBox:focus, QSpinBox:focus { border-color: #0078d4; }

        QLineEdit {
            background: #3c3f41; color: #e0e0e0; border: 1px solid #555;
            border-radius: 3px; padding: 3px 6px; min-height: 22px;
        }
        QLineEdit:focus { border-color: #0078d4; }

        QComboBox {
            background: #3c3f41; color: #e0e0e0; border: 1px solid #555;
            border-radius: 3px; padding: 3px 6px; min-height: 22px;
        }
        QComboBox QAbstractItemView {
            background: #3c3f41; color: #e0e0e0;
            selection-background-color: #094771;
        }

        QCheckBox { color: #e0e0e0; }
        QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid #666;
                               border-radius: 3px; background: #3c3f41; }
        QCheckBox::indicator:checked { background: #0078d4; border-color: #0078d4; }

        QPushButton {
            background: #3c3f41; color: #e0e0e0; border: 1px solid #555;
            border-radius: 4px; padding: 5px 16px;
        }
        QPushButton:hover { background: #4a4a4a; }
        QPushButton:pressed { background: #094771; }

        QTableWidget {
            background: #1e1e1e; color: #d4d4d4; border: none;
            gridline-color: #333;
        }
        QTableWidget::item:selected { background: #094771; color: white; }

        QSlider::groove:horizontal { background: #3c3f41; height: 4px; border-radius: 2px; }
        QSlider::handle:horizontal { background: #0078d4; width: 14px; height: 14px;
                                     margin: -5px 0; border-radius: 7px; }
    )");
}
