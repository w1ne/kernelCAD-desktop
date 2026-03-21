#include "MainWindow.h"
#include "Viewport3D.h"
#include "FeatureTree.h"
#include "TimelinePanel.h"
#include "PropertiesPanel.h"
#include "../document/Document.h"

#include <QDockWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QCloseEvent>
#include <QMessageBox>
#include <QStatusBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_document(std::make_unique<document::Document>())
{
    setWindowTitle("kernelCAD");
    resize(1440, 900);
    setupUI();
    setupMenuBar();
    setupDocks();
    connectSignals();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI()
{
    // Central widget: 3D viewport
    m_viewport = new Viewport3D(this);
    setCentralWidget(m_viewport);
    statusBar()->showMessage("Ready");
}

void MainWindow::setupMenuBar()
{
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&New"),  this, &MainWindow::onNewDocument,  QKeySequence::New);
    fileMenu->addAction(tr("&Open"), this, &MainWindow::onOpenDocument, QKeySequence::Open);
    fileMenu->addAction(tr("&Save"), this, &MainWindow::onSaveDocument, QKeySequence::Save);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Export STEP..."), this, &MainWindow::onExportSTEP);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), qApp, &QApplication::quit, QKeySequence::Quit);

    auto* editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(tr("Undo"), [](){}, QKeySequence::Undo);
    editMenu->addAction(tr("Redo"), [](){}, QKeySequence::Redo);

    menuBar()->addMenu(tr("&View"));
    menuBar()->addMenu(tr("&Sketch"));
    menuBar()->addMenu(tr("&Model"));
    menuBar()->addMenu(tr("&Assembly"));
}

void MainWindow::setupDocks()
{
    // Feature tree — left
    m_featureTree = new FeatureTree(this);
    auto* leftDock = new QDockWidget(tr("Browser"), this);
    leftDock->setWidget(m_featureTree);
    leftDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, leftDock);

    // Properties — right
    m_properties = new PropertiesPanel(this);
    auto* rightDock = new QDockWidget(tr("Properties"), this);
    rightDock->setWidget(m_properties);
    addDockWidget(Qt::RightDockWidgetArea, rightDock);

    // Timeline — bottom
    m_timeline = new TimelinePanel(this);
    auto* bottomDock = new QDockWidget(tr("Timeline"), this);
    bottomDock->setWidget(m_timeline);
    bottomDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, bottomDock);
}

void MainWindow::connectSignals()
{
    // TODO: connect viewport selection → properties panel
    // TODO: connect timeline marker change → recompute
}

void MainWindow::onNewDocument()
{
    m_document->newDocument();
    setWindowTitle("kernelCAD — Untitled");
}

void MainWindow::onOpenDocument()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Open"), {}, tr("kernelCAD Files (*.kcd)"));
    if (!path.isEmpty()) {
        m_document->load(path.toStdString());
        setWindowTitle("kernelCAD — " + QFileInfo(path).baseName());
    }
}

void MainWindow::onSaveDocument()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Save"), {}, tr("kernelCAD Files (*.kcd)"));
    if (!path.isEmpty())
        m_document->save(path.toStdString());
}

void MainWindow::onExportSTEP()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Export STEP"), {}, tr("STEP Files (*.step *.stp)"));
    if (!path.isEmpty())
        statusBar()->showMessage(tr("Exported: %1").arg(path));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_document->isModified()) {
        auto ret = QMessageBox::question(this, tr("Unsaved changes"),
            tr("Save before closing?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (ret == QMessageBox::Cancel) { event->ignore(); return; }
        if (ret == QMessageBox::Save) onSaveDocument();
    }
    event->accept();
}
