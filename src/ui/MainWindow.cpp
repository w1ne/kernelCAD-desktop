#include "MainWindow.h"
#include "DrawingView.h"
#include "IconFactory.h"
#include "Viewport3D.h"
#include "ViewportManipulator.h"
#include "SketchEditor.h"
#include "FeatureTree.h"
#include "TimelinePanel.h"
#include "PropertiesPanel.h"
#include "SelectionManager.h"
#include "MeasureTool.h"
#include "JointCreator.h"
#include "CommandDialog.h"
#include "MarkingMenu.h"
#include "CommandPalette.h"
#include "FeatureDialog.h"
#include "ParameterTablePanel.h"
#include "PreferencesDialog.h"
#include "../document/Document.h"
#include "../document/InteractiveCommands.h"
#include "../document/AutoSave.h"
#include "../document/Timeline.h"
#include "../document/Commands.h"
#include "../document/Component.h"
#include "../document/PreviewEngine.h"
#include "../features/ExtrudeFeature.h"
#include "../features/RevolveFeature.h"
#include "../features/SketchFeature.h"
#include "../features/ShellFeature.h"
#include "../features/FilletFeature.h"
#include "../features/ChamferFeature.h"
#include "../features/DraftFeature.h"
#include "../features/MirrorFeature.h"
#include "../features/CircularPatternFeature.h"
#include "../features/RectangularPatternFeature.h"
#include "../features/HoleFeature.h"
#include "../features/SweepFeature.h"
#include "../features/LoftFeature.h"
#include "../features/OffsetFacesFeature.h"
#include "../features/ConstructionPlane.h"
#include "../features/Joint.h"
#include "../sketch/DxfImporter.h"
#include "../sketch/SvgImporter.h"
#include "../kernel/BRepModel.h"
#include "../kernel/BRepQuery.h"
#include "../kernel/StableReference.h"
#include <TopoDS_Shape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Compound.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <BRep_Tool.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Pnt.hxx>
#include <Poly_Triangulation.hxx>
#include <TopLoc_Location.hxx>
#include <sstream>
#include <algorithm>

#include <QDockWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFileInfo>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>
#include <QSlider>
#include <QApplication>
#include <QPixmap>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QTimer>
#include <QShortcut>
#include <QWidgetAction>
#include <QTabWidget>
#include <QToolButton>
#include <QButtonGroup>
#include <QInputDialog>
#include <QVBoxLayout>
#include <QTabBar>
#include <QFrame>
#include <QSettings>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_document(std::make_unique<document::Document>())
    , m_selectionMgr(std::make_unique<SelectionManager>())
{
    setWindowTitle("kernelCAD");
    resize(1440, 900);
    setupUI();
    setupMenuBar();
    setupToolBar();
    installToolBarHoverFilters();
    setupDocks();
    setupSketchToolBar();
    m_featureTree->setDocument(m_document.get());
    m_properties->setDocument(m_document.get());
    m_parameterTable->setDocument(m_document.get());

    // Parameter changes -> recompute + refresh
    connect(m_parameterTable, &ParameterTablePanel::parameterChanged, this, [this]() {
        m_document->recompute();
        refreshAllPanels();
        m_parameterTable->refresh();
    });

    // Wire up selection manager to viewport and callbacks
    m_viewport->setSelectionManager(m_selectionMgr.get());
    m_viewport->setBRepModel(&m_document->brepModel());

    // Create viewport manipulator (drag handles for extrude distance, etc.)
    m_manipulator = new ViewportManipulator(this);
    m_viewport->setManipulator(m_manipulator);

    // Viewport right-click context menu and marking menu
    m_viewport->setContextMenuPolicy(Qt::PreventContextMenu);
    setupMarkingMenu();
    m_selectionMgr->setOnSelectionChanged([this](const std::vector<SelectionHit>&) {
        onSelectionChanged();
    });

    // Create sketch editor
    m_sketchEditor = new SketchEditor(this);
    connect(m_sketchEditor, &SketchEditor::editingFinished,
            this, &MainWindow::onSketchEditingFinished);
    connect(m_sketchEditor, &SketchEditor::sketchChanged, this, [this]() {
        m_viewport->update();
        m_properties->refreshSketchStats();
    });
    connect(m_sketchEditor, &SketchEditor::constraintSelected, this,
        [this](const QString& /*id*/, const QString& typeName, const QString& desc) {
        if (typeName.isEmpty()) {
            // Deselected — restore default sketch status
            statusBar()->showMessage(
                tr("Sketch Mode \u2014 L:Line  R:Rect  C:Circle  D:Dim  K:Constraint  Esc:Finish"));
        } else {
            statusBar()->showMessage(desc);
        }
    });
    connect(m_sketchEditor, &SketchEditor::toolChanged, this, [this](SketchTool tool) {
        QString toolName;
        switch (tool) {
        case SketchTool::None:                toolName = "Select"; break;
        case SketchTool::DrawLine:            toolName = "Line"; break;
        case SketchTool::DrawRectangle:       toolName = "Rectangle"; break;
        case SketchTool::DrawCircle:          toolName = "Circle"; break;
        case SketchTool::DrawArc:             toolName = "Arc"; break;
        case SketchTool::DrawSpline:          toolName = "Spline"; break;
        case SketchTool::DrawEllipse:         toolName = "Ellipse"; break;
        case SketchTool::DrawPolygon:         toolName = "Polygon"; break;
        case SketchTool::DrawSlot:            toolName = "Slot"; break;
        case SketchTool::DrawCircle3Point:    toolName = "3-Point Circle"; break;
        case SketchTool::DrawArc3Point:       toolName = "3-Point Arc"; break;
        case SketchTool::DrawRectangleCenter: toolName = "Center Rect"; break;
        case SketchTool::Trim:                toolName = "Trim"; break;
        case SketchTool::Extend:              toolName = "Extend"; break;
        case SketchTool::Offset:              toolName = "Offset"; break;
        case SketchTool::ProjectEdge:         toolName = "Project Edge"; break;
        case SketchTool::SketchFillet:        toolName = "Fillet"; break;
        case SketchTool::SketchChamfer:       toolName = "Chamfer"; break;
        case SketchTool::Dimension:           toolName = "Dimension"; break;
        case SketchTool::AddConstraint:       toolName = "Constraint"; break;
        case SketchTool::ConstrainCoincident:  toolName = "Coincident"; break;
        case SketchTool::ConstrainParallel:    toolName = "Parallel"; break;
        case SketchTool::ConstrainPerpendicular: toolName = "Perpendicular"; break;
        case SketchTool::ConstrainTangent:     toolName = "Tangent"; break;
        case SketchTool::ConstrainEqual:       toolName = "Equal"; break;
        case SketchTool::ConstrainSymmetric:   toolName = "Symmetric"; break;
        case SketchTool::ConstrainHorizontal:  toolName = "Horizontal"; break;
        case SketchTool::ConstrainVertical:    toolName = "Vertical"; break;
        case SketchTool::ConstrainConcentric:  toolName = "Concentric"; break;
        default:                              toolName = "Select"; break;
        }
        statusBar()->showMessage(tr("Sketch tool: %1").arg(toolName));

        // Sync sketch toolbar via QButtonGroup
        if (m_sketchToolBar) {
            int toolInt = static_cast<int>(tool);
            auto* group = m_sketchToolBar->findChild<QButtonGroup*>();
            if (group) {
                QAbstractButton* btn = group->button(toolInt);
                if (btn) {
                    btn->blockSignals(true);
                    btn->setChecked(true);
                    btn->blockSignals(false);
                }
            }
        }
    });

    // Status bar context hints from sketch editor
    connect(m_sketchEditor, &SketchEditor::statusHint, this,
            [this](const QString& hint) { statusBar()->showMessage(hint); });

    // Create the measure tool
    m_measureTool = new MeasureTool(this);
    m_measureTool->setBRepModel(&m_document->brepModel());
    connect(m_measureTool, &MeasureTool::measurementReady, this,
            [this](const MeasureTool::MeasureResult& result) {
        statusBar()->showMessage(result.description);
    });

    // Create the preview engine and wire it to the viewport
    m_previewEngine = std::make_unique<document::PreviewEngine>(*m_document);
    m_previewEngine->setMeshCallback([this](const std::vector<float>& verts,
                                             const std::vector<float>& normals,
                                             const std::vector<uint32_t>& indices) {
        m_viewport->setPreviewMesh(verts, normals, indices);
    });
    m_previewEngine->setClearCallback([this]() {
        m_viewport->clearPreviewMesh();
    });

    // Create the joint creator for interactive face-to-face joint workflow
    m_jointCreator = new JointCreator(this);
    m_jointCreator->setBRepModel(&m_document->brepModel());
    connect(m_jointCreator, &JointCreator::jointReady, this,
            [this](features::JointParams params) {
        // Resolve body IDs to occurrence IDs
        std::string occ1 = m_document->findOccurrenceForBody(params.occurrenceOneId);
        std::string occ2 = m_document->findOccurrenceForBody(params.occurrenceTwoId);
        if (!occ1.empty()) params.occurrenceOneId = occ1;
        if (!occ2.empty()) params.occurrenceTwoId = occ2;

        try {
            m_document->executeCommand(
                std::make_unique<document::AddJointCommand>(std::move(params)));
            statusBar()->showMessage(tr("Joint created from face selections"));
        } catch (const std::exception& e) {
            QMessageBox::warning(this, tr("Joint Failed"),
                tr("Could not create joint: %1").arg(e.what()));
        }
        refreshAllPanels();
        hideConfirmBar();
    });
    connect(m_jointCreator, &JointCreator::stateChanged, this,
            [this](JointCreator::State state) {
        switch (state) {
        case JointCreator::State::Idle:
            hideConfirmBar();
            break;
        case JointCreator::State::PickingFace1:
            statusBar()->showMessage(tr("Select first face for joint..."));
            showConfirmBar(tr("Joint: Pick Face 1"));
            break;
        case JointCreator::State::PickingFace2:
            statusBar()->showMessage(tr("Select second face for joint..."));
            showConfirmBar(tr("Joint: Pick Face 2"));
            break;
        }
    });
    connect(m_jointCreator, &JointCreator::cancelled, this, [this]() {
        statusBar()->showMessage(tr("Joint creation cancelled"));
        hideConfirmBar();
    });

    // Auto-save (5 minute interval)
    m_autoSave = new document::AutoSave(m_document.get(), this);

    // Command palette (Ctrl+K)
    setupCommandPalette();

    connectSignals();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI()
{
    // Central widget: 3D viewport
    m_viewport = new Viewport3D(this);
    setCentralWidget(m_viewport);

    setupStatusBar();
    setupConfirmBar();

    // Create floating feature dialog (parented to viewport so it floats over it)
    m_featureDialog = new FeatureDialog(m_viewport);
}

// ─── Ribbon helpers ─────────────────────────────────────────────────────────

void MainWindow::addToolGroup(QHBoxLayout* parentLayout, const QString& groupName,
                               const std::vector<ToolEntry>& tools,
                               const std::vector<ToolEntry>& dropdownExtras)
{
    auto* groupWidget = new QWidget;
    groupWidget->setObjectName("RibbonGroup");
    auto* groupLayout = new QVBoxLayout(groupWidget);
    groupLayout->setContentsMargins(4, 0, 4, 0);
    groupLayout->setSpacing(0);

    // Button row
    auto* buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(2);
    for (const auto& tool : tools) {
        auto* btn = new QToolButton;
        btn->setIcon(tool.icon);
        btn->setIconSize(QSize(28, 28));
        btn->setText(tool.name);
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setToolTip(tool.tooltip);
        btn->setAutoRaise(true);
        btn->setFixedSize(48, 52);
        btn->setObjectName("RibbonButton");
        QFont btnFont = btn->font();
        btnFont.setPointSize(9);
        btn->setFont(btnFont);
        if (tool.action)
            connect(btn, &QToolButton::clicked, this, tool.action);
        buttonRow->addWidget(btn);

        // Tag with tool name for hover-filter lookup
        btn->setProperty("_toolName", tool.name);
    }
    groupLayout->addLayout(buttonRow);

    // Group label as clickable dropdown button (shows all commands in this group)
    auto* groupLabel = new QPushButton(groupName + QString::fromUtf8(" \xe2\x96\xbe"));
    groupLabel->setFlat(true);
    groupLabel->setStyleSheet("color: #777; font-size: 10px; border: none; padding: 0 2px;");
    groupLabel->setCursor(Qt::PointingHandCursor);
    // Capture copies of tools and extras for the dropdown lambda
    auto toolsCopy = tools;
    auto extrasCopy = dropdownExtras;
    connect(groupLabel, &QPushButton::clicked, this, [toolsCopy, extrasCopy, groupLabel]() {
        QMenu menu;
        for (const auto& t : toolsCopy) {
            if (t.action)
                menu.addAction(t.name, t.action);
        }
        if (!extrasCopy.empty()) {
            menu.addSeparator();
            for (const auto& t : extrasCopy) {
                if (t.action)
                    menu.addAction(t.name, t.action);
            }
        }
        menu.exec(groupLabel->mapToGlobal(QPoint(0, -menu.sizeHint().height())));
    });
    groupLayout->addWidget(groupLabel, 0, Qt::AlignCenter);

    parentLayout->addWidget(groupWidget);
}

void MainWindow::addGroupSeparator(QHBoxLayout* layout)
{
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::VLine);
    sep->setObjectName("RibbonSeparator");
    sep->setFixedWidth(1);
    sep->setFixedHeight(58);
    layout->addWidget(sep);
}

// ─── Main ribbon setup ─────────────────────────────────────────────────────

void MainWindow::setupToolBar()
{
    // ── Overall container: quick-access bar on top, ribbon tabs below ────
    m_ribbonContainer = new QWidget(this);
    m_ribbonContainer->setObjectName("RibbonContainer");
    auto* containerLayout = new QVBoxLayout(m_ribbonContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    // ── Quick-access toolbar (New, Open, Save, Undo, Redo) ──────────────
    m_quickAccessBar = new QWidget;
    m_quickAccessBar->setObjectName("QuickAccessBar");
    m_quickAccessBar->setFixedHeight(26);
    auto* qaLayout = new QHBoxLayout(m_quickAccessBar);
    qaLayout->setContentsMargins(8, 2, 8, 2);
    qaLayout->setSpacing(4);

    auto makeQAButton = [this](const QString& iconName, const QString& text,
                                const QString& tip, auto slot) -> QToolButton* {
        auto* btn = new QToolButton;
        btn->setIcon(IconFactory::createIcon(iconName, 16));
        btn->setIconSize(QSize(16, 16));
        btn->setText(text);
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        btn->setToolTip(tip);
        btn->setAutoRaise(true);
        btn->setMinimumSize(22, 22);
        btn->setObjectName("QuickAccessButton");
        connect(btn, &QToolButton::clicked, this, slot);
        return btn;
    };

    qaLayout->addWidget(makeQAButton("new",  tr("New"),  tr("New (Ctrl+N)"),  &MainWindow::onNewDocument));
    qaLayout->addWidget(makeQAButton("open", tr("Open"), tr("Open (Ctrl+O)"), &MainWindow::onOpenDocument));
    qaLayout->addWidget(makeQAButton("save", tr("Save"), tr("Save (Ctrl+S)"), &MainWindow::onSaveDocument));

    // Small separator
    auto* qaSep = new QFrame;
    qaSep->setFrameShape(QFrame::VLine);
    qaSep->setObjectName("RibbonSeparator");
    qaSep->setFixedSize(1, 16);
    qaLayout->addWidget(qaSep);

    qaLayout->addWidget(makeQAButton("undo", tr("Undo"), tr("Undo (Ctrl+Z)"), &MainWindow::onUndo));
    qaLayout->addWidget(makeQAButton("redo", tr("Redo"), tr("Redo (Ctrl+Shift+Z)"), &MainWindow::onRedo));

    qaLayout->addStretch();
    containerLayout->addWidget(m_quickAccessBar);

    // ── Ribbon tab widget ───────────────────────────────────────────────
    m_ribbon = new QTabWidget;
    m_ribbon->setObjectName("Ribbon");
    m_ribbon->setTabPosition(QTabWidget::North);
    m_ribbon->setFixedHeight(88);

    // ════════════════════════════════════════════════════════════════════
    // Tab 1: SOLID
    // ════════════════════════════════════════════════════════════════════
    {
        auto* tab = new QWidget;
        auto* layout = new QHBoxLayout(tab);
        layout->setContentsMargins(6, 2, 6, 2);
        layout->setSpacing(2);

        // Group: Create (sketch + primitives)
        addToolGroup(layout, "Create", {
            {"Sketch",   IconFactory::createIcon("sketch"),   tr("Sketch (S) \u2014 Create a 2D sketch"),
             [this]() { onCreateSketch(); }},
            {"Box",      IconFactory::createIcon("box"),      tr("Box \u2014 Create a parametric box"),
             [this]() { onCreateBox(); }},
            {"Cylinder", IconFactory::createIcon("cylinder"), tr("Cylinder \u2014 Create a parametric cylinder"),
             [this]() { onCreateCylinder(); }},
            {"Sphere",   IconFactory::createIcon("sphere"),   tr("Sphere \u2014 Create a parametric sphere"),
             [this]() { onCreateSphere(); }},
        }, {
            {"Torus",  {}, tr("Torus"),  [this]() { statusBar()->showMessage(tr("Torus — not yet implemented")); }},
            {"Coil",   {}, tr("Coil"),   [this]() { statusBar()->showMessage(tr("Coil — not yet implemented")); }},
            {"Pipe",   {}, tr("Pipe"),   [this]() { statusBar()->showMessage(tr("Pipe — not yet implemented")); }},
        });
        addGroupSeparator(layout);

        // Group: Form (extrude / revolve / sweep / loft)
        addToolGroup(layout, "Form", {
            {"Extrude", IconFactory::createIcon("extrude"), tr("Extrude (E) \u2014 Extrude a sketch or face"),
             [this]() { onExtrudeSketch(); }},
            {"Revolve", IconFactory::createIcon("revolve"), tr("Revolve \u2014 Revolve around an axis"),
             [this]() { onRevolveSketch(); }},
            {"Sweep",   IconFactory::createIcon("sweep"),   tr("Sweep \u2014 Sweep a profile along a path"),
             [this]() { onSweepSketch(); }},
            {"Loft",    IconFactory::createIcon("loft"),    tr("Loft \u2014 Solid between profiles"),
             [this]() { onLoftTest(); }},
        }, {
            {"Extrude from Face",    {}, tr("Extrude from Face"),    [this]() { onExtrudeSketch(); }},
            {"Revolve from Sketch",  {}, tr("Revolve from Sketch"),  [this]() { onRevolveSketch(); }},
        });
        addGroupSeparator(layout);

        // Group: Modify
        addToolGroup(layout, "Modify", {
            {"Fillet",  IconFactory::createIcon("fillet"),  tr("Fillet (F) \u2014 Round selected edges"),
             [this]() { onFillet(); }},
            {"Chamfer", IconFactory::createIcon("chamfer"), tr("Chamfer \u2014 Bevel selected edges"),
             [this]() { onChamfer(); }},
            {"Shell",   IconFactory::createIcon("shell"),   tr("Shell \u2014 Hollow a body"),
             [this]() { onShell(); }},
            {"Draft",   IconFactory::createIcon("draft"),   tr("Draft (D) \u2014 Apply draft angle to faces"),
             [this]() { onDraft(); }},
            {"Hole",    IconFactory::createIcon("hole"),    tr("Hole (H) \u2014 Create a hole on a face"),
             [this]() { onAddHole(); }},
        }, {
            {"Press/Pull",    {}, tr("Press/Pull (Q) — Push/pull faces"),    [this]() { onPressPull(); }},
            {"Scale",         {}, tr("Scale"),         [this]() { statusBar()->showMessage(tr("Scale — not yet implemented")); }},
            {"Combine",       {}, tr("Combine"),       [this]() { statusBar()->showMessage(tr("Combine — not yet implemented")); }},
            {"Replace Face",  {}, tr("Replace Face"),  [this]() { statusBar()->showMessage(tr("Replace Face — not yet implemented")); }},
            {"Split Face",    {}, tr("Split Face"),    [this]() { statusBar()->showMessage(tr("Split Face — not yet implemented")); }},
            {"Split Body",    {}, tr("Split Body"),    [this]() { statusBar()->showMessage(tr("Split Body — not yet implemented")); }},
            {"Offset Faces",  {}, tr("Offset Faces"),  [this]() { statusBar()->showMessage(tr("Offset Faces — not yet implemented")); }},
            {"Delete Face",   {}, tr("Delete Face"),   [this]() { statusBar()->showMessage(tr("Delete Face — not yet implemented")); }},
            {"Thread",        {}, tr("Thread"),        [this]() { statusBar()->showMessage(tr("Thread — not yet implemented")); }},
            {"Thicken",       {}, tr("Thicken"),       [this]() { statusBar()->showMessage(tr("Thicken — not yet implemented")); }},
            {"Move/Copy",     {}, tr("Move/Copy"),     [this]() { statusBar()->showMessage(tr("Move/Copy — not yet implemented")); }},
            {"Appearance",    {}, tr("Appearance"),    [this]() { statusBar()->showMessage(tr("Appearance — not yet implemented")); }},
        });
        addGroupSeparator(layout);

        // Group: Pattern
        addToolGroup(layout, "Pattern", {
            {"Mirror",       IconFactory::createIcon("mirror"),       tr("Mirror \u2014 Mirror across a plane"),
             [this]() { onMirrorLastBody(); }},
            {"Rect Pattern", IconFactory::createIcon("rect_pattern"), tr("Rect Pattern \u2014 Rectangular array"),
             [this]() { onRectangularPattern(); }},
            {"Circ Pattern", IconFactory::createIcon("circ_pattern"), tr("Circ Pattern \u2014 Circular array"),
             [this]() { onCircularPattern(); }},
        }, {
            {"Path Pattern", {}, tr("Path Pattern"), [this]() { statusBar()->showMessage(tr("Path Pattern — not yet implemented")); }},
        });
        addGroupSeparator(layout);

        // Group: Construct (construction geometry)
        addToolGroup(layout, "Construct", {
            {"Plane", IconFactory::createIcon("plane"), tr("Construct Plane \u2014 Create a construction plane"),
             [this]() { onConstructPlane(); }},
            {"Axis",  IconFactory::createIcon("axis"),  tr("Construct Axis \u2014 Create a construction axis"),
             [this]() { onConstructAxis(); }},
            {"Point", IconFactory::createIcon("point"), tr("Construct Point \u2014 Create a construction point"),
             [this]() { onConstructPoint(); }},
        }, {
            {"Offset Plane",             {}, tr("Offset Plane"),             [this]() { onConstructPlane(); }},
            {"Plane at Angle",           {}, tr("Plane at Angle"),           [this]() { statusBar()->showMessage(tr("Plane at Angle — not yet implemented")); }},
            {"Plane Through 3 Points",   {}, tr("Plane Through 3 Points"),  [this]() { statusBar()->showMessage(tr("Plane Through 3 Points — not yet implemented")); }},
            {"Axis Through 2 Points",    {}, tr("Axis Through 2 Points"),   [this]() { onConstructAxis(); }},
            {"Point at Vertex",          {}, tr("Point at Vertex"),          [this]() { onConstructPoint(); }},
        });
        addGroupSeparator(layout);

        // Group: Inspect
        addToolGroup(layout, "Inspect", {
            {"Measure", IconFactory::createIcon("measure"), tr("Measure (M) \u2014 Measure distances"),
             [this]() { onMeasure(); }},
        }, {
            {"Physical Properties", {}, tr("Physical Properties"), [this]() { statusBar()->showMessage(tr("Physical Properties — not yet implemented")); }},
            {"Face Count",          {}, tr("Face Count"),          [this]() {
                auto& brep = m_document->brepModel();
                auto ids = brep.bodyIds();
                int totalFaces = 0;
                for (const auto& id : ids) {
                    auto bq = brep.query(id);
                    totalFaces += bq.faceCount();
                }
                statusBar()->showMessage(tr("Total faces: %1").arg(totalFaces));
            }},
            {"Edge Count",          {}, tr("Edge Count"),          [this]() {
                auto& brep = m_document->brepModel();
                auto ids = brep.bodyIds();
                int totalEdges = 0;
                for (const auto& id : ids) {
                    auto bq = brep.query(id);
                    totalEdges += bq.edgeCount();
                }
                statusBar()->showMessage(tr("Total edges: %1").arg(totalEdges));
            }},
        });

        layout->addStretch();
        m_solidTabIndex = m_ribbon->addTab(tab, tr("SOLID"));
    }

    // ════════════════════════════════════════════════════════════════════
    // Tab 2: SKETCH (shown/auto-selected during sketch editing)
    // ════════════════════════════════════════════════════════════════════
    {
        auto* tab = new QWidget;
        auto* layout = new QHBoxLayout(tab);
        layout->setContentsMargins(6, 2, 6, 2);
        layout->setSpacing(2);

        // Group: Draw
        addToolGroup(layout, "Draw", {
            {"Line",      IconFactory::createIcon("line"),      tr("Line (L)"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::DrawLine); }},
            {"Rectangle", IconFactory::createIcon("rectangle"), tr("Rectangle (R)"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::DrawRectangle); }},
            {"Circle",    IconFactory::createIcon("circle"),    tr("Circle (C)"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::DrawCircle); }},
            {"Arc",       IconFactory::createIcon("arc"),       tr("Arc (A)"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::DrawArc); }},
            {"Ellipse",   IconFactory::createIcon("ellipse"),   tr("Ellipse"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::DrawEllipse); }},
            {"Polygon",   IconFactory::createIcon("polygon"),   tr("Polygon"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::DrawPolygon); }},
        });
        addGroupSeparator(layout);

        // Group: Constrain
        addToolGroup(layout, "Constrain", {
            {"Coincident",    IconFactory::createIcon("coincident"),    tr("Coincident \u2014 Merge two points"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::ConstrainCoincident); }},
            {"Parallel",      IconFactory::createIcon("parallel_c"),    tr("Parallel \u2014 Make lines parallel"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::ConstrainParallel); }},
            {"Perpendicular", IconFactory::createIcon("perpendicular"), tr("Perpendicular \u2014 Make lines perpendicular"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::ConstrainPerpendicular); }},
            {"Tangent",       IconFactory::createIcon("tangent_c"),     tr("Tangent \u2014 Make curves tangent"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::ConstrainTangent); }},
            {"Equal",         IconFactory::createIcon("equal_c"),       tr("Equal \u2014 Equal length or radius"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::ConstrainEqual); }},
            {"Symmetric",     IconFactory::createIcon("symmetric_c"),   tr("Symmetric \u2014 Mirror about a line"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::ConstrainSymmetric); }},
            {"Fix",           IconFactory::createIcon("fix"),           tr("Fix \u2014 Lock a point in place"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::AddConstraint); }},
            {"Dimension",     IconFactory::createIcon("dimension"),     tr("Dimension \u2014 Set a parametric dimension"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::Dimension); }},
        });
        addGroupSeparator(layout);

        // Group: Modify
        addToolGroup(layout, "Modify", {
            {"Trim",    IconFactory::createIcon("trim"),    tr("Trim (T)"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::Trim); }},
            {"Extend",  IconFactory::createIcon("extend"),  tr("Extend (E)"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::Extend); }},
            {"Offset",  IconFactory::createIcon("offset"),  tr("Offset (O)"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::Offset); }},
            {"Fillet",  IconFactory::createIcon("fillet"),   tr("Sketch Fillet (F)"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::SketchFillet); }},
            {"Chamfer", IconFactory::createIcon("chamfer"),  tr("Sketch Chamfer (G)"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::SketchChamfer); }},
        });
        addGroupSeparator(layout);

        // Group: Reference
        addToolGroup(layout, "Reference", {
            {"Project",      IconFactory::createIcon("project"),      tr("Project Edge (P)"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::ProjectEdge); }},
            {"Construction", IconFactory::createIcon("construction"), tr("Construction Mode (X)"),
             [this]() { /* toggled via keyboard X in sketch editor */ }},
            {"Import DXF",   IconFactory::createIcon("import_dxf"),   tr("Import DXF into sketch"),
             [this]() { onImportDxfToSketch(); }},
            {"Import SVG",   IconFactory::createIcon("import_svg"),   tr("Import SVG into sketch"),
             [this]() { onImportSvgToSketch(); }},
        });
        addGroupSeparator(layout);

        // Group: Control
        addToolGroup(layout, "Control", {
            {"Select", IconFactory::createIcon("select"), tr("Select"),
             [this]() { if (m_sketchEditor) m_sketchEditor->setTool(SketchTool::None); }},
            {"Finish", IconFactory::createIcon("finish"), tr("Finish Sketch"),
             [this]() { if (m_sketchEditor) m_sketchEditor->finishEditing(); }},
        });

        layout->addStretch();
        m_sketchTabIndex = m_ribbon->addTab(tab, tr("SKETCH"));
    }

    // ════════════════════════════════════════════════════════════════════
    // Tab 3: ASSEMBLY
    // ════════════════════════════════════════════════════════════════════
    {
        auto* tab = new QWidget;
        auto* layout = new QHBoxLayout(tab);
        layout->setContentsMargins(6, 2, 6, 2);
        layout->setSpacing(2);

        addToolGroup(layout, "Assemble", {
            {"Joint",         IconFactory::createIcon("joint"),     tr("Joint (J) \u2014 Click two faces to create a joint"),
             [this]() { onAddJoint(); }},
            {"New Component", IconFactory::createIcon("component"), tr("New Component \u2014 Add a new component"),
             [this]() { onNewComponent(); }},
            {"Insert .kcd",   IconFactory::createIcon("insert"),    tr("Insert Component \u2014 Import a .kcd file"),
             [this]() { onInsertComponent(); }},
        });
        addGroupSeparator(layout);

        addToolGroup(layout, "Inspect", {
            {"Interference", IconFactory::createIcon("interference"), tr("Check Interference"),
             [this]() { onCheckInterference(); }},
        });

        layout->addStretch();
        m_assemblyTabIndex = m_ribbon->addTab(tab, tr("ASSEMBLY"));
    }

    containerLayout->addWidget(m_ribbon);

    // Install the ribbon container into a toolbar-area wrapper so it sits
    // above the central widget (below the menu bar).
    auto* ribbonToolBar = new QToolBar(tr("Ribbon"), this);
    ribbonToolBar->setObjectName("RibbonToolBar");
    ribbonToolBar->setMovable(false);
    ribbonToolBar->setFloatable(false);
    ribbonToolBar->setAllowedAreas(Qt::TopToolBarArea);
    ribbonToolBar->setContentsMargins(0, 0, 0, 0);
    auto* ribbonWidgetAction = new QWidgetAction(this);
    ribbonWidgetAction->setDefaultWidget(m_ribbonContainer);
    ribbonToolBar->addAction(ribbonWidgetAction);
    addToolBar(Qt::TopToolBarArea, ribbonToolBar);

    // ── Global shortcut actions (invisible, keyboard-only) ──────────────
    m_extrudeAction = new QAction(tr("Extrude"), this);
    m_extrudeAction->setShortcut(QKeySequence(tr("E")));
    connect(m_extrudeAction, &QAction::triggered, this, &MainWindow::onExtrudeSketch);
    addAction(m_extrudeAction);

    m_filletAction = new QAction(tr("Fillet"), this);
    m_filletAction->setShortcut(QKeySequence(tr("F")));
    connect(m_filletAction, &QAction::triggered, this, &MainWindow::onFillet);
    addAction(m_filletAction);

    m_chamferAction = new QAction(tr("Chamfer"), this);
    connect(m_chamferAction, &QAction::triggered, this, &MainWindow::onChamfer);
    addAction(m_chamferAction);

    m_shellAction = new QAction(tr("Shell"), this);
    connect(m_shellAction, &QAction::triggered, this, &MainWindow::onShell);
    addAction(m_shellAction);

    m_draftAction = new QAction(tr("Draft"), this);
    connect(m_draftAction, &QAction::triggered, this, &MainWindow::onDraft);
    addAction(m_draftAction);

    m_holeAction = new QAction(tr("Hole"), this);
    m_holeAction->setShortcut(QKeySequence(tr("H")));
    connect(m_holeAction, &QAction::triggered, this, &MainWindow::onAddHole);
    addAction(m_holeAction);

    m_jointAction = new QAction(tr("Joint"), this);
    m_jointAction->setShortcut(QKeySequence(tr("J")));
    connect(m_jointAction, &QAction::triggered, this, &MainWindow::onAddJoint);
    addAction(m_jointAction);

    m_measureAction = new QAction(tr("Measure"), this);
    m_measureAction->setShortcut(QKeySequence(tr("M")));
    connect(m_measureAction, &QAction::triggered, this, &MainWindow::onMeasure);
    addAction(m_measureAction);

    auto* measureAliasI = new QAction(this);
    measureAliasI->setShortcut(QKeySequence(tr("I")));
    connect(measureAliasI, &QAction::triggered, this, &MainWindow::onMeasure);
    addAction(measureAliasI);

    auto* createSketchShortcut = new QAction(this);
    createSketchShortcut->setShortcut(QKeySequence(tr("S")));
    connect(createSketchShortcut, &QAction::triggered, this, &MainWindow::onCreateSketch);
    addAction(createSketchShortcut);

    auto* pressPullAction = new QAction(this);
    pressPullAction->setShortcut(QKeySequence(tr("Q")));
    connect(pressPullAction, &QAction::triggered, this, &MainWindow::onPressPull);
    addAction(pressPullAction);

    m_deleteAction = new QAction(tr("Delete"), this);
    m_deleteAction->setShortcut(QKeySequence::Delete);
    m_deleteAction->setStatusTip(tr("Delete \u2014 Delete the selected feature"));
    connect(m_deleteAction, &QAction::triggered, this, &MainWindow::onDeleteSelectedFeature);
    addAction(m_deleteAction);
}

void MainWindow::setupMenuBar()
{
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&New"),  this, &MainWindow::onNewDocument,  QKeySequence::New);
    fileMenu->addAction(tr("&Open"), this, &MainWindow::onOpenDocument, QKeySequence::Open);
    fileMenu->addAction(tr("&Save"), this, &MainWindow::onSaveDocument, QKeySequence::Save);

    m_recentFilesMenu = fileMenu->addMenu(tr("Recent Files"));
    updateRecentFilesMenu();

    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Import STEP/IGES..."), this, &MainWindow::onImportFile,
                        QKeySequence(tr("Ctrl+I")));
    fileMenu->addAction(tr("Import STL as Body..."), this, &MainWindow::onImportSTL);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Export STEP..."), this, &MainWindow::onExportSTEP);
    fileMenu->addAction(tr("Export STL..."),  this, &MainWindow::onExportSTL);
    fileMenu->addAction(tr("Export 3MF..."), this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, tr("Export 3MF"), {}, tr("3MF Files (*.3mf)"));
        if (path.isEmpty()) return;
        auto& brep = m_document->brepModel();
        auto ids = brep.bodyIds();
        if (ids.empty()) { statusBar()->showMessage(tr("Nothing to export")); return; }
        TopoDS_Compound compound;
        BRep_Builder builder;
        builder.MakeCompound(compound);
        for (const auto& id : ids)
            builder.Add(compound, brep.getShape(id));
        bool ok = m_document->kernel().export3MF(compound, path.toStdString());
        statusBar()->showMessage(ok ? tr("Exported 3MF: %1").arg(path) : tr("Export failed"));
    });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), qApp, &QApplication::quit, QKeySequence::Quit);

    auto* editMenu = menuBar()->addMenu(tr("&Edit"));
    m_undoAction = editMenu->addAction(tr("Undo"), this, &MainWindow::onUndo, QKeySequence::Undo);
    m_redoAction = editMenu->addAction(tr("Redo"), this, &MainWindow::onRedo, QKeySequence::Redo);
    m_undoAction->setEnabled(false);
    m_redoAction->setEnabled(false);
    editMenu->addSeparator();
    editMenu->addAction(tr("Preferences..."), this, &MainWindow::onPreferences,
                        QKeySequence(tr("Ctrl+,")));

    // -- View menu with selection filter modes ----------------------------
    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    m_filterGroup = new QActionGroup(this);
    m_filterGroup->setExclusive(true);

    m_selectAllAction = viewMenu->addAction(tr("Select All"));
    m_selectAllAction->setCheckable(true);
    m_selectAllAction->setChecked(true);
    m_selectAllAction->setShortcut(QKeySequence(tr("1")));
    m_filterGroup->addAction(m_selectAllAction);

    m_selectFacesAction = viewMenu->addAction(tr("Select Faces"));
    m_selectFacesAction->setCheckable(true);
    m_selectFacesAction->setShortcut(QKeySequence(tr("2")));
    m_filterGroup->addAction(m_selectFacesAction);

    m_selectEdgesAction = viewMenu->addAction(tr("Select Edges"));
    m_selectEdgesAction->setCheckable(true);
    m_selectEdgesAction->setShortcut(QKeySequence(tr("3")));
    m_filterGroup->addAction(m_selectEdgesAction);

    m_selectBodiesAction = viewMenu->addAction(tr("Select Bodies"));
    m_selectBodiesAction->setCheckable(true);
    m_selectBodiesAction->setShortcut(QKeySequence(tr("4")));
    m_filterGroup->addAction(m_selectBodiesAction);

    connect(m_selectAllAction, &QAction::triggered, this, [this]() {
        m_selectionMgr->setFilter(SelectionFilter::All);
        statusBar()->showMessage(tr("Selection filter: All"));
    });
    connect(m_selectFacesAction, &QAction::triggered, this, [this]() {
        m_selectionMgr->setFilter(SelectionFilter::Faces);
        statusBar()->showMessage(tr("Selection filter: Faces"));
    });
    connect(m_selectEdgesAction, &QAction::triggered, this, [this]() {
        m_selectionMgr->setFilter(SelectionFilter::Edges);
        statusBar()->showMessage(tr("Selection filter: Edges"));
    });
    connect(m_selectBodiesAction, &QAction::triggered, this, [this]() {
        m_selectionMgr->setFilter(SelectionFilter::Bodies);
        statusBar()->showMessage(tr("Selection filter: Bodies"));
    });

    // -- View mode (render style) ----------------------------------------
    viewMenu->addSeparator();
    m_viewModeGroup = new QActionGroup(this);
    m_viewModeGroup->setExclusive(true);

    m_viewSolidWithEdgesAction = viewMenu->addAction(tr("Solid with Edges"));
    m_viewSolidWithEdgesAction->setCheckable(true);
    m_viewSolidWithEdgesAction->setChecked(true);
    m_viewSolidWithEdgesAction->setShortcut(QKeySequence(tr("Ctrl+1")));
    m_viewModeGroup->addAction(m_viewSolidWithEdgesAction);

    m_viewSolidAction = viewMenu->addAction(tr("Solid Only"));
    m_viewSolidAction->setCheckable(true);
    m_viewSolidAction->setShortcut(QKeySequence(tr("Ctrl+2")));
    m_viewModeGroup->addAction(m_viewSolidAction);

    m_viewWireframeAction = viewMenu->addAction(tr("Wireframe"));
    m_viewWireframeAction->setCheckable(true);
    m_viewWireframeAction->setShortcut(QKeySequence(tr("W")));
    m_viewModeGroup->addAction(m_viewWireframeAction);

    connect(m_viewSolidWithEdgesAction, &QAction::triggered, this, [this]() {
        m_viewport->setViewMode(ViewMode::SolidWithEdges);
        statusBar()->showMessage(tr("View mode: Solid with Edges"));
    });
    connect(m_viewSolidAction, &QAction::triggered, this, [this]() {
        m_viewport->setViewMode(ViewMode::Solid);
        statusBar()->showMessage(tr("View mode: Solid Only"));
    });
    connect(m_viewWireframeAction, &QAction::triggered, this, [this]() {
        m_viewport->setViewMode(ViewMode::Wireframe);
        statusBar()->showMessage(tr("View mode: Wireframe"));
    });

    // -- Show Grid toggle ------------------------------------------------
    viewMenu->addSeparator();
    auto* showGridAction = viewMenu->addAction(tr("Show Grid"));
    showGridAction->setCheckable(true);
    showGridAction->setChecked(true);
    showGridAction->setShortcut(QKeySequence(tr("G")));
    connect(showGridAction, &QAction::toggled, this, [this](bool checked) {
        m_viewport->setShowGrid(checked);
        statusBar()->showMessage(checked ? tr("Grid visible") : tr("Grid hidden"));
    });

    // -- Show Origin toggle -----------------------------------------------
    auto* showOriginAction = viewMenu->addAction(tr("Show Origin"));
    showOriginAction->setCheckable(true);
    showOriginAction->setChecked(true);
    showOriginAction->setShortcut(QKeySequence(tr("O")));
    connect(showOriginAction, &QAction::toggled, this, [this](bool checked) {
        m_viewport->setShowOrigin(checked);
        statusBar()->showMessage(checked ? tr("Origin visible") : tr("Origin hidden"));
    });

    // -- Standard camera views ---------------------------------------------
    viewMenu->addSeparator();
    m_viewFrontAction = viewMenu->addAction(tr("Front View"));
    m_viewFrontAction->setShortcut(QKeySequence(Qt::Key_1 | Qt::KeypadModifier));
    connect(m_viewFrontAction, &QAction::triggered, this, [this]() {
        m_viewport->setStandardView(StandardView::Front);
        statusBar()->showMessage(tr("View: Front"));
    });

    m_viewBackAction = viewMenu->addAction(tr("Back View"));
    m_viewBackAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1 | Qt::KeypadModifier));
    connect(m_viewBackAction, &QAction::triggered, this, [this]() {
        m_viewport->setStandardView(StandardView::Back);
        statusBar()->showMessage(tr("View: Back"));
    });

    m_viewRightAction = viewMenu->addAction(tr("Right View"));
    m_viewRightAction->setShortcut(QKeySequence(Qt::Key_3 | Qt::KeypadModifier));
    connect(m_viewRightAction, &QAction::triggered, this, [this]() {
        m_viewport->setStandardView(StandardView::Right);
        statusBar()->showMessage(tr("View: Right"));
    });

    m_viewLeftAction = viewMenu->addAction(tr("Left View"));
    m_viewLeftAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_3 | Qt::KeypadModifier));
    connect(m_viewLeftAction, &QAction::triggered, this, [this]() {
        m_viewport->setStandardView(StandardView::Left);
        statusBar()->showMessage(tr("View: Left"));
    });

    m_viewTopAction = viewMenu->addAction(tr("Top View"));
    m_viewTopAction->setShortcut(QKeySequence(Qt::Key_7 | Qt::KeypadModifier));
    connect(m_viewTopAction, &QAction::triggered, this, [this]() {
        m_viewport->setStandardView(StandardView::Top);
        statusBar()->showMessage(tr("View: Top"));
    });

    m_viewBottomAction = viewMenu->addAction(tr("Bottom View"));
    m_viewBottomAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_7 | Qt::KeypadModifier));
    connect(m_viewBottomAction, &QAction::triggered, this, [this]() {
        m_viewport->setStandardView(StandardView::Bottom);
        statusBar()->showMessage(tr("View: Bottom"));
    });

    m_viewIsometricAction = viewMenu->addAction(tr("Isometric View"));
    m_viewIsometricAction->setShortcut(QKeySequence(Qt::Key_0 | Qt::KeypadModifier));
    connect(m_viewIsometricAction, &QAction::triggered, this, [this]() {
        m_viewport->setStandardView(StandardView::Isometric);
        statusBar()->showMessage(tr("View: Isometric"));
    });

    viewMenu->addSeparator();
    m_viewFitAllAction = viewMenu->addAction(tr("Fit All"));
    m_viewFitAllAction->setShortcut(QKeySequence(Qt::Key_Home));
    connect(m_viewFitAllAction, &QAction::triggered, this, [this]() {
        m_viewport->fitAll();
        statusBar()->showMessage(tr("Fit All"));
    });

    // -- Section view (clipping plane) ------------------------------------
    viewMenu->addSeparator();
    m_sectionXAction = viewMenu->addAction(tr("Section View X"), this, &MainWindow::onSectionX);
    m_sectionXAction->setShortcut(QKeySequence(tr("Shift+X")));
    m_sectionYAction = viewMenu->addAction(tr("Section View Y"), this, &MainWindow::onSectionY);
    m_sectionYAction->setShortcut(QKeySequence(tr("Shift+Y")));
    m_sectionZAction = viewMenu->addAction(tr("Section View Z"), this, &MainWindow::onSectionZ);
    m_sectionZAction->setShortcut(QKeySequence(tr("Shift+Z")));
    m_clearSectionAction = viewMenu->addAction(tr("Clear Section"), this, &MainWindow::onClearSection);
    m_clearSectionAction->setShortcut(QKeySequence(tr("Shift+C")));

    // Section plane slider in the status bar
    m_sectionSlider = new QSlider(Qt::Horizontal, this);
    m_sectionSlider->setRange(-1000, 1000);
    m_sectionSlider->setValue(0);
    m_sectionSlider->setFixedWidth(200);
    m_sectionSlider->setToolTip(tr("Section plane position"));
    m_sectionSlider->setVisible(false);
    statusBar()->addPermanentWidget(m_sectionSlider);
    connect(m_sectionSlider, &QSlider::valueChanged,
            this, &MainWindow::onSectionSliderChanged);

    auto* sketchMenu = menuBar()->addMenu(tr("&Sketch"));
    sketchMenu->addAction(tr("Create Sketch"), this, &MainWindow::onCreateSketch);
    sketchMenu->addAction(tr("Edit Sketch"), this, &MainWindow::onEditSketch);
    sketchMenu->addSeparator();
    sketchMenu->addAction(tr("Import DXF..."), this, &MainWindow::onImportDxfToSketch);
    sketchMenu->addAction(tr("Import SVG..."), this, &MainWindow::onImportSvgToSketch);

    auto* modelMenu = menuBar()->addMenu(tr("&Model"));
    modelMenu->addAction(tr("Create Box"), this, &MainWindow::onCreateBox);
    modelMenu->addAction(tr("Create Cylinder"), this, &MainWindow::onCreateCylinder);
    modelMenu->addAction(tr("Create Sphere"), this, &MainWindow::onCreateSphere);
    modelMenu->addSeparator();
    modelMenu->addAction(tr("Extrude Sketch"), this, &MainWindow::onExtrudeSketch);
    modelMenu->addAction(tr("Revolve Sketch"), this, &MainWindow::onRevolveSketch);
    modelMenu->addSeparator();
    modelMenu->addAction(tr("Add Hole"), this, &MainWindow::onAddHole);
    modelMenu->addAction(tr("Extrude (Dialog)..."), this, [this]() {
        executeInteractiveCommand(std::make_unique<document::ExtrudeInteractiveCommand>());
    });
    modelMenu->addAction(tr("Hole (Dialog)..."), this, [this]() {
        executeInteractiveCommand(std::make_unique<document::HoleInteractiveCommand>());
    });
    modelMenu->addAction(tr("Shell..."), this, &MainWindow::onShell);
    modelMenu->addAction(tr("Fillet..."), this, &MainWindow::onFillet);
    modelMenu->addAction(tr("Chamfer..."), this, &MainWindow::onChamfer);
    modelMenu->addAction(tr("Draft..."), this, &MainWindow::onDraft);
    modelMenu->addSeparator();
    modelMenu->addAction(tr("Mirror Last Body"), this, &MainWindow::onMirrorLastBody);
    modelMenu->addAction(tr("Circular Pattern"), this, &MainWindow::onCircularPattern);
    modelMenu->addAction(tr("Rectangular Pattern"), this, &MainWindow::onRectangularPattern);
    modelMenu->addSeparator();
    modelMenu->addAction(tr("Sweep (test helix)"), this, &MainWindow::onSweepTest);
    modelMenu->addAction(tr("Loft (test sections)"), this, &MainWindow::onLoftTest);
    modelMenu->addAction(tr("Sweep Sketch along Path"), this, &MainWindow::onSweepSketch);

    auto* assemblyMenu = menuBar()->addMenu(tr("&Assembly"));
    assemblyMenu->addAction(tr("New Component"), this, &MainWindow::onNewComponent);
    assemblyMenu->addAction(tr("Insert Component from .kcd..."), this, &MainWindow::onInsertComponent);
    assemblyMenu->addSeparator();
    assemblyMenu->addAction(tr("Add Joint (click faces)..."), this, &MainWindow::onAddJoint,
                            QKeySequence(tr("J")));

    // ── View menu: Exploded View ────────────────────────────────────────
    // (Appended to existing View menu -- find it via menuBar)
    {
        // The View menu was already created above; find it and append.
        QMenu* existingViewMenu = nullptr;
        for (auto* act : menuBar()->actions()) {
            if (act->text().contains("View")) {
                existingViewMenu = act->menu();
                break;
            }
        }
        if (existingViewMenu) {
            existingViewMenu->addSeparator();
            auto* explodeMenu = existingViewMenu->addMenu(tr("Exploded View"));
            m_explodeSlider = new QSlider(Qt::Horizontal, this);
            m_explodeSlider->setRange(0, 100);
            m_explodeSlider->setValue(0);
            m_explodeSlider->setFixedWidth(200);
            connect(m_explodeSlider, &QSlider::valueChanged, this, [this](int value) {
                float factor = static_cast<float>(value) / 100.0f;
                m_viewport->setExplodeFactor(factor);
                statusBar()->showMessage(tr("Explode: %1%").arg(value));
            });
            auto* sliderAction = new QWidgetAction(this);
            sliderAction->setDefaultWidget(m_explodeSlider);
            explodeMenu->addAction(sliderAction);
        }
    }

    // ── Tools menu ──────────────────────────────────────────────────────
    auto* toolsMenu = menuBar()->addMenu(tr("&Tools"));
    toolsMenu->addAction(tr("&Measure"), this, &MainWindow::onMeasure,
                          QKeySequence(tr("M")));
    toolsMenu->addSeparator();
    toolsMenu->addAction(tr("&Parameters..."), this, [this]() {
        // Find and raise the Parameters dock
        const auto docks = findChildren<QDockWidget*>();
        for (auto* dock : docks) {
            if (dock->windowTitle() == tr("Parameters")) {
                dock->show();
                dock->raise();
                break;
            }
        }
    });
    toolsMenu->addSeparator();
    toolsMenu->addAction(tr("Check &Interference"), this, &MainWindow::onCheckInterference);
    toolsMenu->addSeparator();
    toolsMenu->addAction(tr("Create &Drawing..."), this, &MainWindow::onCreateDrawing);

    // --- Appearance menu (material assignment) ---
    auto* appearanceMenu = menuBar()->addMenu(tr("A&ppearance"));

    QMenu* setBodyMatMenu = appearanceMenu->addMenu(tr("Set Body Material"));
    const auto& allMats = kernel::MaterialLibrary::all();
    for (const auto& mat : allMats) {
        QString matName = QString::fromStdString(mat.name);
        QAction* matAction = setBodyMatMenu->addAction(matName);

        // Color swatch icon
        QPixmap swatch(16, 16);
        swatch.fill(QColor::fromRgbF(mat.baseR, mat.baseG, mat.baseB));
        matAction->setIcon(QIcon(swatch));

        connect(matAction, &QAction::triggered, this, [this, matName]() {
            if (!m_selectionMgr->hasSelection()) return;
            const auto& hit = m_selectionMgr->selection().front();
            if (hit.bodyId.empty()) return;
            const auto* m = kernel::MaterialLibrary::byName(matName.toStdString());
            if (m) {
                m_document->appearances().setBodyMaterial(hit.bodyId, *m);
                m_document->setModified(true);
                updateViewport();
                updateWindowTitle();
                onSelectionChanged();
            }
        });
    }

    QMenu* setFaceMatMenu = appearanceMenu->addMenu(tr("Set Face Material"));
    for (const auto& mat : allMats) {
        QString matName = QString::fromStdString(mat.name);
        QAction* matAction = setFaceMatMenu->addAction(matName);

        QPixmap swatch(16, 16);
        swatch.fill(QColor::fromRgbF(mat.baseR, mat.baseG, mat.baseB));
        matAction->setIcon(QIcon(swatch));

        connect(matAction, &QAction::triggered, this, [this, matName]() {
            if (!m_selectionMgr->hasSelection()) return;
            const auto& hit = m_selectionMgr->selection().front();
            if (hit.bodyId.empty() || hit.faceIndex < 0) return;
            const auto* m = kernel::MaterialLibrary::byName(matName.toStdString());
            if (m) {
                m_document->appearances().setFaceMaterial(
                    hit.bodyId, hit.faceIndex, *m);
                m_document->setModified(true);
                updateViewport();
                updateWindowTitle();
                onSelectionChanged();
            }
        });
    }

    appearanceMenu->addSeparator();
    appearanceMenu->addAction(tr("Clear Face Material"), this, [this]() {
        if (!m_selectionMgr->hasSelection()) return;
        const auto& hit = m_selectionMgr->selection().front();
        if (hit.bodyId.empty() || hit.faceIndex < 0) return;
        m_document->appearances().clearFaceOverride(hit.bodyId, hit.faceIndex);
        m_document->setModified(true);
        updateViewport();
        updateWindowTitle();
        onSelectionChanged();
    });
    appearanceMenu->addAction(tr("Clear Body Material"), this, [this]() {
        if (!m_selectionMgr->hasSelection()) return;
        const auto& hit = m_selectionMgr->selection().front();
        if (hit.bodyId.empty()) return;
        m_document->appearances().clearBody(hit.bodyId);
        m_document->setModified(true);
        updateViewport();
        updateWindowTitle();
        onSelectionChanged();
    });
}

void MainWindow::setupDocks()
{
    // Feature tree -- left, with browser tab bar above it
    m_featureTree = new FeatureTree(this);

    auto* browserContainer = new QWidget(this);
    auto* browserLayout = new QVBoxLayout(browserContainer);
    browserLayout->setContentsMargins(0, 0, 0, 0);
    browserLayout->setSpacing(0);

    auto* browserTabBar = new QTabBar(browserContainer);
    browserTabBar->addTab(tr("Model"));
    browserTabBar->addTab(tr("Bodies"));
    browserTabBar->addTab(tr("Sketches"));
    browserTabBar->addTab(tr("Components"));
    browserTabBar->setCurrentIndex(0);
    browserTabBar->setExpanding(false);
    browserLayout->addWidget(browserTabBar);
    browserLayout->addWidget(m_featureTree, 1);

    connect(browserTabBar, &QTabBar::currentChanged, this, [this](int index) {
        switch (index) {
        case 0: m_featureTree->applyFilter(FeatureTree::BrowserTab::Model);      break;
        case 1: m_featureTree->applyFilter(FeatureTree::BrowserTab::Bodies);     break;
        case 2: m_featureTree->applyFilter(FeatureTree::BrowserTab::Sketches);   break;
        case 3: m_featureTree->applyFilter(FeatureTree::BrowserTab::Components); break;
        }
    });

    auto* leftDock = new QDockWidget(tr("Browser"), this);
    leftDock->setWidget(browserContainer);
    leftDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    leftDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::LeftDockWidgetArea, leftDock);

    // Properties -- right
    m_properties = new PropertiesPanel(this);
    auto* rightDock = new QDockWidget(tr("Properties"), this);
    rightDock->setWidget(m_properties);
    rightDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::RightDockWidgetArea, rightDock);

    // Parameter Table -- tabbed alongside Properties on the right
    m_parameterTable = new ParameterTablePanel(this);
    auto* paramDock = new QDockWidget(tr("Parameters"), this);
    paramDock->setWidget(m_parameterTable);
    paramDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::RightDockWidgetArea, paramDock);
    tabifyDockWidget(rightDock, paramDock);
    rightDock->raise();  // Properties tab is shown first by default

    // Timeline -- bottom
    m_timeline = new TimelinePanel(this);
    auto* bottomDock = new QDockWidget(tr("Timeline"), this);
    bottomDock->setWidget(m_timeline);
    bottomDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    bottomDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::BottomDockWidgetArea, bottomDock);
}

void MainWindow::connectSignals()
{
    // Ribbon tab changed -> update window title to show current workspace
    if (m_ribbon) {
        connect(m_ribbon, &QTabWidget::currentChanged, this, [this](int) {
            updateWindowTitle();
        });
    }

    // Timeline marker moved -> roll-back / roll-forward and refresh
    connect(m_timeline, &TimelinePanel::markerMoved, this, [this](int index) {
        m_document->timeline().setMarker(static_cast<size_t>(index));
        m_document->recompute();
        refreshAllPanels();
    });

    // Timeline entry double-clicked -> roll back and edit feature
    connect(m_timeline, &TimelinePanel::entryDoubleClicked,
            this, &MainWindow::onEditFeature);

    // Timeline drag-reorder -> execute as undoable command
    connect(m_timeline, &TimelinePanel::reorderRequested, this, [this](int from, int to) {
        auto& tl = m_document->timeline();
        if (static_cast<size_t>(from) >= tl.count()) return;
        const std::string featureId = tl.entry(static_cast<size_t>(from)).id;
        try {
            m_document->executeCommand(
                std::make_unique<document::ReorderFeatureCommand>(featureId, static_cast<size_t>(to)));
            refreshAllPanels();
        } catch (const std::exception& e) {
            statusBar()->showMessage(tr(e.what()), 3000);
        }
    });

    // Feature tree item selected -> show in properties + highlight + manipulator
    connect(m_featureTree, &FeatureTree::featureSelected, this, [this](const QString& featureId) {
        m_properties->showFeature(featureId);

        // Don't interfere when already in edit mode or sketch mode
        if (!m_editingFeatureId.isEmpty() ||
            (m_sketchEditor && m_sketchEditor->isEditing()))
            return;

        // Look up the feature in the timeline
        auto& tl = m_document->timeline();
        const features::Feature* feat = nullptr;
        for (size_t i = 0; i < tl.count(); ++i) {
            if (tl.entry(i).id == featureId.toStdString()) {
                feat = tl.entry(i).feature.get();
                break;
            }
        }
        if (!feat) {
            hideManipulator();
            m_viewport->setHighlightedSketch(nullptr);
            m_viewport->setHighlightedFaces({});
            return;
        }

        // --- Issue 3: If the selected feature is a Sketch, highlight it ---
        if (feat->type() == features::FeatureType::Sketch) {
            auto* skFeat = static_cast<const features::SketchFeature*>(feat);
            m_viewport->setHighlightedSketch(&skFeat->sketch());
            hideManipulator();
            m_viewport->setHighlightedFaces({});
            return;
        }

        // Clear sketch highlight for non-sketch features
        m_viewport->setHighlightedSketch(nullptr);

        // --- Issue 2: Show manipulator for dimensional features ---
        if (feat->type() == features::FeatureType::Extrude) {
            showExtrudeManipulator(featureId);
        } else if (feat->type() == features::FeatureType::Fillet) {
            showFilletManipulator(featureId);
        } else {
            hideManipulator();
        }
    });

    // Viewport Ctrl+drag -> move occurrence transform
    connect(m_viewport, &Viewport3D::occurrenceDragged, this,
            [this](float dx, float dy, float dz) {
        if (!m_selectionMgr->hasSelection())
            return;
        const auto& hit = m_selectionMgr->selection().front();
        if (hit.bodyId.empty())
            return;

        // Find the occurrence that owns this body
        std::string occId = m_document->findOccurrenceForBody(hit.bodyId);
        if (occId.empty())
            return;

        // Update the occurrence transform (translation part only)
        auto& root = m_document->components().rootComponent();
        auto* occ = root.findOccurrence(occId);
        if (!occ)
            return;

        occ->transform[12] += static_cast<double>(dx);
        occ->transform[13] += static_cast<double>(dy);
        occ->transform[14] += static_cast<double>(dz);
        m_document->setModified(true);

        updateViewport();
    });

    // Feature tree "Edit" context menu -> roll back and edit feature
    connect(m_featureTree, &FeatureTree::featureEditRequested,
            this, &MainWindow::onEditFeature);

    // Feature tree delete request -> execute DeleteFeatureCommand
    connect(m_featureTree, &FeatureTree::featureDeleteRequested, this, [this](const QString& featureId) {
        m_document->executeCommand(
            std::make_unique<document::DeleteFeatureCommand>(featureId.toStdString()));
        m_properties->clear();
        refreshAllPanels();
    });

    // Feature tree suppress toggle -> execute SuppressFeatureCommand
    connect(m_featureTree, &FeatureTree::featureSuppressToggled, this, [this](const QString& featureId) {
        m_document->executeCommand(
            std::make_unique<document::SuppressFeatureCommand>(featureId.toStdString()));
        refreshAllPanels();
    });

    // Feature tree in-place rename -> update customName in TimelineEntry
    connect(m_featureTree, &FeatureTree::featureRenamed, this,
            [this](const QString& featureId, const QString& newName) {
        auto& tl = m_document->timeline();
        for (size_t i = 0; i < tl.count(); ++i) {
            if (tl.entry(i).id == featureId.toStdString()) {
                // If the new name matches the default name, clear customName
                if (newName.toStdString() == tl.entry(i).name)
                    tl.entry(i).customName.clear();
                else
                    tl.entry(i).customName = newName.toStdString();
                m_document->setModified(true);
                break;
            }
        }
    });

    // Feature tree: create component under a parent component
    connect(m_featureTree, &FeatureTree::createComponentRequested, this, [this](const QString& parentCompId) {
        auto& reg = m_document->components();
        auto* parent = reg.findComponent(parentCompId.toStdString());
        if (!parent) return;

        std::string newId = reg.createComponent("Component");
        parent->addOccurrence(newId);
        m_document->setModified(true);
        refreshAllPanels();
        statusBar()->showMessage(tr("Created component: %1").arg(QString::fromStdString(newId)));
    });

    // Feature tree: activate a component
    connect(m_featureTree, &FeatureTree::activateComponentRequested, this, [this](const QString& compId) {
        auto& reg = m_document->components();
        // Deactivate all components, then activate the requested one
        for (const auto& id : reg.componentIds()) {
            auto* c = reg.findComponent(id);
            if (c) c->setActive(false);
        }
        auto* target = reg.findComponent(compId.toStdString());
        if (target) {
            target->setActive(true);
            statusBar()->showMessage(
                tr("Activated component: %1").arg(QString::fromStdString(target->name())));
        }
        refreshAllPanels();
    });

    // Body visibility toggle from feature tree
    connect(m_featureTree, &FeatureTree::bodyVisibilityToggled, this,
            [this](const QString& bodyId, bool visible) {
        m_document->brepModel().setBodyVisible(bodyId.toStdString(), visible);
        updateViewport();
    });

    // Body isolate from feature tree double-click
    connect(m_featureTree, &FeatureTree::bodyIsolateRequested, this,
            [this](const QString& bodyId) {
        onIsolateBody(bodyId.toStdString());
    });

    // Body isolate from viewport double-click
    connect(m_viewport, &Viewport3D::bodyDoubleClicked, this,
            [this](const std::string& bodyId) {
        onIsolateBody(bodyId);
    });

    // Body material assignment from FeatureTree context menu
    connect(m_featureTree, &FeatureTree::bodyMaterialRequested, this,
            [this](const QString& bodyId, const QString& materialName) {
        const auto* mat = kernel::MaterialLibrary::byName(materialName.toStdString());
        if (mat) {
            m_document->appearances().setBodyMaterial(bodyId.toStdString(), *mat);
            m_document->setModified(true);
            updateViewport();
            updateWindowTitle();
            // Refresh properties if this body is currently selected
            if (m_selectionMgr->hasSelection() &&
                !m_selectionMgr->selection().empty() &&
                m_selectionMgr->selection().front().bodyId == bodyId.toStdString()) {
                onSelectionChanged();
            }
        }
    });

    // Material dropdown in properties panel -> update body material
    connect(m_properties, &PropertiesPanel::materialChanged, this,
            [this](const QString& bodyId, const QString& materialName) {
        const auto* mat = kernel::MaterialLibrary::byName(materialName.toStdString());
        if (mat) {
            m_document->appearances().setBodyMaterial(bodyId.toStdString(), *mat);
        } else {
            // "Default" — clear the assignment
            m_document->appearances().clearBody(bodyId.toStdString());
        }
        m_document->setModified(true);
        updateViewport();
        updateWindowTitle();
    });

    // Body color changed from properties panel color picker
    connect(m_properties, &PropertiesPanel::bodyColorChanged, this,
            [this](const QString& bodyId, const QColor& color) {
        kernel::Material mat = m_document->appearances().bodyMaterial(bodyId.toStdString());
        mat.baseR = static_cast<float>(color.redF());
        mat.baseG = static_cast<float>(color.greenF());
        mat.baseB = static_cast<float>(color.blueF());
        mat.name = "Custom";
        m_document->appearances().setBodyMaterial(bodyId.toStdString(), mat);
        m_document->setModified(true);
        updateViewport();
        updateWindowTitle();
    });

    // Property panel edits -> update feature params and trigger preview
    connect(m_properties, &PropertiesPanel::propertyChanged,
            this, &MainWindow::onPropertyChanged);

    // Commit preview when Enter is pressed or focus leaves the panel
    connect(m_properties, &PropertiesPanel::editingCommitted,
            this, &MainWindow::onEditingCommitted);

    // Cancel preview when Escape is pressed
    connect(m_properties, &PropertiesPanel::editingCancelled,
            this, &MainWindow::onEditingCancelled);
    connect(m_properties, &PropertiesPanel::finishSketchClicked, this, [this]() {
        if (m_sketchEditor && m_sketchEditor->isEditing())
            m_sketchEditor->finishEditing();
    });
}

void MainWindow::onPropertyChanged(const QString& featureId,
                                   const QString& propertyName,
                                   const QVariant& newValue)
{
    auto& tl = m_document->timeline();
    features::Feature* feat = nullptr;
    for (size_t i = 0; i < tl.count(); ++i) {
        auto& entry = tl.entry(i);
        if (entry.id == featureId.toStdString() ||
            (entry.feature && entry.feature->id() == featureId.toStdString()))
        {
            feat = entry.feature.get();
            break;
        }
    }
    if (!feat)
        return;

    using FT = features::FeatureType;
    switch (feat->type()) {
    case FT::Extrude: {
        auto* ef = static_cast<features::ExtrudeFeature*>(feat);
        auto& p = ef->params();
        if (propertyName == QLatin1String("Distance")) {
            std::ostringstream oss;
            oss << newValue.toDouble() << " mm";
            p.distanceExpr = oss.str();
        } else if (propertyName == QLatin1String("distanceExpr")) {
            p.distanceExpr = newValue.toString().toStdString();
        } else if (propertyName == QLatin1String("ExtentType")) {
            QString txt = newValue.toString();
            if (txt == QLatin1String("Distance"))    p.extentType = features::ExtentType::Distance;
            else if (txt == QLatin1String("ThroughAll"))  p.extentType = features::ExtentType::ThroughAll;
            else if (txt == QLatin1String("ToEntity"))    p.extentType = features::ExtentType::ToEntity;
            else if (txt == QLatin1String("Symmetric"))   p.extentType = features::ExtentType::Symmetric;
        } else if (propertyName == QLatin1String("Operation")) {
            QString txt = newValue.toString();
            if (txt == QLatin1String("NewBody"))      p.operation = features::FeatureOperation::NewBody;
            else if (txt == QLatin1String("Join"))    p.operation = features::FeatureOperation::Join;
            else if (txt == QLatin1String("Cut"))     p.operation = features::FeatureOperation::Cut;
            else if (txt == QLatin1String("Intersect")) p.operation = features::FeatureOperation::Intersect;
        } else if (propertyName == QLatin1String("TaperAngle")) {
            p.taperAngleDeg = newValue.toDouble();
        } else if (propertyName == QLatin1String("Symmetric")) {
            p.isSymmetric = newValue.toBool();
        }
        break;
    }
    case FT::Revolve: {
        auto* rf = static_cast<features::RevolveFeature*>(feat);
        auto& p = rf->params();
        if (propertyName == QLatin1String("Angle")) {
            std::ostringstream oss;
            oss << newValue.toDouble() << " deg";
            p.angleExpr = oss.str();
        } else if (propertyName == QLatin1String("angleExpr")) {
            p.angleExpr = newValue.toString().toStdString();
        } else if (propertyName == QLatin1String("AxisType")) {
            QString txt = newValue.toString();
            if (txt == QLatin1String("XAxis"))       p.axisType = features::AxisType::XAxis;
            else if (txt == QLatin1String("YAxis"))  p.axisType = features::AxisType::YAxis;
            else if (txt == QLatin1String("ZAxis"))  p.axisType = features::AxisType::ZAxis;
            else if (txt == QLatin1String("Custom")) p.axisType = features::AxisType::Custom;
        } else if (propertyName == QLatin1String("FullRevolution")) {
            p.isFullRevolution = newValue.toBool();
        }
        break;
    }
    case FT::Fillet: {
        auto* ff = static_cast<features::FilletFeature*>(feat);
        auto& p = ff->params();
        if (propertyName == QLatin1String("Radius")) {
            std::ostringstream oss;
            oss << newValue.toDouble() << " mm";
            p.radiusExpr = oss.str();
        } else if (propertyName == QLatin1String("radiusExpr")) {
            p.radiusExpr = newValue.toString().toStdString();
        }
        break;
    }
    case FT::Chamfer: {
        auto* cf = static_cast<features::ChamferFeature*>(feat);
        auto& p = cf->params();
        if (propertyName == QLatin1String("Distance")) {
            std::ostringstream oss;
            oss << newValue.toDouble() << " mm";
            p.distanceExpr = oss.str();
        } else if (propertyName == QLatin1String("distanceExpr")) {
            p.distanceExpr = newValue.toString().toStdString();
        } else if (propertyName == QLatin1String("ChamferType")) {
            QString txt = newValue.toString();
            if (txt == QLatin1String("EqualDistance"))      p.chamferType = features::ChamferType::EqualDistance;
            else if (txt == QLatin1String("TwoDistances"))  p.chamferType = features::ChamferType::TwoDistances;
            else if (txt == QLatin1String("DistanceAndAngle")) p.chamferType = features::ChamferType::DistanceAndAngle;
        }
        break;
    }
    case FT::Hole: {
        auto* hf = static_cast<features::HoleFeature*>(feat);
        auto& p = hf->params();
        if (propertyName == QLatin1String("Diameter")) {
            std::ostringstream oss;
            oss << newValue.toDouble() << " mm";
            p.diameterExpr = oss.str();
        } else if (propertyName == QLatin1String("diameterExpr")) {
            p.diameterExpr = newValue.toString().toStdString();
        } else if (propertyName == QLatin1String("Depth")) {
            std::ostringstream oss;
            oss << newValue.toDouble() << " mm";
            p.depthExpr = oss.str();
        } else if (propertyName == QLatin1String("depthExpr")) {
            p.depthExpr = newValue.toString().toStdString();
        } else if (propertyName == QLatin1String("TipAngle")) {
            p.tipAngleDeg = newValue.toDouble();
        }
        break;
    }
    case FT::Shell: {
        auto* sf = static_cast<features::ShellFeature*>(feat);
        auto& p = sf->params();
        if (propertyName == QLatin1String("Thickness")) {
            p.thicknessExpr = newValue.toDouble();
        }
        break;
    }
    default:
        break;
    }

    std::string fid = featureId.toStdString();

    // Rebuild dependency edges from the edited feature's updated params
    // before recomputing, so the dirty set includes any newly-referenced bodies.
    m_document->updateDependenciesFromParams(fid);

    if (!m_editingFeatureId.isEmpty()) {
        // During edit mode, use incremental recompute from the edited feature
        // for better performance -- only downstream features are recalculated.
        m_document->recomputeFrom(fid);
        updateViewport();
    } else {
        // Use the preview engine for live geometry updates instead of full recompute.
        // beginPreview saves the original params; updatePreview re-executes just
        // this feature and pushes a semi-transparent mesh to the viewport.
        if (!m_previewEngine->isActive() || m_previewEngine->activeFeatureId() != fid)
            m_previewEngine->beginPreview(fid);
        m_previewEngine->updatePreview();
    }
}

void MainWindow::onEditingCommitted(const QString& /*featureId*/)
{
    hideConfirmBar();
    m_viewport->setHighlightedFaces({});
    m_viewport->setHighlightedSketch(nullptr);

    if (m_previewEngine->isActive()) {
        m_previewEngine->commitPreview();
    }
    // If we were editing a feature (rolled back), restore the timeline marker
    // and recompute incrementally from the edited feature forward.
    if (!m_editingFeatureId.isEmpty()) {
        QString editedId = m_editingFeatureId;
        m_editingFeatureId.clear();

        auto& tl = m_document->timeline();
        // Restore to original marker position (or end if it was at end)
        tl.setMarker(std::min(m_editOriginalMarkerPos, tl.count()));
        m_document->recomputeFrom(editedId.toStdString());

        m_properties->setEditMode(false);
        m_timeline->setEditingFeatureId(QString());
        refreshAllPanels();

        statusBar()->showMessage(
            tr("Applied changes to %1").arg(editedId), 3000);
    } else {
        refreshAllPanels();
    }
}

void MainWindow::onEditingCancelled(const QString& featureId)
{
    hideConfirmBar();
    hideManipulator();
    m_viewport->setHighlightedFaces({});
    m_viewport->setHighlightedSketch(nullptr);

    if (m_previewEngine->isActive()) {
        m_previewEngine->cancelPreview();
        // Restore the properties panel to show the original values
        m_properties->showFeature(featureId);
    }
    // If we were editing a feature (rolled back), restore the original marker
    // position and recompute to get back to the pre-edit state.
    if (!m_editingFeatureId.isEmpty()) {
        m_editingFeatureId.clear();

        auto& tl = m_document->timeline();
        tl.setMarker(std::min(m_editOriginalMarkerPos, tl.count()));
        m_document->recompute();

        m_properties->setEditMode(false);
        m_timeline->setEditingFeatureId(QString());
        refreshAllPanels();

        statusBar()->showMessage(tr("Edit cancelled"), 3000);
    }
}

void MainWindow::onEditFeature(const QString& featureId)
{
    // If already editing a different feature, finish that first
    if (!m_editingFeatureId.isEmpty() && m_editingFeatureId != featureId) {
        onFinishEditing();
    }

    // Find the feature's index in the timeline
    auto& tl = m_document->timeline();
    for (size_t i = 0; i < tl.count(); ++i) {
        if (tl.entry(i).id == featureId.toStdString()) {
            // Save original marker position so cancel can restore it
            m_editOriginalMarkerPos = tl.markerPosition();

            // Roll back to just after this feature so it and everything before are active
            tl.setMarker(i + 1);
            m_document->recompute();

            m_editingFeatureId = featureId;
            m_properties->showFeature(featureId);
            m_properties->setEditMode(true);
            m_timeline->setEditingFeatureId(featureId);
            refreshAllPanels();

            // Show the confirmation toolbar with feature name
            showConfirmBar(tr("Editing: %1")
                .arg(QString::fromStdString(tl.entry(i).displayName())));

            // Highlight input geometry of the feature (e.g. sketch profile faces)
            if (tl.entry(i).feature) {
                auto deps = m_document->depGraph().dependenciesOf(tl.entry(i).id);
                // Find sketch faces to highlight: any face belonging to the current body
                // is a good visual cue. For now, highlight all faces of the first body.
                std::string bodyId;
                auto faces = collectSelectedFaces(bodyId);
                // If we have face selection, highlight those; otherwise use all faces
                if (!faces.empty()) {
                    std::vector<int> faceIndices(faces.begin(), faces.end());
                    m_viewport->setHighlightedFaces(faceIndices);
                }

                // Sketch features: enter sketch editing mode directly
                if (tl.entry(i).feature->type() == features::FeatureType::Sketch) {
                    auto* skFeat = static_cast<features::SketchFeature*>(tl.entry(i).feature.get());
                    beginSketchEditing(skFeat);
                    return;
                }

                // Show distance manipulator for Extrude features
                if (tl.entry(i).feature->type() == features::FeatureType::Extrude) {
                    showExtrudeManipulator(featureId);
                }

                // Show radius manipulator for Fillet features
                if (tl.entry(i).feature->type() == features::FeatureType::Fillet) {
                    showFilletManipulator(featureId);
                }
            }

            statusBar()->showMessage(tr("Editing %1 \u2014 drag arrow or edit properties.  Enter:OK  Esc:Cancel")
                .arg(QString::fromStdString(tl.entry(i).displayName())));
            return;
        }
    }
}

void MainWindow::onFinishEditing()
{
    m_editingFeatureId.clear();
    hideManipulator();

    auto& tl = m_document->timeline();
    // Restore to original marker position (or end if it was at end)
    tl.setMarker(std::min(m_editOriginalMarkerPos, tl.count()));
    m_document->recompute();

    m_properties->setEditMode(false);
    m_timeline->setEditingFeatureId(QString());
    hideConfirmBar();
    m_viewport->setHighlightedFaces({});
    m_viewport->setHighlightedSketch(nullptr);
    refreshAllPanels();

    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (!m_editingFeatureId.isEmpty()) {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            onEditingCommitted(m_editingFeatureId);
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            onEditingCancelled(m_editingFeatureId);
            return;
        }
    }

    // Pending selection-driven command: Enter commits, Escape cancels
    if (m_pendingCommand != PendingCommand::None) {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            onCommitPendingCommand();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            onCancelPendingCommand();
            return;
        }
    }

    // Forward number keys to the feature dialog's distance field
    if (m_featureDialog && m_featureDialog->isActive()) {
        if ((event->key() >= Qt::Key_0 && event->key() <= Qt::Key_9) ||
             event->key() == Qt::Key_Period || event->key() == Qt::Key_Minus) {
            m_featureDialog->forwardKeyToDistance(event);
            return;
        }
    }

    // Escape exits isolated body view
    if (event->key() == Qt::Key_Escape && !m_isolatedBodyId.empty()) {
        onShowAll();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::onNewDocument()
{
    if (m_previewEngine->isActive())
        m_previewEngine->cancelPreview();
    m_document->newDocument();
    setWindowTitle("kernelCAD — Untitled");
    m_featureTree->setDocument(m_document.get());
    m_parameterTable->setDocument(m_document.get());
    m_properties->clear();
    m_selectionMgr->clearSelection();
    m_selectionMgr->clearPreSelection();
    m_firstBodyFitDone = false;
    m_isolatedBodyId.clear();
    refreshAllPanels();
}

void MainWindow::onIsolateBody(const std::string& bodyId)
{
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();

    if (!m_isolatedBodyId.empty() && m_isolatedBodyId == bodyId) {
        // Already isolated on this body -- toggle back to show all
        onShowAll();
        return;
    }

    m_isolatedBodyId = bodyId;
    for (const auto& id : ids) {
        brep.setBodyVisible(id, id == bodyId);
    }
    updateViewport();
    m_featureTree->refresh();
    statusBar()->showMessage(tr("Isolated body — double-click or press Escape to show all"), 3000);
}

void MainWindow::onShowAll()
{
    m_isolatedBodyId.clear();
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    for (const auto& id : ids) {
        brep.setBodyVisible(id, true);
    }
    updateViewport();
    m_featureTree->refresh();
    statusBar()->showMessage(tr("All bodies visible"), 2000);
}

void MainWindow::onOpenDocument()
{
    if (m_previewEngine->isActive())
        m_previewEngine->cancelPreview();
    QString path = QFileDialog::getOpenFileName(this, tr("Open"), {}, tr("kernelCAD Files (*.kcd)"));
    if (!path.isEmpty()) {
        if (m_document->load(path.toStdString())) {
            m_featureTree->setDocument(m_document.get());
            m_parameterTable->setDocument(m_document.get());
            m_properties->clear();
            refreshAllPanels();
            addToRecentFiles(path);
            setWindowTitle("kernelCAD \u2014 " + QFileInfo(path).baseName());
            statusBar()->showMessage(tr("Opened: %1").arg(path));
        } else {
            QMessageBox::warning(this, tr("Open Failed"),
                tr("Could not open file: %1").arg(path));
        }
    }
}

void MainWindow::onSaveDocument()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Save"), {}, tr("kernelCAD Files (*.kcd)"));
    if (!path.isEmpty()) {
        if (m_document->save(path.toStdString())) {
            addToRecentFiles(path);
            updateWindowTitle();
            statusBar()->showMessage(tr("Saved: %1").arg(path));
        } else {
            QMessageBox::warning(this, tr("Save Failed"),
                tr("Could not save file: %1").arg(path));
        }
    }
}

void MainWindow::onImportFile()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Import CAD File"), {},
        tr("CAD Files (*.step *.stp *.igs *.iges);;STEP Files (*.step *.stp);;IGES Files (*.igs *.iges)"));
    if (path.isEmpty())
        return;

    try {
        int count = m_document->importFile(path.toStdString());
        statusBar()->showMessage(
            tr("Imported %1 body(s) from %2").arg(count).arg(QFileInfo(path).fileName()));
        m_viewport->fitAll();
        refreshAllPanels();
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Import Failed"),
            tr("Could not import file:\n%1").arg(e.what()));
    }
}

void MainWindow::onImportSTL()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Import STL as Body"), {},
        tr("STL Files (*.stl)"));
    if (path.isEmpty())
        return;

    try {
        int count = m_document->importFile(path.toStdString());
        statusBar()->showMessage(
            tr("Imported %1 body(s) from %2").arg(count).arg(QFileInfo(path).fileName()));
        m_viewport->fitAll();
        refreshAllPanels();
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Import Failed"),
            tr("Could not import STL file:\n%1").arg(e.what()));
    }
}

void MainWindow::onImportDxfToSketch()
{
    if (!m_sketchEditor || !m_sketchEditor->isEditing()) {
        statusBar()->showMessage(tr("Enter sketch mode first"), 3000);
        return;
    }

    QString path = QFileDialog::getOpenFileName(this, tr("Import DXF"), {},
        tr("DXF Files (*.dxf)"));
    if (path.isEmpty()) return;

    auto result = sketch::DxfImporter::importFile(path.toStdString());
    auto* sk = m_sketchEditor->currentSketch();

    for (auto& ln : result.lines) {
        auto p1 = sk->addPoint(ln.x1, ln.y1);
        auto p2 = sk->addPoint(ln.x2, ln.y2);
        sk->addLine(p1, p2);
    }
    for (auto& c : result.circles) {
        auto cp = sk->addPoint(c.cx, c.cy);
        sk->addCircle(cp, c.radius);
    }
    for (auto& a : result.arcs) {
        sk->addArc(a.cx, a.cy, a.radius, a.startAngle, a.endAngle);
    }

    sk->solve();
    m_viewport->update();
    statusBar()->showMessage(tr("Imported %1 lines, %2 circles, %3 arcs from DXF")
        .arg(result.lines.size()).arg(result.circles.size()).arg(result.arcs.size()));
}

void MainWindow::onImportSvgToSketch()
{
    if (!m_sketchEditor || !m_sketchEditor->isEditing()) {
        statusBar()->showMessage(tr("Enter sketch mode first"), 3000);
        return;
    }

    QString path = QFileDialog::getOpenFileName(this, tr("Import SVG"), {},
        tr("SVG Files (*.svg)"));
    if (path.isEmpty()) return;

    auto result = sketch::SvgImporter::importFile(path.toStdString());
    auto* sk = m_sketchEditor->currentSketch();

    for (auto& ln : result.lines) {
        auto p1 = sk->addPoint(ln.x1, ln.y1);
        auto p2 = sk->addPoint(ln.x2, ln.y2);
        sk->addLine(p1, p2);
    }
    for (auto& c : result.circles) {
        auto cp = sk->addPoint(c.cx, c.cy);
        sk->addCircle(cp, c.radius);
    }
    for (auto& r : result.rects) {
        // Convert rectangle to 4 lines
        auto p1 = sk->addPoint(r.x, r.y);
        auto p2 = sk->addPoint(r.x + r.width, r.y);
        auto p3 = sk->addPoint(r.x + r.width, r.y + r.height);
        auto p4 = sk->addPoint(r.x, r.y + r.height);
        sk->addLine(p1, p2);
        sk->addLine(p2, p3);
        sk->addLine(p3, p4);
        sk->addLine(p4, p1);
    }

    sk->solve();
    m_viewport->update();
    statusBar()->showMessage(tr("Imported %1 lines, %2 circles, %3 rects from SVG")
        .arg(result.lines.size()).arg(result.circles.size()).arg(result.rects.size()));
}

void MainWindow::onExportSTEP()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Export STEP"), {}, tr("STEP Files (*.step *.stp)"));
    if (path.isEmpty()) return;

    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        statusBar()->showMessage(tr("Nothing to export"));
        return;
    }

    // Combine all bodies into a compound shape for export
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const auto& id : ids)
        builder.Add(compound, brep.getShape(id));

    bool ok = m_document->kernel().exportSTEP(compound, path.toStdString());
    statusBar()->showMessage(ok ? tr("Exported: %1").arg(path) : tr("Export failed"));
}

void MainWindow::onExportSTL()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Export STL"), {}, tr("STL Files (*.stl)"));
    if (path.isEmpty()) return;

    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        statusBar()->showMessage(tr("Nothing to export"));
        return;
    }

    // Combine all bodies into a compound shape for export
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const auto& id : ids)
        builder.Add(compound, brep.getShape(id));

    bool ok = m_document->kernel().exportSTL(compound, path.toStdString());
    statusBar()->showMessage(ok ? tr("Exported: %1").arg(path) : tr("Export failed"));
}

void MainWindow::onCreateBox()
{
    m_lastCommandName = tr("Create Box");
    m_lastCommandCallback = [this]() { onCreateBox(); };
    features::ExtrudeParams params;
    params.profileId    = "";           // empty = base-feature shortcut (makeBox)
    params.distanceExpr = "50 mm";
    params.extentType   = features::ExtentType::Distance;
    params.operation    = features::FeatureOperation::NewBody;

    m_document->executeCommand(
        std::make_unique<document::AddExtrudeCommand>(std::move(params)));
    statusBar()->showMessage(tr("Created box"));
    refreshAllPanels();

    // Select the newly created feature in the properties panel
    auto& tl = m_document->timeline();
    if (tl.count() > 0) {
        auto& lastEntry = tl.entry(tl.count() - 1);
        m_properties->showFeature(QString::fromStdString(lastEntry.id));
    }
}

void MainWindow::onCreateCylinder()
{
    m_lastCommandName = tr("Create Cylinder");
    m_lastCommandCallback = [this]() { onCreateCylinder(); };
    features::RevolveParams params;
    params.profileId       = "";          // empty = base-feature shortcut (makeCylinder)
    params.angleExpr       = "360 deg";
    params.isFullRevolution = true;
    params.operation       = features::FeatureOperation::NewBody;

    m_document->executeCommand(
        std::make_unique<document::AddRevolveCommand>(std::move(params)));
    statusBar()->showMessage(tr("Created cylinder"));
    refreshAllPanels();

    // Select the newly created feature in the properties panel
    auto& tl = m_document->timeline();
    if (tl.count() > 0) {
        auto& lastEntry = tl.entry(tl.count() - 1);
        m_properties->showFeature(QString::fromStdString(lastEntry.id));
    }
}

void MainWindow::onCreateSphere()
{
    m_lastCommandName = tr("Create Sphere");
    m_lastCommandCallback = [this]() { onCreateSphere(); };
    m_document->executeCommand(
        std::make_unique<document::AddSphereCommand>(25.0));
    statusBar()->showMessage(tr("Created sphere"));
    refreshAllPanels();

    // Select the newly created feature in the properties panel
    auto& tl = m_document->timeline();
    if (tl.count() > 0) {
        auto& lastEntry = tl.entry(tl.count() - 1);
        m_properties->showFeature(QString::fromStdString(lastEntry.id));
    }
}

void MainWindow::onCreateSketch()
{
    m_lastCommandName = tr("Create Sketch");
    m_lastCommandCallback = [this]() { onCreateSketch(); };
    // If already editing a sketch, finish first
    if (m_sketchEditor->isEditing())
        m_sketchEditor->finishEditing();

    // Enter "pick a plane" mode -- wait for user to click an origin plane or planar face
    m_pendingSketchPlane = true;
    m_pendingCommand = PendingCommand::SketchPlane;
    m_selectionMgr->setFilter(SelectionFilter::Faces);
    if (m_selectFacesAction) m_selectFacesAction->setChecked(true);
    statusBar()->showMessage(tr("Select a plane or planar face to sketch on..."));
    showConfirmBar(tr("Sketch: Pick Plane"));
}

std::string MainWindow::hitTestOriginPlanes(const QPoint& screenPos,
                                            double& ox, double& oy, double& oz,
                                            double& xDirX, double& xDirY, double& xDirZ,
                                            double& yDirX, double& yDirY, double& yDirZ) const
{
    // Unproject screen position to a ray using the viewport camera matrices
    QMatrix4x4 view = m_viewport->viewMatrix();
    QMatrix4x4 proj = m_viewport->projectionMatrix();
    QMatrix4x4 invVP = (proj * view).inverted();

    float ndcX = (2.0f * screenPos.x()) / m_viewport->width() - 1.0f;
    float ndcY = 1.0f - (2.0f * screenPos.y()) / m_viewport->height();

    QVector4D nearPt4 = invVP * QVector4D(ndcX, ndcY, -1.0f, 1.0f);
    QVector4D farPt4  = invVP * QVector4D(ndcX, ndcY,  1.0f, 1.0f);
    if (std::abs(nearPt4.w()) > 1e-7f) nearPt4 /= nearPt4.w();
    if (std::abs(farPt4.w())  > 1e-7f) farPt4  /= farPt4.w();

    QVector3D rayOrigin = nearPt4.toVector3D();
    QVector3D rayDir    = (farPt4.toVector3D() - rayOrigin).normalized();

    // Origin plane half-extent (matches initOriginPlanes)
    constexpr float S = 50.0f;

    // Test each origin plane: XY (normal Z), XZ (normal Y), YZ (normal X)
    struct PlaneTest {
        const char* name;
        QVector3D normal;
        // For bounds check: which two axes define the plane
        int axis1, axis2; // 0=X, 1=Y, 2=Z
        double oxv, oyv, ozv;
        double xdx, xdy, xdz, ydx, ydy, ydz;
    };

    PlaneTest planes[] = {
        {"XY", QVector3D(0, 0, 1), 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0},
        {"XZ", QVector3D(0, 1, 0), 0, 2, 0, 0, 0, 1, 0, 0, 0, 0, 1},
        {"YZ", QVector3D(1, 0, 0), 1, 2, 0, 0, 0, 0, 1, 0, 0, 0, 1},
    };

    float bestDist = std::numeric_limits<float>::max();
    std::string bestPlane;

    for (auto& pl : planes) {
        float denom = QVector3D::dotProduct(rayDir, pl.normal);
        if (std::abs(denom) < 1e-7f) continue; // parallel

        // Plane passes through origin, so d=0: t = -dot(rayOrigin, normal) / denom
        float t = -QVector3D::dotProduct(rayOrigin, pl.normal) / denom;
        if (t < 0.0f) continue; // behind camera

        QVector3D hitPt = rayOrigin + t * rayDir;

        // Check if within the plane bounds (S x S)
        float v1 = hitPt[pl.axis1];
        float v2 = hitPt[pl.axis2];
        if (std::abs(v1) <= S && std::abs(v2) <= S) {
            if (t < bestDist) {
                bestDist = t;
                bestPlane = pl.name;
                ox = pl.oxv; oy = pl.oyv; oz = pl.ozv;
                xDirX = pl.xdx; xDirY = pl.xdy; xDirZ = pl.xdz;
                yDirX = pl.ydx; yDirY = pl.ydy; yDirZ = pl.ydz;
            }
        }
    }

    return bestPlane;
}

void MainWindow::handleSketchPlaneSelection(const SelectionHit& hit)
{
    features::SketchParams skParams;

    // Check if the hit is on a planar face of a body
    if (!hit.bodyId.empty() && hit.faceIndex >= 0) {
        auto& brep = m_document->brepModel();
        if (brep.hasBody(hit.bodyId)) {
            try {
                kernel::BRepQuery bq = brep.query(hit.bodyId);
                if (hit.faceIndex < bq.faceCount()) {
                    kernel::FaceInfo fi = bq.faceInfo(hit.faceIndex);
                    if (fi.surfaceType == kernel::SurfaceType::Plane) {
                        // Use the face centroid as sketch origin and face normal for orientation
                        skParams.planeId = hit.bodyId + ":face:" + std::to_string(hit.faceIndex);
                        skParams.originX = fi.centroidX;
                        skParams.originY = fi.centroidY;
                        skParams.originZ = fi.centroidZ;

                        // Build a coordinate frame from the face normal
                        double nx = fi.normalX, ny = fi.normalY, nz = fi.normalZ;
                        // Choose an up vector that is not parallel to normal
                        double ux = 0, uy = 1, uz = 0;
                        if (std::abs(ny) > 0.9) { ux = 1; uy = 0; uz = 0; }
                        // xDir = cross(up, normal), then yDir = cross(normal, xDir)
                        skParams.xDirX = uy * nz - uz * ny;
                        skParams.xDirY = uz * nx - ux * nz;
                        skParams.xDirZ = ux * ny - uy * nx;
                        double xLen = std::sqrt(skParams.xDirX * skParams.xDirX +
                                                skParams.xDirY * skParams.xDirY +
                                                skParams.xDirZ * skParams.xDirZ);
                        if (xLen > 1e-9) {
                            skParams.xDirX /= xLen;
                            skParams.xDirY /= xLen;
                            skParams.xDirZ /= xLen;
                        }
                        skParams.yDirX = ny * skParams.xDirZ - nz * skParams.xDirY;
                        skParams.yDirY = nz * skParams.xDirX - nx * skParams.xDirZ;
                        skParams.yDirZ = nx * skParams.xDirY - ny * skParams.xDirX;
                    } else {
                        statusBar()->showMessage(tr("Selected face is not planar. Pick a flat face or origin plane."), 3000);
                        return;
                    }
                }
            } catch (...) {
                statusBar()->showMessage(tr("Could not query face geometry."), 3000);
                return;
            }
        }
    } else {
        // No body face hit -- check origin planes using the world hit position
        // Approximate: use the hit world coords to determine which origin plane
        statusBar()->showMessage(tr("No planar face selected. Click an origin plane or a flat face."), 3000);
        return;
    }

    // Clean up pending state
    m_pendingSketchPlane = false;
    m_pendingCommand = PendingCommand::None;
    m_selectionMgr->clearSelection();
    m_selectionMgr->setFilter(SelectionFilter::All);
    if (m_selectAllAction) m_selectAllAction->setChecked(true);
    hideConfirmBar();

    // Create the sketch with the selected plane params
    m_document->executeCommand(
        std::make_unique<document::AddSketchCommand>(std::move(skParams)));
    refreshAllPanels();

    // Find the newly created sketch feature and enter editing mode
    auto& tl = m_document->timeline();
    for (size_t i = tl.count(); i > 0; --i) {
        auto& entry = tl.entry(i - 1);
        if (entry.feature &&
            entry.feature->type() == features::FeatureType::Sketch &&
            !entry.isSuppressed && !entry.isRolledBack) {
            auto* skFeat = static_cast<features::SketchFeature*>(entry.feature.get());
            beginSketchEditing(skFeat);
            break;
        }
    }
}

void MainWindow::onEditSketch()
{
    // If already editing, finish first
    if (m_sketchEditor->isEditing())
        m_sketchEditor->finishEditing();

    // Find the last (or selected) sketch feature in the timeline
    auto& tl = m_document->timeline();
    features::SketchFeature* targetSketch = nullptr;

    for (size_t i = tl.count(); i > 0; --i) {
        auto& entry = tl.entry(i - 1);
        if (entry.feature &&
            entry.feature->type() == features::FeatureType::Sketch &&
            !entry.isSuppressed && !entry.isRolledBack) {
            targetSketch = static_cast<features::SketchFeature*>(entry.feature.get());
            break;
        }
    }

    if (!targetSketch) {
        statusBar()->showMessage(tr("No sketch found. Create a sketch first."), 3000);
        return;
    }

    beginSketchEditing(targetSketch);
}

void MainWindow::onExtrudeSketch()
{
    m_lastCommandName = tr("Extrude");
    m_lastCommandCallback = [this]() { onExtrudeSketch(); };

    // Collect all sketches with closed profiles
    auto& tl = m_document->timeline();
    std::vector<std::pair<features::SketchFeature*, std::string>> sketchesWithProfiles;

    for (size_t i = 0; i < tl.count(); ++i) {
        auto& entry = tl.entry(i);
        if (entry.feature &&
            entry.feature->type() == features::FeatureType::Sketch &&
            !entry.isSuppressed && !entry.isRolledBack) {
            auto* sk = static_cast<features::SketchFeature*>(entry.feature.get());
            auto profiles = sk->sketch().detectProfiles();
            if (!profiles.empty())
                sketchesWithProfiles.push_back({sk, entry.feature->id()});
        }
    }

    if (sketchesWithProfiles.empty()) {
        statusBar()->showMessage(tr("No sketch with closed profiles found. Draw a closed shape first (S → R for rectangle)."), 5000);
        return;
    }

    // If exactly one sketch with profiles, use it directly
    // If multiple, use the last one (most recent)
    auto* targetSketch = sketchesWithProfiles.back().first;
    std::string targetSketchId = sketchesWithProfiles.back().second;

    // If a sketch is selected in the feature tree, prefer that one
    for (auto& [sk, skId] : sketchesWithProfiles) {
        // Check if this sketch is highlighted/selected
        if (!m_selectionMgr->selection().empty()) {
            std::string selectedBody = m_selectionMgr->selection().front().bodyId;
            std::string featureForSel = m_document->featureForBody(selectedBody);
            if (featureForSel == skId) {
                targetSketch = sk;
                targetSketchId = skId;
                break;
            }
        }
    }

    auto profiles = targetSketch->sketch().detectProfiles();

    // Build a profileId string from the first detected profile
    std::string profileId;
    for (size_t i = 0; i < profiles[0].size(); ++i) {
        if (i > 0) profileId += ",";
        profileId += profiles[0][i];
    }

    // Build default params and show the floating feature dialog
    features::ExtrudeParams params;
    params.profileId    = profileId;
    params.sketchId     = targetSketchId;
    params.distanceExpr = "10 mm";
    params.extentType   = features::ExtentType::Distance;
    params.operation    = features::FeatureOperation::NewBody;

    // Highlight the sketch being extruded
    m_viewport->setHighlightedSketch(&targetSketch->sketch());

    m_featureDialog->showExtrude(params);
    showConfirmBar(tr("Extrude — press Enter to apply, Escape to cancel"));
    statusBar()->showMessage(
        tr("Extruding sketch '%1' (%2 profiles found). Adjust distance and press Enter.")
            .arg(QString::fromStdString(targetSketch->name()))
            .arg(profiles.size()));

    connect(m_featureDialog, &FeatureDialog::extrudeAccepted, this,
            [this](features::ExtrudeParams p) {
        m_document->executeCommand(
            std::make_unique<document::AddExtrudeCommand>(std::move(p)));
        m_viewport->setHighlightedSketch(nullptr);
        statusBar()->showMessage(tr("Extruded sketch"));
        hideConfirmBar();
        refreshAllPanels();
    }, Qt::SingleShotConnection);

    connect(m_featureDialog, &FeatureDialog::cancelled, this,
            [this]() {
        m_viewport->setHighlightedSketch(nullptr);
        hideConfirmBar();
        statusBar()->showMessage(tr("Extrude cancelled"));
    }, Qt::SingleShotConnection);
}

void MainWindow::onRevolveSketch()
{
    m_lastCommandName = tr("Revolve");
    m_lastCommandCallback = [this]() { onRevolveSketch(); };
    // Find the last non-suppressed sketch in the timeline
    auto& tl = m_document->timeline();
    features::SketchFeature* lastSketch = nullptr;
    std::string lastSketchId;

    for (size_t i = 0; i < tl.count(); ++i) {
        auto& entry = tl.entry(i);
        if (entry.feature &&
            entry.feature->type() == features::FeatureType::Sketch &&
            !entry.isSuppressed && !entry.isRolledBack) {
            lastSketch = static_cast<features::SketchFeature*>(entry.feature.get());
            lastSketchId = entry.feature->id();
        }
    }

    if (!lastSketch) {
        statusBar()->showMessage(tr("No sketch found. Create a sketch first."), 3000);
        return;
    }

    // Detect profiles in the sketch
    auto profiles = lastSketch->sketch().detectProfiles();
    if (profiles.empty()) {
        statusBar()->showMessage(tr("No closed profiles found in the sketch."), 3000);
        return;
    }

    // Build a profileId string from the first detected profile
    std::string profileId;
    for (size_t i = 0; i < profiles[0].size(); ++i) {
        if (i > 0) profileId += ",";
        profileId += profiles[0][i];
    }

    // Build default params and show the floating feature dialog
    features::RevolveParams params;
    params.profileId        = profileId;
    params.sketchId         = lastSketchId;
    params.axisType         = features::AxisType::ZAxis;
    params.angleExpr        = "360 deg";
    params.isFullRevolution = true;
    params.operation        = features::FeatureOperation::NewBody;

    m_featureDialog->showRevolve(params);
    showConfirmBar(tr("Revolve"));

    connect(m_featureDialog, &FeatureDialog::revolveAccepted, this,
            [this](features::RevolveParams p) {
        m_document->executeCommand(
            std::make_unique<document::AddRevolveCommand>(std::move(p)));
        statusBar()->showMessage(tr("Revolved sketch"));
        hideConfirmBar();
        refreshAllPanels();
    }, Qt::SingleShotConnection);

    connect(m_featureDialog, &FeatureDialog::cancelled, this,
            [this]() {
        hideConfirmBar();
        statusBar()->showMessage(tr("Revolve cancelled"));
    }, Qt::SingleShotConnection);
}

void MainWindow::onAddHole()
{
    m_lastCommandName = tr("Hole");
    m_lastCommandCallback = [this]() { onAddHole(); };
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        statusBar()->showMessage(tr("No bodies found. Create a body first."), 3000);
        return;
    }

    const auto& sel = m_selectionMgr->selection();

    // Selection-driven: if no selection, enter pending mode for face selection
    if (sel.empty()) {
        m_pendingCommand = PendingCommand::Hole;
        m_selectionMgr->setFilter(SelectionFilter::Faces);
        if (m_selectFacesAction)
            m_selectFacesAction->setChecked(true);
        statusBar()->showMessage(
            tr("Select a face for the hole position, then press Enter (Esc to cancel)"));
        return;
    }

    // If a face is selected, use its center as the hole position
    std::string lastBodyId;
    double holePosX = 0, holePosY = 0, holePosZ = 0;

    if (!sel.empty() && sel[0].faceIndex >= 0) {
        lastBodyId = sel[0].bodyId.empty() ? ids.back() : sel[0].bodyId;
        // Use the 3D hit point from the selection as hole position
        holePosX = sel[0].worldX;
        holePosY = sel[0].worldY;
        holePosZ = sel[0].worldZ;
    } else {
        lastBodyId = ids.back();
    }

    // Add a 10mm diameter, 20mm deep hole.
    // If no face was selected, determine the top face center by finding
    // the face with the highest average Z.
    const TopoDS_Shape& shape = brep.getShape(lastBodyId);

    // If no face was pre-selected, find the top face (highest average Z)
    if (!sel.empty() && sel[0].faceIndex < 0) {
        double bestAvgZ = -1e30;
        TopExp_Explorer faceEx(shape, TopAbs_FACE);
        for (; faceEx.More(); faceEx.Next()) {
            const TopoDS_Face& face = TopoDS::Face(faceEx.Current());
            double sumX = 0, sumY = 0, sumZ = 0;
            int count = 0;
            TopExp_Explorer vertEx(face, TopAbs_VERTEX);
            for (; vertEx.More(); vertEx.Next()) {
                gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(vertEx.Current()));
                sumX += p.X();
                sumY += p.Y();
                sumZ += p.Z();
                count++;
            }
            if (count > 0) {
                double avgZ = sumZ / count;
                if (avgZ > bestAvgZ) {
                    bestAvgZ = avgZ;
                    holePosX = sumX / count;
                    holePosY = sumY / count;
                    holePosZ = sumZ / count;
                }
            }
        }
    } else if (sel.empty()) {
        // Fallback: top face search when no selection (shouldn't reach here
        // due to pending mode above, but kept for robustness)
        double bestAvgZ = -1e30;
        TopExp_Explorer faceEx(shape, TopAbs_FACE);
        for (; faceEx.More(); faceEx.Next()) {
            const TopoDS_Face& face = TopoDS::Face(faceEx.Current());
            double sumX = 0, sumY = 0, sumZ = 0;
            int count = 0;
            TopExp_Explorer vertEx(face, TopAbs_VERTEX);
            for (; vertEx.More(); vertEx.Next()) {
                gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(vertEx.Current()));
                sumX += p.X();
                sumY += p.Y();
                sumZ += p.Z();
                count++;
            }
            if (count > 0) {
                double avgZ = sumZ / count;
                if (avgZ > bestAvgZ) {
                    bestAvgZ = avgZ;
                    holePosX = sumX / count;
                    holePosY = sumY / count;
                    holePosZ = sumZ / count;
                }
            }
        }
    }

    // Show the Hole dialog so user can set diameter, depth, type — like Fusion 360
    auto cmd = std::make_unique<document::HoleInteractiveCommand>();
    CommandDialog dlg(cmd.get(), m_document.get(), this);
    dlg.setWindowTitle(tr("Hole"));
    if (dlg.exec() != QDialog::Accepted) {
        statusBar()->showMessage(tr("Hole cancelled"), 2000);
        return;
    }

    auto inputs = dlg.values();
    features::HoleParams params;
    params.targetBodyId = lastBodyId;
    params.holeType     = static_cast<features::HoleType>(inputs.getInt("holeType", 0));
    params.posX = holePosX;
    params.posY = holePosY;
    params.posZ = holePosZ;
    params.dirX = 0;
    params.dirY = 0;
    params.dirZ = -1;

    std::ostringstream diaOss, depOss;
    diaOss << inputs.getNumeric("diameter", 10) << " mm";
    depOss << inputs.getNumeric("depth", 20) << " mm";
    params.diameterExpr = diaOss.str();
    params.depthExpr    = depOss.str();

    try {
        m_document->executeCommand(
            std::make_unique<document::AddHoleCommand>(std::move(params)));
        statusBar()->showMessage(tr("Added hole (%1 dia, %2 deep)")
            .arg(QString::fromStdString(params.diameterExpr),
                 QString::fromStdString(params.depthExpr)));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Hole Failed"),
            tr("Could not create hole: %1").arg(e.what()));
    }

    m_pendingCommand = PendingCommand::None;
    m_selectionMgr->clearSelection();
    refreshAllPanels();
}

// ---------------------------------------------------------------------------
// Helper: collect selected edge indices from the current selection.
// Only edges belonging to the first body encountered are collected.
// ---------------------------------------------------------------------------
std::vector<int> MainWindow::collectSelectedEdges(std::string& bodyIdOut) const
{
    const auto& sel = m_selectionMgr->selection();
    std::vector<int> edges;
    bodyIdOut.clear();

    for (const auto& hit : sel) {
        if (hit.bodyId.empty())
            continue;
        if (bodyIdOut.empty())
            bodyIdOut = hit.bodyId;
        if (hit.bodyId == bodyIdOut && hit.edgeIndex >= 0)
            edges.push_back(hit.edgeIndex);
    }

    // If no edges found but we have a body from a face hit, use that body id
    if (bodyIdOut.empty() && !sel.empty())
        bodyIdOut = sel.front().bodyId;

    return edges;
}

// ---------------------------------------------------------------------------
// Helper: collect selected face indices from the current selection.
// ---------------------------------------------------------------------------
std::vector<int> MainWindow::collectSelectedFaces(std::string& bodyIdOut) const
{
    const auto& sel = m_selectionMgr->selection();
    std::vector<int> faces;
    bodyIdOut.clear();

    for (const auto& hit : sel) {
        if (hit.bodyId.empty())
            continue;
        if (bodyIdOut.empty())
            bodyIdOut = hit.bodyId;
        if (hit.bodyId == bodyIdOut && hit.faceIndex >= 0)
            faces.push_back(hit.faceIndex);
    }

    if (bodyIdOut.empty() && !sel.empty())
        bodyIdOut = sel.front().bodyId;

    return faces;
}

// ---------------------------------------------------------------------------
// Fillet -- selection-aware: uses selected edges, or enters pending mode,
// or falls back to all edges of the last body.
// ---------------------------------------------------------------------------
void MainWindow::onFillet()
{
    m_lastCommandName = tr("Fillet");
    m_lastCommandCallback = [this]() { onFillet(); };
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        statusBar()->showMessage(tr("No bodies found. Create a body first."), 3000);
        return;
    }

    const auto& sel = m_selectionMgr->selection();

    // If no selection at all, enter pending mode so user can pick edges
    if (sel.empty()) {
        m_pendingCommand = PendingCommand::Fillet;
        m_selectionMgr->setFilter(SelectionFilter::Edges);
        if (m_selectEdgesAction)
            m_selectEdgesAction->setChecked(true);
        statusBar()->showMessage(
            tr("Select edges to fillet, then press Enter to confirm (Esc to cancel)"));
        return;
    }

    // Collect edges from selection
    std::string bodyId;
    std::vector<int> edgeIndices = collectSelectedEdges(bodyId);

    // If selection has no body, fall back to last body
    if (bodyId.empty())
        bodyId = ids.back();

    features::FilletParams params;
    params.targetBodyId = bodyId;
    params.edgeIds      = edgeIndices;  // empty = fillet all edges (fallback)
    params.radiusExpr   = "3 mm";

    // Capture stable edge signatures for topology persistence
    if (!edgeIndices.empty()) {
        try {
            const TopoDS_Shape& shape = brep.getShape(bodyId);
            for (int idx : edgeIndices)
                params.edgeSignatures.push_back(
                    kernel::StableReference::computeEdgeSignature(shape, idx));
        } catch (...) {
            // If signature capture fails, proceed without them
        }
    }

    // Show the floating feature dialog for parameter input
    m_featureDialog->showFillet(params);
    showConfirmBar(tr("Fillet"));

    connect(m_featureDialog, &FeatureDialog::filletAccepted, this,
            [this](features::FilletParams p) {
        try {
            m_document->executeCommand(
                std::make_unique<document::AddFilletCommand>(std::move(p)));
            statusBar()->showMessage(tr("Fillet applied"));
        } catch (const std::exception& e) {
            QMessageBox::warning(this, tr("Fillet Failed"),
                tr("Could not fillet: %1").arg(e.what()));
        }
        hideConfirmBar();
        m_pendingCommand = PendingCommand::None;
        m_selectionMgr->clearSelection();
        refreshAllPanels();
    }, Qt::SingleShotConnection);

    connect(m_featureDialog, &FeatureDialog::cancelled, this,
            [this]() {
        hideConfirmBar();
        m_pendingCommand = PendingCommand::None;
        m_selectionMgr->clearSelection();
        statusBar()->showMessage(tr("Fillet cancelled"));
    }, Qt::SingleShotConnection);
}

// ---------------------------------------------------------------------------
// Chamfer -- selection-aware (same pattern as Fillet)
// ---------------------------------------------------------------------------
void MainWindow::onChamfer()
{
    m_lastCommandName = tr("Chamfer");
    m_lastCommandCallback = [this]() { onChamfer(); };
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        statusBar()->showMessage(tr("No bodies found. Create a body first."), 3000);
        return;
    }

    const auto& sel = m_selectionMgr->selection();

    if (sel.empty()) {
        m_pendingCommand = PendingCommand::Chamfer;
        m_selectionMgr->setFilter(SelectionFilter::Edges);
        if (m_selectEdgesAction)
            m_selectEdgesAction->setChecked(true);
        statusBar()->showMessage(
            tr("Select edges to chamfer, then press Enter to confirm (Esc to cancel)"));
        return;
    }

    std::string bodyId;
    std::vector<int> edgeIndices = collectSelectedEdges(bodyId);

    if (bodyId.empty())
        bodyId = ids.back();

    features::ChamferParams params;
    params.targetBodyId = bodyId;
    params.edgeIds      = edgeIndices;  // empty = chamfer all edges
    params.distanceExpr = "2 mm";

    if (!edgeIndices.empty()) {
        try {
            const TopoDS_Shape& shape = brep.getShape(bodyId);
            for (int idx : edgeIndices)
                params.edgeSignatures.push_back(
                    kernel::StableReference::computeEdgeSignature(shape, idx));
        } catch (...) {}
    }

    // Show the floating feature dialog for parameter input
    m_featureDialog->showChamfer(params);
    showConfirmBar(tr("Chamfer"));

    connect(m_featureDialog, &FeatureDialog::chamferAccepted, this,
            [this](features::ChamferParams p) {
        try {
            m_document->executeCommand(
                std::make_unique<document::AddChamferCommand>(std::move(p)));
            statusBar()->showMessage(tr("Chamfer applied"));
        } catch (const std::exception& e) {
            QMessageBox::warning(this, tr("Chamfer Failed"),
                tr("Could not chamfer: %1").arg(e.what()));
        }
        hideConfirmBar();
        m_pendingCommand = PendingCommand::None;
        m_selectionMgr->clearSelection();
        refreshAllPanels();
    }, Qt::SingleShotConnection);

    connect(m_featureDialog, &FeatureDialog::cancelled, this,
            [this]() {
        hideConfirmBar();
        m_pendingCommand = PendingCommand::None;
        m_selectionMgr->clearSelection();
        statusBar()->showMessage(tr("Chamfer cancelled"));
    }, Qt::SingleShotConnection);
}

// ---------------------------------------------------------------------------
// Shell -- selection-aware: uses selected faces as removed faces.
// ---------------------------------------------------------------------------
void MainWindow::onShell()
{
    m_lastCommandName = tr("Shell");
    m_lastCommandCallback = [this]() { onShell(); };
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        statusBar()->showMessage(tr("No bodies found. Create a body first."), 3000);
        return;
    }

    const auto& sel = m_selectionMgr->selection();

    if (sel.empty()) {
        m_pendingCommand = PendingCommand::Shell;
        m_selectionMgr->setFilter(SelectionFilter::Faces);
        if (m_selectFacesAction)
            m_selectFacesAction->setChecked(true);
        statusBar()->showMessage(
            tr("Select faces to remove for shell, then press Enter (Esc to cancel)"));
        return;
    }

    std::string bodyId;
    std::vector<int> faceIndices = collectSelectedFaces(bodyId);

    if (bodyId.empty())
        bodyId = ids.back();

    features::ShellParams params;
    params.targetBodyId   = bodyId;
    params.thicknessExpr  = 2.0;
    params.removedFaceIds = faceIndices;  // empty = auto-detect top face

    // Capture stable face signatures
    if (!faceIndices.empty()) {
        try {
            const TopoDS_Shape& shape = brep.getShape(bodyId);
            for (int idx : faceIndices)
                params.faceSignatures.push_back(
                    kernel::StableReference::computeFaceSignature(shape, idx));
        } catch (...) {}
    }

    // Show the floating feature dialog for parameter input
    m_featureDialog->showShell(params);
    showConfirmBar(tr("Shell"));

    connect(m_featureDialog, &FeatureDialog::shellAccepted, this,
            [this](features::ShellParams p) {
        try {
            m_document->executeCommand(
                std::make_unique<document::AddShellCommand>(std::move(p)));
            statusBar()->showMessage(tr("Shell applied"));
        } catch (const std::exception& e) {
            QMessageBox::warning(this, tr("Shell Failed"),
                tr("Could not shell body: %1").arg(e.what()));
        }
        hideConfirmBar();
        m_pendingCommand = PendingCommand::None;
        m_selectionMgr->clearSelection();
        refreshAllPanels();
    }, Qt::SingleShotConnection);

    connect(m_featureDialog, &FeatureDialog::cancelled, this,
            [this]() {
        hideConfirmBar();
        m_pendingCommand = PendingCommand::None;
        m_selectionMgr->clearSelection();
        statusBar()->showMessage(tr("Shell cancelled"));
    }, Qt::SingleShotConnection);
}

// ---------------------------------------------------------------------------
// Draft -- selection-aware: uses selected faces for draft angle.
// ---------------------------------------------------------------------------
void MainWindow::onDraft()
{
    m_lastCommandName = tr("Draft");
    m_lastCommandCallback = [this]() { onDraft(); };
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        statusBar()->showMessage(tr("No bodies found. Create a body first."), 3000);
        return;
    }

    const auto& sel = m_selectionMgr->selection();

    if (sel.empty()) {
        m_pendingCommand = PendingCommand::Draft;
        m_selectionMgr->setFilter(SelectionFilter::Faces);
        if (m_selectFacesAction)
            m_selectFacesAction->setChecked(true);
        statusBar()->showMessage(
            tr("Select faces to draft, then press Enter (Esc to cancel)"));
        return;
    }

    std::string bodyId;
    std::vector<int> faceIndices = collectSelectedFaces(bodyId);

    if (bodyId.empty())
        bodyId = ids.back();

    if (faceIndices.empty()) {
        statusBar()->showMessage(tr("Please select at least one face to draft."), 3000);
        return;
    }

    features::DraftParams params;
    params.targetBodyId = bodyId;
    params.faceIndices  = faceIndices;
    params.angleExpr    = "3 deg";

    // Capture stable face signatures
    try {
        const TopoDS_Shape& shape = brep.getShape(bodyId);
        for (int idx : faceIndices)
            params.faceSignatures.push_back(
                kernel::StableReference::computeFaceSignature(shape, idx));
    } catch (...) {}

    try {
        m_document->executeCommand(
            std::make_unique<document::AddDraftCommand>(std::move(params)));
        statusBar()->showMessage(
            tr("Drafted %1 face(s) on %2 (3 deg)")
                .arg(faceIndices.size())
                .arg(QString::fromStdString(bodyId)));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Draft Failed"),
            tr("Could not draft: %1").arg(e.what()));
    }

    m_pendingCommand = PendingCommand::None;
    m_selectionMgr->clearSelection();
    refreshAllPanels();
}

// ---------------------------------------------------------------------------
// Pending command: commit / cancel
// ---------------------------------------------------------------------------
void MainWindow::onCommitPendingCommand()
{
    PendingCommand cmd = m_pendingCommand;
    m_pendingCommand = PendingCommand::None;

    switch (cmd) {
    case PendingCommand::Fillet:      onFillet();  break;
    case PendingCommand::Chamfer:     onChamfer(); break;
    case PendingCommand::Shell:       onShell();   break;
    case PendingCommand::Draft:       onDraft();   break;
    case PendingCommand::Hole:        onAddHole(); break;
    case PendingCommand::PressPull:   onPressPull(); break;
    case PendingCommand::SketchPlane: break; // no commit for plane pick
    case PendingCommand::None:        break;
    }
}

void MainWindow::onCancelPendingCommand()
{
    // Cancel joint creator if active
    if (m_jointCreator && m_jointCreator->state() != JointCreator::State::Idle) {
        m_jointCreator->cancel();
    }

    // Dismiss the feature dialog if active
    if (m_featureDialog && m_featureDialog->isActive()) {
        m_featureDialog->dismiss();
    }

    if (m_pendingCommand != PendingCommand::None) {
        m_pendingSketchPlane = false;
        m_pendingCommand = PendingCommand::None;
        m_selectionMgr->clearSelection();
        m_selectionMgr->setFilter(SelectionFilter::All);
        if (m_selectAllAction)
            m_selectAllAction->setChecked(true);
        hideConfirmBar();
        statusBar()->showMessage(tr("Command cancelled"));
    }
}

// =============================================================================
// Press/Pull (Offset Faces)
// =============================================================================

void MainWindow::onPressPull()
{
    m_lastCommandName = tr("Press/Pull");
    m_lastCommandCallback = [this]() { onPressPull(); };
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        statusBar()->showMessage(tr("No bodies found. Create a body first."), 3000);
        return;
    }

    const auto& sel = m_selectionMgr->selection();

    // If no selection, enter pending mode for face selection
    if (sel.empty()) {
        m_pendingCommand = PendingCommand::PressPull;
        m_selectionMgr->setFilter(SelectionFilter::Faces);
        if (m_selectFacesAction)
            m_selectFacesAction->setChecked(true);
        statusBar()->showMessage(
            tr("Select face(s) to push/pull, then press Enter to confirm (Esc to cancel)"));
        return;
    }

    // Collect selected face indices
    std::string bodyId;
    std::vector<int> faceIndices = collectSelectedFaces(bodyId);

    if (bodyId.empty())
        bodyId = ids.back();

    if (faceIndices.empty()) {
        statusBar()->showMessage(tr("No faces selected. Select at least one face."), 3000);
        return;
    }

    features::OffsetFacesParams params;
    params.targetBodyId = bodyId;
    params.faceIndices = faceIndices;
    params.distance = 5.0;

    // Capture stable face signatures
    try {
        const TopoDS_Shape& shape = brep.getShape(bodyId);
        for (int idx : faceIndices)
            params.faceSignatures.push_back(
                kernel::StableReference::computeFaceSignature(shape, idx));
    } catch (...) {}

    // Show the floating feature dialog for parameter input
    m_featureDialog->showPressPull(params);
    showConfirmBar(tr("Press/Pull"));

    connect(m_featureDialog, &FeatureDialog::pressPullAccepted, this,
            [this](features::OffsetFacesParams p) {
        try {
            m_document->executeCommand(
                std::make_unique<document::AddOffsetFacesCommand>(std::move(p)));
            statusBar()->showMessage(tr("Press/Pull applied"));
        } catch (const std::exception& e) {
            QMessageBox::warning(this, tr("Press/Pull Failed"),
                tr("Could not apply press/pull: %1").arg(e.what()));
        }
        hideConfirmBar();
        m_pendingCommand = PendingCommand::None;
        m_selectionMgr->clearSelection();
        refreshAllPanels();
    }, Qt::SingleShotConnection);

    connect(m_featureDialog, &FeatureDialog::cancelled, this,
            [this]() {
        hideConfirmBar();
        m_pendingCommand = PendingCommand::None;
        m_selectionMgr->clearSelection();
        statusBar()->showMessage(tr("Press/Pull cancelled"));
    }, Qt::SingleShotConnection);
}

// =============================================================================
// Construction geometry
// =============================================================================

void MainWindow::onConstructPlane()
{
    m_lastCommandName = tr("Construction Plane");
    m_lastCommandCallback = [this]() { onConstructPlane(); };

    features::ConstructionPlaneParams params;
    params.definitionType = features::PlaneDefinitionType::OffsetFromPlane;
    params.parentPlaneId = "XY";
    params.offsetDistance = 10.0;
    params.originX = 0; params.originY = 0; params.originZ = 10.0;
    params.normalX = 0; params.normalY = 0; params.normalZ = 1;
    params.xDirX = 1; params.xDirY = 0; params.xDirZ = 0;

    // Show the floating feature dialog
    m_featureDialog->showConstructionPlane(params);
    showConfirmBar(tr("Construction Plane"));

    connect(m_featureDialog, &FeatureDialog::constructionPlaneAccepted, this,
            [this](features::ConstructionPlaneParams p) {
        try {
            m_document->executeCommand(
                std::make_unique<document::AddConstructionPlaneCommand>(std::move(p)));
            statusBar()->showMessage(tr("Construction plane created"));
        } catch (const std::exception& e) {
            QMessageBox::warning(this, tr("Construction Plane Failed"),
                tr("Could not create plane: %1").arg(e.what()));
        }
        hideConfirmBar();
        refreshAllPanels();
    }, Qt::SingleShotConnection);

    connect(m_featureDialog, &FeatureDialog::cancelled, this,
            [this]() {
        hideConfirmBar();
        statusBar()->showMessage(tr("Construction plane cancelled"));
    }, Qt::SingleShotConnection);
}

void MainWindow::onConstructAxis()
{
    bool ok = false;
    double x = QInputDialog::getDouble(this, tr("Construction Axis"),
        tr("Direction X:"), 0.0, -1.0, 1.0, 3, &ok);
    if (!ok) return;
    double y = QInputDialog::getDouble(this, tr("Construction Axis"),
        tr("Direction Y:"), 0.0, -1.0, 1.0, 3, &ok);
    if (!ok) return;
    double z = QInputDialog::getDouble(this, tr("Construction Axis"),
        tr("Direction Z:"), 1.0, -1.0, 1.0, 3, &ok);
    if (!ok) return;

    statusBar()->showMessage(
        tr("Construction axis created along (%1, %2, %3)").arg(x).arg(y).arg(z));
}

void MainWindow::onConstructPoint()
{
    bool ok = false;
    double x = QInputDialog::getDouble(this, tr("Construction Point"),
        tr("X coordinate (mm):"), 0.0, -10000, 10000, 2, &ok);
    if (!ok) return;
    double y = QInputDialog::getDouble(this, tr("Construction Point"),
        tr("Y coordinate (mm):"), 0.0, -10000, 10000, 2, &ok);
    if (!ok) return;
    double z = QInputDialog::getDouble(this, tr("Construction Point"),
        tr("Z coordinate (mm):"), 0.0, -10000, 10000, 2, &ok);
    if (!ok) return;

    statusBar()->showMessage(
        tr("Construction point created at (%1, %2, %3)").arg(x).arg(y).arg(z));
}

void MainWindow::onMirrorLastBody()
{
    m_lastCommandName = tr("Mirror");
    m_lastCommandCallback = [this]() { onMirrorLastBody(); };
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        statusBar()->showMessage(tr("No bodies found. Create a body first."), 3000);
        return;
    }

    // Mirror the most recent body about the YZ plane (X=0)
    std::string lastBodyId = ids.back();

    features::MirrorParams params;
    params.targetBodyId = lastBodyId;
    params.planeOx = 0; params.planeOy = 0; params.planeOz = 0;
    params.planeNx = 1; params.planeNy = 0; params.planeNz = 0;  // YZ plane
    params.isCombine = true;

    try {
        m_document->executeCommand(
            std::make_unique<document::AddMirrorCommand>(std::move(params)));
        statusBar()->showMessage(
            tr("Mirrored body: %1 about YZ plane").arg(QString::fromStdString(lastBodyId)));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Mirror Failed"),
            tr("Could not mirror body: %1").arg(e.what()));
    }

    refreshAllPanels();
}

void MainWindow::onCircularPattern()
{
    m_lastCommandName = tr("Circular Pattern");
    m_lastCommandCallback = [this]() { onCircularPattern(); };
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        statusBar()->showMessage(tr("No bodies found. Create a body first."), 3000);
        return;
    }

    // Create 6 copies of the most recent body around the Z axis (full 360 degrees)
    std::string lastBodyId = ids.back();

    features::CircularPatternParams params;
    params.targetBodyId = lastBodyId;
    params.axisOx = 0; params.axisOy = 0; params.axisOz = 0;
    params.axisDx = 0; params.axisDy = 0; params.axisDz = 1;  // Z axis
    params.count = 6;
    params.totalAngleDeg = 360.0;
    params.operation = features::FeatureOperation::Join;

    try {
        m_document->executeCommand(
            std::make_unique<document::AddCircularPatternCommand>(std::move(params)));
        statusBar()->showMessage(
            tr("Circular pattern: %1 (6 copies around Z)").arg(QString::fromStdString(lastBodyId)));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Circular Pattern Failed"),
            tr("Could not create circular pattern: %1").arg(e.what()));
    }

    refreshAllPanels();
}

void MainWindow::onRectangularPattern()
{
    m_lastCommandName = tr("Rectangular Pattern");
    m_lastCommandCallback = [this]() { onRectangularPattern(); };
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        statusBar()->showMessage(tr("No bodies found. Create a body first."), 3000);
        return;
    }

    std::string lastBodyId = ids.back();

    features::RectangularPatternParams params;
    params.targetBodyId = lastBodyId;
    params.dir1X = 1; params.dir1Y = 0; params.dir1Z = 0;  // X direction
    params.spacing1Expr = "30 mm";
    params.count1 = 3;
    params.dir2X = 0; params.dir2Y = 1; params.dir2Z = 0;  // Y direction
    params.spacing2Expr = "30 mm";
    params.count2 = 2;
    params.operation = features::FeatureOperation::Join;

    try {
        m_document->executeCommand(
            std::make_unique<document::AddRectangularPatternCommand>(std::move(params)));
        statusBar()->showMessage(
            tr("Rectangular pattern: %1 (3x2, 30 mm spacing)").arg(QString::fromStdString(lastBodyId)));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Rectangular Pattern Failed"),
            tr("Could not create rectangular pattern: %1").arg(e.what()));
    }

    refreshAllPanels();
}

void MainWindow::onSweepTest()
{
    features::SweepParams params;
    // Empty profileId/pathId triggers the test helix sweep
    params.operation = features::FeatureOperation::NewBody;

    try {
        m_document->executeCommand(
            std::make_unique<document::AddSweepCommand>(std::move(params)));
        statusBar()->showMessage(tr("Created sweep (test helix)"));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Sweep Failed"),
            tr("Could not create sweep: %1").arg(e.what()));
    }
    refreshAllPanels();
}

void MainWindow::onLoftTest()
{
    m_lastCommandName = tr("Loft");
    m_lastCommandCallback = [this]() { onLoftTest(); };
    features::LoftParams params;
    // Empty sectionIds triggers the test circle-to-square loft
    params.operation = features::FeatureOperation::NewBody;

    try {
        m_document->executeCommand(
            std::make_unique<document::AddLoftCommand>(std::move(params)));
        statusBar()->showMessage(tr("Created loft (test sections)"));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Loft Failed"),
            tr("Could not create loft: %1").arg(e.what()));
    }
    refreshAllPanels();
}

void MainWindow::onSweepSketch()
{
    m_lastCommandName = tr("Sweep");
    m_lastCommandCallback = [this]() { onSweepSketch(); };
    // Find the last two sketch features: first = profile, second = path
    auto& tl = m_document->timeline();
    std::vector<std::pair<std::string, features::SketchFeature*>> sketches;

    for (size_t i = 0; i < tl.count(); ++i) {
        auto& entry = tl.entry(i);
        if (entry.feature &&
            entry.feature->type() == features::FeatureType::Sketch &&
            !entry.isSuppressed && !entry.isRolledBack) {
            auto* sf = static_cast<features::SketchFeature*>(entry.feature.get());
            sketches.push_back({entry.feature->id(), sf});
        }
    }

    if (sketches.size() < 2) {
        statusBar()->showMessage(tr("Need at least two sketches (profile + path). Create them first."), 3000);
        return;
    }

    // Use second-to-last as profile, last as path
    auto& [profileSketchId, profileSketch] = sketches[sketches.size() - 2];
    auto& [pathSketchId, pathSketch] = sketches[sketches.size() - 1];

    // Detect profiles in the profile sketch
    auto profiles = profileSketch->sketch().detectProfiles();
    if (profiles.empty()) {
        statusBar()->showMessage(tr("No closed profiles found in the profile sketch."), 3000);
        return;
    }

    // Build profileId from first detected profile
    std::string profileId;
    for (size_t i = 0; i < profiles[0].size(); ++i) {
        if (i > 0) profileId += ",";
        profileId += profiles[0][i];
    }

    // Detect profiles in the path sketch to get path curves
    auto pathProfiles = pathSketch->sketch().detectProfiles();
    if (pathProfiles.empty()) {
        statusBar()->showMessage(tr("No profiles found in the path sketch."), 3000);
        return;
    }

    std::string pathId;
    for (size_t i = 0; i < pathProfiles[0].size(); ++i) {
        if (i > 0) pathId += ",";
        pathId += pathProfiles[0][i];
    }

    features::SweepParams params;
    params.profileId   = profileId;
    params.sketchId    = profileSketchId;
    params.pathId      = pathId;
    params.pathSketchId = pathSketchId;
    params.operation   = features::FeatureOperation::NewBody;

    try {
        m_document->executeCommand(
            std::make_unique<document::AddSweepCommand>(std::move(params)));
        statusBar()->showMessage(tr("Swept sketch along path"));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Sweep Failed"),
            tr("Could not sweep sketch: %1").arg(e.what()));
    }
    refreshAllPanels();
}

void MainWindow::onNewComponent()
{
    auto& reg = m_document->components();

    // Find the currently active component (or root)
    document::Component* active = nullptr;
    for (const auto& id : reg.componentIds()) {
        auto* c = reg.findComponent(id);
        if (c && c->isActive()) {
            active = c;
            break;
        }
    }
    if (!active)
        active = &reg.rootComponent();

    // Create a new child component and add an occurrence to the active component
    std::string newId = reg.createComponent("Component");
    std::string occId = active->addOccurrence(newId);
    m_document->setModified(true);

    statusBar()->showMessage(
        tr("New component created under %1").arg(QString::fromStdString(active->name())));
    refreshAllPanels();
}

void MainWindow::onInsertComponent()
{
    QString filePath = QFileDialog::getOpenFileName(this,
        tr("Insert Component from .kcd"),
        QString(),
        tr("kernelCAD files (*.kcd);;All files (*)"));

    if (filePath.isEmpty())
        return;

    try {
        std::string occId = m_document->insertComponentFromFile(filePath.toStdString());
        statusBar()->showMessage(
            tr("Inserted component from %1 (occurrence %2)")
                .arg(QFileInfo(filePath).fileName())
                .arg(QString::fromStdString(occId)));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Insert Failed"),
            tr("Could not insert component: %1").arg(e.what()));
    }

    refreshAllPanels();
}

void MainWindow::onAddJoint()
{
    // If the joint creator is already active, cancel it
    if (m_jointCreator->state() != JointCreator::State::Idle) {
        m_jointCreator->cancel();
        return;
    }

    // Start interactive face-to-face joint creation (default: Rigid)
    m_jointCreator->begin(features::JointType::Rigid);
}

void MainWindow::onCheckInterference()
{
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();

    if (ids.size() < 2) {
        statusBar()->showMessage(tr("Need at least two bodies to check interference."), 3000);
        return;
    }

    // Build the body list for interference check
    std::vector<std::pair<std::string, TopoDS_Shape>> bodies;
    bodies.reserve(ids.size());
    for (const auto& id : ids) {
        bodies.emplace_back(id, brep.getShape(id));
    }

    auto results = m_document->kernel().checkInterference(bodies);

    if (results.empty()) {
        statusBar()->showMessage(tr("No interference detected between %1 bodies")
            .arg(static_cast<int>(ids.size())));
        statusBar()->showMessage(tr("No interference detected. All bodies are clear."), 3000);
        return;
    }

    // Report results and highlight interfering bodies
    QString report;
    std::vector<int> highlightFaces;
    for (const auto& ir : results) {
        report += tr("Interference between %1 and %2: volume = %3 mm^3\n")
            .arg(QString::fromStdString(ir.body1Id))
            .arg(QString::fromStdString(ir.body2Id))
            .arg(ir.volume, 0, 'f', 3);
    }

    statusBar()->showMessage(tr("%1 interference(s) found").arg(static_cast<int>(results.size())));
    QMessageBox::warning(this, tr("Interference Detected"), report);
}

void MainWindow::onCreateDrawing()
{
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();

    if (ids.empty()) {
        statusBar()->showMessage(tr("No bodies to create a drawing from."), 3000);
        return;
    }

    // Use the first visible body
    TopoDS_Shape shape;
    for (const auto& id : ids) {
        shape = brep.getShape(id);
        if (!shape.IsNull())
            break;
    }

    if (shape.IsNull()) {
        statusBar()->showMessage(tr("Could not find a valid body for drawing."), 3000);
        return;
    }

    // Create a drawing view as a standalone window
    auto* drawing = new DrawingView();
    drawing->setAttribute(Qt::WA_DeleteOnClose);
    drawing->resize(800, 600);
    drawing->setBody(shape);
    drawing->generateStandardViews();

    // Add a simple menu bar for export
    auto* drawingWin = new QMainWindow();
    drawingWin->setAttribute(Qt::WA_DeleteOnClose);
    drawingWin->setWindowTitle(tr("2D Drawing - kernelCAD"));
    drawingWin->setCentralWidget(drawing);
    drawingWin->resize(QSize(820, 660));

    auto* fileMenu = drawingWin->menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("Export &PDF..."), drawing, [drawing]() {
        QString path = QFileDialog::getSaveFileName(
            drawing, QObject::tr("Export PDF"), QString(), QObject::tr("PDF Files (*.pdf)"));
        if (!path.isEmpty())
            drawing->exportPDF(path);
    });
    fileMenu->addAction(tr("Export &SVG..."), drawing, [drawing]() {
        QString path = QFileDialog::getSaveFileName(
            drawing, QObject::tr("Export SVG"), QString(), QObject::tr("SVG Files (*.svg)"));
        if (!path.isEmpty())
            drawing->exportSVG(path);
    });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Close"), drawingWin, &QMainWindow::close);

    drawingWin->show();
    statusBar()->showMessage(tr("2D Drawing created with 4 views."), 3000);
}

void MainWindow::onUndo()
{
    if (!m_document->history().canUndo()) return;
    try {
        // Cancel any active editing/sketch mode before undoing
        if (m_sketchEditor && m_sketchEditor->isEditing())
            m_sketchEditor->finishEditing();
        if (!m_editingFeatureId.isEmpty())
            onFinishEditing();

        std::string desc = m_document->history().undoDescription();
        m_document->history().undo(*m_document);
        statusBar()->showMessage(
            tr("Undo: %1").arg(QString::fromStdString(desc)));
        refreshAllPanels();
    } catch (const std::exception& e) {
        statusBar()->showMessage(tr("Undo failed: %1").arg(e.what()));
    } catch (...) {
        statusBar()->showMessage(tr("Undo failed (unknown error)"));
    }
}

void MainWindow::onRedo()
{
    if (!m_document->history().canRedo()) return;
    try {
        std::string desc = m_document->history().redoDescription();
        m_document->history().redo(*m_document);
        statusBar()->showMessage(
            tr("Redo: %1").arg(QString::fromStdString(desc)));
        refreshAllPanels();
    } catch (const std::exception& e) {
        statusBar()->showMessage(tr("Redo failed: %1").arg(e.what()));
    } catch (...) {
        statusBar()->showMessage(tr("Redo failed (unknown error)"));
    }
}

void MainWindow::updateWindowTitle()
{
    QString title = QStringLiteral("kernelCAD");
    if (!m_document->name().empty())
        title += QStringLiteral(" \u2014 ") + QString::fromStdString(m_document->name());
    if (m_document->isModified())
        title += QStringLiteral(" *");

    // Append current workspace tab name (SOLID / SKETCH / ASSEMBLY)
    if (m_ribbon) {
        QString tabName = m_ribbon->tabText(m_ribbon->currentIndex()).trimmed();
        if (!tabName.isEmpty())
            title += QStringLiteral(" \u2014 ") + tabName;
    }

    setWindowTitle(title);
}

void MainWindow::updateUndoRedoActions()
{
    auto& hist = m_document->history();

    if (m_undoAction) {
        m_undoAction->setEnabled(hist.canUndo());
        if (hist.canUndo())
            m_undoAction->setText(tr("Undo %1").arg(
                QString::fromStdString(hist.undoDescription())));
        else
            m_undoAction->setText(tr("Undo"));
    }

    if (m_redoAction) {
        m_redoAction->setEnabled(hist.canRedo());
        if (hist.canRedo())
            m_redoAction->setText(tr("Redo %1").arg(
                QString::fromStdString(hist.redoDescription())));
        else
            m_redoAction->setText(tr("Redo"));
    }
}

// =============================================================================
// Section plane handlers
// =============================================================================

void MainWindow::onSectionX()
{
    m_sectionAxis = 0;
    m_sectionSlider->setVisible(true);
    m_sectionSlider->setValue(0);
    m_viewport->setSectionPlane(true, 1.0f, 0.0f, 0.0f, 0.0f);
    statusBar()->showMessage(tr("Section View: X axis"));
}

void MainWindow::onSectionY()
{
    m_sectionAxis = 1;
    m_sectionSlider->setVisible(true);
    m_sectionSlider->setValue(0);
    m_viewport->setSectionPlane(true, 0.0f, 1.0f, 0.0f, 0.0f);
    statusBar()->showMessage(tr("Section View: Y axis"));
}

void MainWindow::onSectionZ()
{
    m_sectionAxis = 2;
    m_sectionSlider->setVisible(true);
    m_sectionSlider->setValue(0);
    m_viewport->setSectionPlane(true, 0.0f, 0.0f, 1.0f, 0.0f);
    statusBar()->showMessage(tr("Section View: Z axis"));
}

void MainWindow::onClearSection()
{
    m_sectionAxis = -1;
    m_sectionSlider->setVisible(false);
    m_viewport->setSectionPlane(false);
    statusBar()->showMessage(tr("Section cleared"));
}

void MainWindow::onSectionSliderChanged(int value)
{
    if (m_sectionAxis < 0) return;

    // Map slider range [-1000, 1000] to a world-space offset.
    // Use the bounding box extent to determine the range.
    float scale = 100.0f;  // default total range = +/- 100 mm
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (!ids.empty()) {
        // Compute overall bounding box to size the slider range
        float minVal = 1e9f, maxVal = -1e9f;
        for (const auto& id : ids) {
            auto props = brep.getProperties(id);
            float lo = 0, hi = 0;
            switch (m_sectionAxis) {
            case 0: lo = static_cast<float>(props.bboxMinX); hi = static_cast<float>(props.bboxMaxX); break;
            case 1: lo = static_cast<float>(props.bboxMinY); hi = static_cast<float>(props.bboxMaxY); break;
            case 2: lo = static_cast<float>(props.bboxMinZ); hi = static_cast<float>(props.bboxMaxZ); break;
            }
            if (lo < minVal) minVal = lo;
            if (hi > maxVal) maxVal = hi;
        }
        scale = (maxVal - minVal) * 0.6f;
        if (scale < 1.0f) scale = 1.0f;
    }

    float d = -static_cast<float>(value) / 1000.0f * scale;
    float nx = (m_sectionAxis == 0) ? 1.0f : 0.0f;
    float ny = (m_sectionAxis == 1) ? 1.0f : 0.0f;
    float nz = (m_sectionAxis == 2) ? 1.0f : 0.0f;
    m_viewport->setSectionPlane(true, nx, ny, nz, d);
}

void MainWindow::onSelectionChanged()
{
    // Route selection to JointCreator if it's active
    if (m_jointCreator && m_jointCreator->state() != JointCreator::State::Idle
        && m_selectionMgr->hasSelection()) {
        const auto& hit = m_selectionMgr->selection().front();
        m_jointCreator->onFaceSelected(hit);
        return;
    }

    // Handle pending sketch plane selection
    if (m_pendingSketchPlane && m_selectionMgr->hasSelection()) {
        const auto& hit = m_selectionMgr->selection().front();
        handleSketchPlaneSelection(hit);
        return;
    }

    if (!m_selectionMgr->hasSelection()) {
        m_properties->clear();
        // Clear single-click highlights from tree selection
        if (m_editingFeatureId.isEmpty()) {
            hideManipulator();
            m_viewport->setHighlightedSketch(nullptr);
            m_viewport->setHighlightedFaces({});
        }
        updateStatusBarInfo();
        statusBar()->showMessage(tr("Selection cleared"));
        return;
    }

    const auto& sel = m_selectionMgr->selection();
    const auto& hit = sel.front();

    // Body-level selection (no face or edge): show rich body properties panel
    if (!hit.bodyId.empty() && hit.faceIndex < 0 && hit.edgeIndex < 0) {
        m_properties->showBodyProperties(hit.bodyId);
        updateStatusBarInfo();
        statusBar()->showMessage(
            tr("Selected body %1").arg(QString::fromStdString(hit.bodyId)));
        return;
    }

    // Build a properties display for the selected entity
    std::vector<std::tuple<QString, QString, QVariant>> props;

    if (!hit.bodyId.empty()) {
        props.emplace_back(tr("Body ID"), QStringLiteral("string"),
                           QString::fromStdString(hit.bodyId));

        // Show body material
        const auto& bodyMat = m_document->appearances().bodyMaterial(hit.bodyId);
        props.emplace_back(tr("Body Material"), QStringLiteral("string"),
                           QString::fromStdString(bodyMat.name));
    }

    if (hit.faceIndex >= 0) {
        props.emplace_back(tr("Face Index"), QStringLiteral("int"),
                           hit.faceIndex);

        // Show face material (override or inherited)
        if (!hit.bodyId.empty()) {
            const auto& faceMat = m_document->appearances().faceMaterial(
                hit.bodyId, hit.faceIndex);
            bool hasOverride = m_document->appearances().hasFaceOverride(
                hit.bodyId, hit.faceIndex);
            QString matLabel = QString::fromStdString(faceMat.name);
            if (hasOverride)
                matLabel += QStringLiteral(" (override)");
            props.emplace_back(tr("Face Material"), QStringLiteral("string"),
                               matLabel);
        }
    }

    if (hit.edgeIndex >= 0) {
        props.emplace_back(tr("Edge Index"), QStringLiteral("int"),
                           hit.edgeIndex);
    }

    props.emplace_back(tr("World X"), QStringLiteral("double"),
                       static_cast<double>(hit.worldX));
    props.emplace_back(tr("World Y"), QStringLiteral("double"),
                       static_cast<double>(hit.worldY));
    props.emplace_back(tr("World Z"), QStringLiteral("double"),
                       static_cast<double>(hit.worldZ));
    props.emplace_back(tr("Depth"), QStringLiteral("double"),
                       static_cast<double>(hit.depth));

    // --- B-Rep topology info via BRepQuery ---
    if (!hit.bodyId.empty()) {
        auto& brep = m_document->brepModel();
        if (brep.hasBody(hit.bodyId)) {
            try {
                kernel::BRepQuery bq = brep.query(hit.bodyId);

                // Face-level topology info
                if (hit.faceIndex >= 0 && hit.faceIndex < bq.faceCount()) {
                    kernel::FaceInfo fi = bq.faceInfo(hit.faceIndex);

                    props.emplace_back(tr("--- Face Topology ---"), QStringLiteral("label"),
                                       QString());
                    props.emplace_back(tr("Surface Type"), QStringLiteral("string"),
                                       QString::fromLatin1(kernel::surfaceTypeName(fi.surfaceType)));
                    props.emplace_back(tr("Face Area"), QStringLiteral("string"),
                                       QString::number(fi.area, 'f', 4) + QStringLiteral(" mm2"));
                    props.emplace_back(tr("Normal"), QStringLiteral("string"),
                                       QStringLiteral("(%1, %2, %3)")
                                           .arg(fi.normalX, 0, 'f', 4)
                                           .arg(fi.normalY, 0, 'f', 4)
                                           .arg(fi.normalZ, 0, 'f', 4));
                    props.emplace_back(tr("Centroid"), QStringLiteral("string"),
                                       QStringLiteral("(%1, %2, %3)")
                                           .arg(fi.centroidX, 0, 'f', 4)
                                           .arg(fi.centroidY, 0, 'f', 4)
                                           .arg(fi.centroidZ, 0, 'f', 4));
                    props.emplace_back(tr("Edge Count"), QStringLiteral("int"),
                                       fi.edgeCount);
                    props.emplace_back(tr("Loop Count"), QStringLiteral("int"),
                                       fi.loopCount);
                    props.emplace_back(tr("Reversed"), QStringLiteral("string"),
                                       fi.isReversed ? QStringLiteral("Yes") : QStringLiteral("No"));

                    // Adjacent faces
                    auto adj = bq.adjacentFaces(hit.faceIndex);
                    if (!adj.empty()) {
                        QStringList adjList;
                        for (int idx : adj) adjList.append(QString::number(idx));
                        props.emplace_back(tr("Adjacent Faces"), QStringLiteral("string"),
                                           adjList.join(QStringLiteral(", ")));
                    }
                }

                // Edge-level topology info
                if (hit.edgeIndex >= 0 && hit.edgeIndex < bq.edgeCount()) {
                    kernel::EdgeInfo ei = bq.edgeInfo(hit.edgeIndex);

                    props.emplace_back(tr("--- Edge Topology ---"), QStringLiteral("label"),
                                       QString());
                    props.emplace_back(tr("Curve Type"), QStringLiteral("string"),
                                       QString::fromLatin1(kernel::curveTypeName(ei.curveType)));
                    props.emplace_back(tr("Edge Length"), QStringLiteral("string"),
                                       QString::number(ei.length, 'f', 4) + QStringLiteral(" mm"));
                    props.emplace_back(tr("Convexity"), QStringLiteral("string"),
                                       QString::fromLatin1(kernel::edgeConvexityName(ei.convexity)));
                    props.emplace_back(tr("Start"), QStringLiteral("string"),
                                       QStringLiteral("(%1, %2, %3)")
                                           .arg(ei.startX, 0, 'f', 4)
                                           .arg(ei.startY, 0, 'f', 4)
                                           .arg(ei.startZ, 0, 'f', 4));
                    props.emplace_back(tr("End"), QStringLiteral("string"),
                                       QStringLiteral("(%1, %2, %3)")
                                           .arg(ei.endX, 0, 'f', 4)
                                           .arg(ei.endY, 0, 'f', 4)
                                           .arg(ei.endZ, 0, 'f', 4));
                    if (ei.adjacentFace1 >= 0)
                        props.emplace_back(tr("Adj. Face 1"), QStringLiteral("int"),
                                           ei.adjacentFace1);
                    if (ei.adjacentFace2 >= 0)
                        props.emplace_back(tr("Adj. Face 2"), QStringLiteral("int"),
                                           ei.adjacentFace2);
                    if (ei.isSeam)
                        props.emplace_back(tr("Seam Edge"), QStringLiteral("string"),
                                           QStringLiteral("Yes"));
                    if (ei.isDegenerate)
                        props.emplace_back(tr("Degenerate"), QStringLiteral("string"),
                                           QStringLiteral("Yes"));
                }
            } catch (...) {
                // BRepQuery may throw on degenerate shapes; silently skip topology info
            }

            // Physical properties for the selected body
            auto phys = brep.getProperties(hit.bodyId);

            props.emplace_back(tr("--- Physical Properties ---"), QStringLiteral("label"),
                               QString());

            props.emplace_back(tr("Volume"), QStringLiteral("string"),
                               QString::number(phys.volume, 'f', 4) + QStringLiteral(" mm3"));
            props.emplace_back(tr("Surface Area"), QStringLiteral("string"),
                               QString::number(phys.surfaceArea, 'f', 4) + QStringLiteral(" mm2"));
            props.emplace_back(tr("Mass (steel)"), QStringLiteral("string"),
                               QString::number(phys.mass, 'f', 4) + QStringLiteral(" g"));
            props.emplace_back(tr("CoG X"), QStringLiteral("string"),
                               QString::number(phys.cogX, 'f', 4) + QStringLiteral(" mm"));
            props.emplace_back(tr("CoG Y"), QStringLiteral("string"),
                               QString::number(phys.cogY, 'f', 4) + QStringLiteral(" mm"));
            props.emplace_back(tr("CoG Z"), QStringLiteral("string"),
                               QString::number(phys.cogZ, 'f', 4) + QStringLiteral(" mm"));
            props.emplace_back(tr("Bbox Width (X)"), QStringLiteral("string"),
                               QString::number(phys.bboxMaxX - phys.bboxMinX, 'f', 4) + QStringLiteral(" mm"));
            props.emplace_back(tr("Bbox Height (Y)"), QStringLiteral("string"),
                               QString::number(phys.bboxMaxY - phys.bboxMinY, 'f', 4) + QStringLiteral(" mm"));
            props.emplace_back(tr("Bbox Depth (Z)"), QStringLiteral("string"),
                               QString::number(phys.bboxMaxZ - phys.bboxMinZ, 'f', 4) + QStringLiteral(" mm"));
        }
    }

    m_properties->setProperties(props);

    // Add material dropdown when a body is selected
    if (!hit.bodyId.empty()) {
        const auto& bodyMat = m_document->appearances().bodyMaterial(hit.bodyId);
        m_properties->addMaterialDropdown(
            QString::fromStdString(hit.bodyId),
            QString::fromStdString(bodyMat.name));
    }

    // Status bar
    QString msg;
    if (sel.size() == 1) {
        if (hit.edgeIndex >= 0) {
            msg = tr("Selected edge %1").arg(hit.edgeIndex);
        } else {
            msg = tr("Selected face %1").arg(hit.faceIndex);
        }
        if (!hit.bodyId.empty())
            msg += tr(" on body %1").arg(QString::fromStdString(hit.bodyId));
    } else {
        msg = tr("Selected %1 entities").arg(sel.size());
    }
    statusBar()->showMessage(msg);

    // Refresh status bar center (body count + selection count)
    updateStatusBarInfo();
}

// Map feature type to a consistent colour used in both FeatureTree icons and timeline entries.
static QColor featureTypeColor(features::FeatureType type)
{
    switch (type) {
    case features::FeatureType::Extrude:            return {80, 160, 255};
    case features::FeatureType::Revolve:            return {80, 200, 80};
    case features::FeatureType::Fillet:             return {255, 160, 60};
    case features::FeatureType::Chamfer:            return {255, 140, 40};
    case features::FeatureType::Shell:              return {80, 200, 200};
    case features::FeatureType::Sketch:             return {180, 100, 220};
    case features::FeatureType::Hole:               return {220, 60, 60};
    case features::FeatureType::Sweep:              return {80, 200, 180};
    case features::FeatureType::Loft:               return {80, 200, 180};
    case features::FeatureType::Mirror:             return {100, 180, 255};
    case features::FeatureType::Thread:             return {180, 140, 80};
    case features::FeatureType::Draft:              return {255, 160, 60};
    case features::FeatureType::Combine:            return {220, 200, 60};
    case features::FeatureType::Move:               return {140, 170, 210};
    case features::FeatureType::Scale:              return {160, 120, 200};
    case features::FeatureType::Joint:              return {80, 200, 80};
    case features::FeatureType::Coil:               return {200, 160, 100};
    case features::FeatureType::RectangularPattern: return {100, 220, 210};
    case features::FeatureType::CircularPattern:    return {100, 220, 210};
    case features::FeatureType::PathPattern:        return {100, 220, 210};
    default:                                        return {160, 165, 170};
    }
}

// Build a rich tooltip string (HTML) for a timeline entry.
static QString buildEntryTooltip(const document::TimelineEntry& entry)
{
    if (!entry.feature)
        return QString::fromStdString(entry.displayName());

    QString tip;
    tip += QStringLiteral("<b>") + QString::fromStdString(entry.displayName()) + QStringLiteral("</b><br>");

    // Feature type
    tip += QStringLiteral("Type: ") + QString::fromStdString(entry.feature->name()) + QStringLiteral("<br>");

    // Health state
    auto h = entry.feature->healthState();
    const char* healthStr = "Healthy";
    switch (h) {
    case features::HealthState::Healthy:    healthStr = "Healthy"; break;
    case features::HealthState::Warning:    healthStr = "Warning"; break;
    case features::HealthState::Error:      healthStr = "Error"; break;
    case features::HealthState::Suppressed: healthStr = "Suppressed"; break;
    case features::HealthState::RolledBack: healthStr = "Rolled Back"; break;
    case features::HealthState::Unknown:    healthStr = "Unknown"; break;
    }
    tip += QStringLiteral("Health: ") + QString::fromLatin1(healthStr);

    if (h == features::HealthState::Error && !entry.feature->errorMessage().empty())
        tip += QStringLiteral("<br><span style='color:red'>") +
               QString::fromStdString(entry.feature->errorMessage()) +
               QStringLiteral("</span>");

    // Key parameters per feature type
    auto ft = entry.feature->type();
    if (ft == features::FeatureType::Extrude) {
        auto* ef = dynamic_cast<const features::ExtrudeFeature*>(entry.feature.get());
        if (ef)
            tip += QStringLiteral("<br>Distance: ") + QString::fromStdString(ef->params().distanceExpr);
    } else if (ft == features::FeatureType::Fillet) {
        auto* ff = dynamic_cast<const features::FilletFeature*>(entry.feature.get());
        if (ff)
            tip += QStringLiteral("<br>Radius: ") + QString::fromStdString(ff->params().radiusExpr);
    } else if (ft == features::FeatureType::Chamfer) {
        auto* cf = dynamic_cast<const features::ChamferFeature*>(entry.feature.get());
        if (cf)
            tip += QStringLiteral("<br>Distance: ") + QString::fromStdString(cf->params().distanceExpr);
    } else if (ft == features::FeatureType::Revolve) {
        auto* rf = dynamic_cast<const features::RevolveFeature*>(entry.feature.get());
        if (rf)
            tip += QStringLiteral("<br>Angle: ") + QString::fromStdString(rf->params().angleExpr);
    } else if (ft == features::FeatureType::Hole) {
        auto* hf = dynamic_cast<const features::HoleFeature*>(entry.feature.get());
        if (hf)
            tip += QStringLiteral("<br>Diameter: ") + QString::fromStdString(hf->params().diameterExpr);
    } else if (ft == features::FeatureType::Shell) {
        auto* sf = dynamic_cast<const features::ShellFeature*>(entry.feature.get());
        if (sf)
            tip += QStringLiteral("<br>Thickness: ") +
                   QString::number(sf->params().thicknessExpr, 'f', 2) + QStringLiteral(" mm");
    }

    return tip;
}

void MainWindow::refreshAllPanels()
{
    // Auto-fit viewport when the first body appears (like professional CAD apps)
    {
        auto& brep = m_document->brepModel();
        if (!m_firstBodyFitDone && !brep.bodyIds().empty()) {
            m_firstBodyFitDone = true;
            // Defer fitAll so the mesh is uploaded first
            QTimer::singleShot(0, m_viewport, &Viewport3D::fitAll);
        }
    }

    // Refresh the feature tree
    m_featureTree->refresh();

    // Rebuild timeline entries from the document with rich info
    auto& tl = m_document->timeline();
    std::vector<TimelinePanel::EntryInfo> entries;
    entries.reserve(tl.count());
    for (size_t i = 0; i < tl.count(); ++i) {
        const auto& e = tl.entry(i);
        TimelinePanel::EntryInfo ei;
        ei.id          = QString::fromStdString(e.id);
        ei.displayName = QString::fromStdString(e.displayName());
        ei.tooltip     = buildEntryTooltip(e);
        ei.iconColor   = e.feature ? featureTypeColor(e.feature->type()) : QColor(160, 165, 170);
        ei.suppressed  = e.isSuppressed;
        if (e.feature) {
            ei.featureType = e.feature->type();
            ei.hasError = (e.feature->healthState() == features::HealthState::Error ||
                           e.feature->healthState() == features::HealthState::Warning);
        }
        entries.push_back(std::move(ei));
    }
    m_timeline->setEntriesEx(entries);
    m_timeline->setMarkerPosition(static_cast<int>(tl.markerPosition()));
    m_timeline->setEditingFeatureId(m_editingFeatureId);

    // Refresh the 3D viewport
    updateViewport();

    // If the properties panel is showing a feature, re-read its params
    // (covers undo/redo and any external mutation)
    QString currentFeatId = m_properties->currentFeatureId();
    if (!currentFeatId.isEmpty())
        m_properties->showFeature(currentFeatId);

    // Update undo/redo menu items and window title
    updateUndoRedoActions();
    updateWindowTitle();

    // Update rich status bar (body count, mode, units)
    updateStatusBarInfo();

    // Refresh parameter table
    if (m_parameterTable)
        m_parameterTable->refresh();

    // Notify auto-save that the document changed
    if (m_autoSave)
        m_autoSave->documentChanged();
}

void MainWindow::updateViewport()
{
    // Collect all sketches from timeline for passive rendering
    {
        std::vector<const sketch::Sketch*> passiveSketches;
        auto& tl = m_document->timeline();
        for (size_t i = 0; i < tl.count(); ++i) {
            const auto& entry = tl.entry(i);
            if (entry.feature && !entry.isSuppressed && !entry.isRolledBack &&
                entry.feature->type() == features::FeatureType::Sketch) {
                auto* skFeat = static_cast<const features::SketchFeature*>(entry.feature.get());
                passiveSketches.push_back(&skFeat->sketch());
            }
        }
        m_viewport->setPassiveSketches(passiveSketches);
    }

    // Collect construction planes from timeline for rendering
    {
        std::vector<Viewport3D::ConstructionPlaneData> cplanes;
        auto& tl = m_document->timeline();
        for (size_t i = 0; i < tl.count(); ++i) {
            const auto& entry = tl.entry(i);
            if (entry.feature && !entry.isSuppressed && !entry.isRolledBack &&
                entry.feature->type() == features::FeatureType::ConstructionPlane) {
                auto* cpFeat = static_cast<const features::ConstructionPlane*>(entry.feature.get());
                // Skip standard origin planes (rendered separately)
                if (cpFeat->params().definitionType == features::PlaneDefinitionType::Standard)
                    continue;
                Viewport3D::ConstructionPlaneData cpd;
                double ox, oy, oz;
                cpFeat->origin(ox, oy, oz);
                cpd.originX = static_cast<float>(ox);
                cpd.originY = static_cast<float>(oy);
                cpd.originZ = static_cast<float>(oz);
                double nx, ny, nz;
                cpFeat->normal(nx, ny, nz);
                cpd.normalX = static_cast<float>(nx);
                cpd.normalY = static_cast<float>(ny);
                cpd.normalZ = static_cast<float>(nz);
                double xx, xy, xz;
                cpFeat->xDirection(xx, xy, xz);
                cpd.xDirX = static_cast<float>(xx);
                cpd.xDirY = static_cast<float>(xy);
                cpd.xDirZ = static_cast<float>(xz);
                cpd.label = cpFeat->name();
                cplanes.push_back(cpd);
            }
        }
        m_viewport->setConstructionPlanes(cplanes);
    }

    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty())
        return;

    // Fallback per-body color palette (used when no material is assigned)
    static const float kBodyColors[][3] = {
        {0.6f, 0.65f, 0.7f},   // steel blue-gray (default)
        {0.7f, 0.5f, 0.3f},    // bronze
        {0.3f, 0.7f, 0.4f},    // green
        {0.7f, 0.3f, 0.3f},    // red
        {0.4f, 0.4f, 0.7f},    // purple
        {0.7f, 0.7f, 0.3f},    // gold
    };
    static constexpr size_t kNumBodyColors = sizeof(kBodyColors) / sizeof(kBodyColors[0]);
    const auto& appearances = m_document->appearances();

    std::vector<BodyRenderData> bodies;
    bodies.reserve(ids.size());

    for (size_t bi = 0; bi < ids.size(); ++bi) {
        const auto& id = ids[bi];
        auto mesh = brep.getMesh(id, 0.1);
        auto edgeMesh = brep.getEdgeMesh(id, 0.1);

        BodyRenderData body;
        body.bodyId = id;
        body.vertices = std::move(mesh.vertices);
        body.normals = std::move(mesh.normals);
        body.indices = std::move(mesh.indices);
        body.edgeVertices = std::move(edgeMesh.vertices);
        body.edgeIndices = std::move(edgeMesh.indices);

        // Use material color from AppearanceManager if assigned, otherwise palette
        const auto& mat = appearances.bodyMaterial(id);
        if (appearances.bodyMaterials().count(id)) {
            body.colorR = mat.baseR;
            body.colorG = mat.baseG;
            body.colorB = mat.baseB;
        } else {
            body.colorR = kBodyColors[bi % kNumBodyColors][0];
            body.colorG = kBodyColors[bi % kNumBodyColors][1];
            body.colorB = kBodyColors[bi % kNumBodyColors][2];
        }

        // Visibility from BRepModel
        body.isVisible = brep.isBodyVisible(id);

        // Build per-face triangle IDs by walking OCCT face explorer
        const TopoDS_Shape& shape = brep.getShape(id);
        TopExp_Explorer faceEx(shape, TopAbs_FACE);
        uint32_t localFaceIndex = 0;
        for (; faceEx.More(); faceEx.Next()) {
            const TopoDS_Face& face = TopoDS::Face(faceEx.Current());
            TopLoc_Location loc;
            auto tri = BRep_Tool::Triangulation(face, loc);
            if (tri.IsNull()) continue;

            int faceTris = tri->NbTriangles();
            for (int t = 0; t < faceTris; ++t) {
                body.faceIds.push_back(localFaceIndex);
            }
            ++localFaceIndex;
        }

        bodies.push_back(std::move(body));
    }

    m_viewport->setBodies(bodies);
}

// =============================================================================
// Sketch editing support
// =============================================================================

void MainWindow::setupSketchToolBar()
{
    m_sketchToolBar = new QToolBar(tr("Sketch Tools"), this);
    m_sketchToolBar->setMovable(false);
    m_sketchToolBar->setIconSize(QSize(28, 28));
    m_sketchToolBar->setObjectName("SketchToolBar");
    m_sketchToolBar->setStyleSheet(
        "#SketchToolBar { background: #353535; border: none; spacing: 2px; padding: 2px; }"
        "#SketchToolBar QToolButton { background: transparent; border: 1px solid transparent; border-radius: 4px; padding: 3px; }"
        "#SketchToolBar QToolButton:hover { background: #4a4a4a; border: 1px solid #666; }"
        "#SketchToolBar QToolButton:checked { background-color: #2a82da; border: 2px solid #5cb8ff; color: white; }"
    );

    // Button group ensures exactly one button is checked at a time (radio behavior).
    // We DON'T use setAutoExclusive because QToolBar reparents widgets,
    // breaking Qt's auto-exclusive logic.
    auto* sketchBtnGroup = new QButtonGroup(m_sketchToolBar);
    sketchBtnGroup->setExclusive(true);

    auto addBtn = [this, sketchBtnGroup](const QString& iconName, const QString& tooltip, SketchTool tool) {
        auto* btn = new QToolButton;
        btn->setIcon(IconFactory::createIcon(iconName, 28));
        btn->setToolTip(tooltip);
        btn->setCheckable(true);
        btn->setProperty("_sketchTool", static_cast<int>(tool));
        sketchBtnGroup->addButton(btn, static_cast<int>(tool));
        connect(btn, &QToolButton::clicked, [this, tool]() {
            m_sketchEditor->setTool(tool);
        });
        m_sketchToolBar->addWidget(btn);
        return btn;
    };

    // Draw tools
    addBtn("line", "Line (L)", SketchTool::DrawLine);
    addBtn("rectangle", "Rectangle (R)", SketchTool::DrawRectangle);
    addBtn("center_rectangle", "Center Rectangle", SketchTool::DrawRectangleCenter);
    addBtn("circle", "Circle (C)", SketchTool::DrawCircle);
    addBtn("circle_3point", "3-Point Circle", SketchTool::DrawCircle3Point);
    addBtn("arc", "Arc (A)", SketchTool::DrawArc);
    addBtn("arc_3point", "3-Point Arc", SketchTool::DrawArc3Point);
    addBtn("ellipse", "Ellipse", SketchTool::DrawEllipse);
    addBtn("polygon", "Polygon", SketchTool::DrawPolygon);
    addBtn("slot", "Slot", SketchTool::DrawSlot);
    addBtn("spline", "Spline (S)", SketchTool::DrawSpline);

    m_sketchToolBar->addSeparator();

    // Modify tools
    addBtn("trim", "Trim (T)", SketchTool::Trim);
    addBtn("extend", "Extend (E)", SketchTool::Extend);
    addBtn("offset", "Offset (O)", SketchTool::Offset);
    addBtn("project", "Project Edge (P)", SketchTool::ProjectEdge);
    addBtn("fillet_sketch", "Fillet (F)", SketchTool::SketchFillet);
    addBtn("chamfer_sketch", "Chamfer (G)", SketchTool::SketchChamfer);

    m_sketchToolBar->addSeparator();

    // Constraint tools
    addBtn("dimension", "Dimension (D)", SketchTool::Dimension);
    addBtn("constraint", "Constraint (K)", SketchTool::AddConstraint);

    m_sketchToolBar->addSeparator();

    // Construction toggle (checkable, not auto-exclusive)
    auto* constructionBtn = new QToolButton;
    constructionBtn->setIcon(IconFactory::createIcon("construction", 28));
    constructionBtn->setToolTip("Toggle Construction (X)");
    constructionBtn->setCheckable(true);
    connect(constructionBtn, &QToolButton::toggled, [this](bool on) {
        m_sketchEditor->setConstructionMode(on);
    });
    m_sketchToolBar->addWidget(constructionBtn);

    m_sketchToolBar->addSeparator();

    // Select (pointer mode)
    addBtn("select", "Select", SketchTool::None);

    m_sketchToolBar->addSeparator();

    // Finish sketch
    auto* finishBtn = new QToolButton;
    finishBtn->setIcon(IconFactory::createIcon("finish", 28));
    finishBtn->setToolTip("Finish Sketch (Esc)");
    finishBtn->setStyleSheet(
        "QToolButton { background: #2a5a2a; border-radius: 4px; padding: 4px; }"
        "QToolButton:hover { background: #3a7a3a; }"
    );
    connect(finishBtn, &QToolButton::clicked, [this]() {
        m_sketchEditor->finishEditing();
    });
    m_sketchToolBar->addWidget(finishBtn);

    addToolBar(Qt::TopToolBarArea, m_sketchToolBar);
    m_sketchToolBar->setVisible(false);
}

void MainWindow::showSketchToolBar(bool visible)
{
    if (m_sketchToolBar)
        m_sketchToolBar->setVisible(visible);
}

void MainWindow::beginSketchEditing(features::SketchFeature* sketchFeat)
{
    if (!sketchFeat)
        return;

    m_activeSketchFeature = sketchFeat;

    // Ensure the sketch has origin geometry (fixed origin point + construction axes)
    // This matches Fusion 360 where every sketch shows the origin as pickable entities.
    auto& sk = sketchFeat->sketch();
    bool hasOriginPt = false;
    for (const auto& [pid, pt] : sk.points()) {
        if (pt.isFixed && std::abs(pt.x) < 0.01 && std::abs(pt.y) < 0.01) {
            hasOriginPt = true;
            break;
        }
    }
    if (!hasOriginPt) {
        // Add fixed origin point at (0,0)
        sk.addPoint(0, 0, true);
        // Add construction X and Y axis lines through origin
        auto xPt = sk.addPoint(100, 0, true);
        auto yPt = sk.addPoint(0, 100, true);
        auto oxPt = sk.addPoint(0, 0, true);  // shared origin for axes
        // Use the first fixed origin point
        for (const auto& [pid, pt] : sk.points()) {
            if (pt.isFixed && std::abs(pt.x) < 0.01 && std::abs(pt.y) < 0.01) {
                auto xLine = sk.addLine(pid, xPt, true);  // construction
                auto yLine = sk.addLine(pid, yPt, true);  // construction
                break;
            }
        }
    }

    m_sketchEditor->beginEditing(&sk, m_viewport);
    m_sketchEditor->setTool(SketchTool::DrawLine);  // default to line tool

    // Save camera state and animate to face the sketch plane head-on
    m_viewport->saveCameraState();

    double nx, ny, nz;
    sketchFeat->sketch().planeNormal(nx, ny, nz);
    double ox, oy, oz;
    sketchFeat->sketch().planeOrigin(ox, oy, oz);

    QVector3D normal(static_cast<float>(nx), static_cast<float>(ny), static_cast<float>(nz));
    QVector3D origin(static_cast<float>(ox), static_cast<float>(oy), static_cast<float>(oz));
    QVector3D targetEye = origin + normal * m_viewport->orbitDistance();
    QVector3D targetUp(0.0f, 1.0f, 0.0f);
    // If looking along Y axis, use Z as up to avoid gimbal lock
    if (std::abs(static_cast<float>(ny)) > 0.9f)
        targetUp = QVector3D(0.0f, 0.0f, -1.0f);

    m_viewport->animateTo(targetEye, origin, targetUp, 400);

    // Ghost bodies and lock rotation during sketch editing
    m_viewport->setSketchMode(true);

    // Disable feature shortcuts that conflict with sketch tool keys
    // (E=extend, F=fillet, S=spline, C=circle, etc.)
    if (m_extrudeAction)  m_extrudeAction->setEnabled(false);
    if (m_filletAction)   m_filletAction->setEnabled(false);
    if (m_holeAction)     m_holeAction->setEnabled(false);
    if (m_jointAction)    m_jointAction->setEnabled(false);
    if (m_measureAction)  m_measureAction->setEnabled(false);
    if (m_deleteAction)   m_deleteAction->setEnabled(false);

    showSketchToolBar(true);
    // Auto-switch ribbon to SKETCH tab
    if (m_ribbon) m_ribbon->setCurrentIndex(m_sketchTabIndex);
    showConfirmBar(tr("Sketch: Line"));
    statusBar()->showMessage(tr("Sketch Mode \u2014 L:Line  R:Rect  C:Circle  A:Arc  D:Dim  T:Trim  X:Construction  Esc:Finish"));

    // Show sketch palettes in the properties panel
    m_properties->showSketchPalettes(&sketchFeat->sketch(), m_sketchEditor);
}

void MainWindow::onSketchEditingFinished()
{
    // Clear sketch palettes from the properties panel
    m_properties->clear();

    // Restore bodies to full opacity and re-enable rotation
    m_viewport->setSketchMode(false);

    // Restore camera to pre-sketch position (smooth animation)
    m_viewport->restoreCameraState(/*animate=*/true);

    showSketchToolBar(false);
    hideConfirmBar();
    // Auto-switch ribbon back to SOLID tab
    if (m_ribbon) m_ribbon->setCurrentIndex(m_solidTabIndex);

    // Re-enable feature shortcuts that were disabled during sketch editing
    if (m_extrudeAction)  m_extrudeAction->setEnabled(true);
    if (m_filletAction)   m_filletAction->setEnabled(true);
    if (m_holeAction)     m_holeAction->setEnabled(true);
    if (m_jointAction)    m_jointAction->setEnabled(true);
    if (m_measureAction)  m_measureAction->setEnabled(true);
    if (m_deleteAction)   m_deleteAction->setEnabled(true);

    if (m_activeSketchFeature) {
        // Solve the sketch one final time
        m_activeSketchFeature->sketch().solve();

        // If the sketch has closed profiles, suggest extruding
        auto profiles = m_activeSketchFeature->sketch().detectProfiles();
        m_activeSketchFeature = nullptr;

        refreshAllPanels();

        if (!profiles.empty()) {
            statusBar()->showMessage(
                tr("Sketch has %1 profile(s). Press E to extrude.")
                    .arg(profiles.size()), 5000);
        } else {
            statusBar()->showMessage(tr("Sketch editing finished"));
        }
    } else {
        refreshAllPanels();
        statusBar()->showMessage(tr("Sketch editing finished"));
    }
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

// =============================================================================
// Measure tool
// =============================================================================

void MainWindow::onMeasure()
{
    m_lastCommandName = tr("Measure");
    m_lastCommandCallback = [this]() { onMeasure(); };
    if (m_measureActive) {
        // Toggle off
        m_measureActive = false;
        m_measureTool->reset();
        statusBar()->showMessage(tr("Measure tool deactivated"));
        return;
    }

    m_measureActive = true;
    m_measureTool->reset();
    m_measureTool->setMode(MeasureTool::MeasureMode::PointToPoint);

    // Wire up selection clicks to the measure tool while active
    m_selectionMgr->setOnSelectionChanged([this](const std::vector<SelectionHit>& sel) {
        if (!m_measureActive || sel.empty()) {
            onSelectionChanged();
            return;
        }

        const auto& hit = sel.front();

        if (!m_measureTool->hasFirstEntity()) {
            m_measureTool->setFirstEntity(hit);
            statusBar()->showMessage(tr("Measure: pick second point (M to cancel)"));
        } else {
            m_measureTool->setSecondEntity(hit);
            // Result is emitted via measurementReady signal
            // Reset for next measurement
            m_measureTool->reset();
            statusBar()->showMessage(tr("Measure: pick first point (M to cancel)"));
        }
    });

    statusBar()->showMessage(tr("Measure: pick first point (M to cancel)"));
}

void MainWindow::executeInteractiveCommand(std::unique_ptr<document::InteractiveCommand> cmd)
{
    CommandDialog dlg(cmd.get(), m_document.get(), this);
    if (dlg.exec() == QDialog::Accepted) {
        cmd->setInputValues(dlg.values());
        m_document->executeCommand(std::move(cmd));
        refreshAllPanels();
    }
}

// =============================================================================
// Delete selected feature
// =============================================================================

void MainWindow::onDeleteSelectedFeature()
{
    // If something is selected in the viewport, try to find the corresponding feature.
    // For now, delete the last feature in the timeline (most common workflow).
    if (!m_selectionMgr->hasSelection()) {
        statusBar()->showMessage(tr("Nothing selected to delete"), 3000);
        return;
    }

    // Try to find a body-level feature from the selection
    const auto& hit = m_selectionMgr->selection().front();
    if (hit.bodyId.empty()) {
        statusBar()->showMessage(tr("No feature associated with selection"), 3000);
        return;
    }

    // The bodyId often corresponds to a feature id in the timeline
    QString featureId = QString::fromStdString(hit.bodyId);
    m_document->executeCommand(
        std::make_unique<document::DeleteFeatureCommand>(featureId.toStdString()));
    m_selectionMgr->clearSelection();
    m_properties->clear();
    refreshAllPanels();
    statusBar()->showMessage(tr("Feature deleted"));
}

// =============================================================================
// Viewport right-click context menu
// =============================================================================

void MainWindow::showViewportContextMenu(const QPoint& globalPos)
{
    QMenu menu(this);

    // "Repeat [last command]" — always first, matching professional CAD UX
    if (!m_lastCommandName.isEmpty() && m_lastCommandCallback) {
        menu.addAction(tr("Repeat %1").arg(m_lastCommandName), this, m_lastCommandCallback);
        menu.addSeparator();
    }

    if (!m_selectionMgr->hasSelection()) {
        // ── Right-click on empty space ──────────────────────────────────
        menu.addAction(tr("Fit All"), this, [this]() {
            m_viewport->fitAll();
            statusBar()->showMessage(tr("Fit All"));
        });

        // Standard Views submenu (all 7 standard views)
        auto* viewsMenu = menu.addMenu(tr("Standard Views"));
        viewsMenu->addAction(tr("Front"), this, [this]() {
            m_viewport->setStandardView(StandardView::Front);
        });
        viewsMenu->addAction(tr("Back"), this, [this]() {
            m_viewport->setStandardView(StandardView::Back);
        });
        viewsMenu->addAction(tr("Left"), this, [this]() {
            m_viewport->setStandardView(StandardView::Left);
        });
        viewsMenu->addAction(tr("Right"), this, [this]() {
            m_viewport->setStandardView(StandardView::Right);
        });
        viewsMenu->addAction(tr("Top"), this, [this]() {
            m_viewport->setStandardView(StandardView::Top);
        });
        viewsMenu->addAction(tr("Bottom"), this, [this]() {
            m_viewport->setStandardView(StandardView::Bottom);
        });
        viewsMenu->addAction(tr("Isometric"), this, [this]() {
            m_viewport->setStandardView(StandardView::Isometric);
        });

        menu.addSeparator();

        // Visual Style submenu (all 4 view modes)
        auto* viewModeMenu = menu.addMenu(tr("Visual Style"));
        auto* solidEdgesAct = viewModeMenu->addAction(tr("Solid + Edges"));
        solidEdgesAct->setCheckable(true);
        solidEdgesAct->setChecked(m_viewport->viewMode() == ViewMode::SolidWithEdges);
        connect(solidEdgesAct, &QAction::triggered, this, [this]() {
            m_viewport->setViewMode(ViewMode::SolidWithEdges);
        });
        auto* solidAct = viewModeMenu->addAction(tr("Solid"));
        solidAct->setCheckable(true);
        solidAct->setChecked(m_viewport->viewMode() == ViewMode::Solid);
        connect(solidAct, &QAction::triggered, this, [this]() {
            m_viewport->setViewMode(ViewMode::Solid);
        });
        auto* wireAct = viewModeMenu->addAction(tr("Wireframe"));
        wireAct->setCheckable(true);
        wireAct->setChecked(m_viewport->viewMode() == ViewMode::Wireframe);
        connect(wireAct, &QAction::triggered, this, [this]() {
            m_viewport->setViewMode(ViewMode::Wireframe);
        });
        auto* hiddenAct = viewModeMenu->addAction(tr("Hidden Line"));
        hiddenAct->setCheckable(true);
        hiddenAct->setChecked(m_viewport->viewMode() == ViewMode::HiddenLine);
        connect(hiddenAct, &QAction::triggered, this, [this]() {
            m_viewport->setViewMode(ViewMode::HiddenLine);
        });

        menu.addSeparator();

        // Toggle Grid
        auto* gridAction = menu.addAction(tr("Toggle Grid"));
        gridAction->setCheckable(true);
        gridAction->setChecked(m_viewport->showGrid());
        connect(gridAction, &QAction::toggled, this, [this](bool checked) {
            m_viewport->setShowGrid(checked);
            statusBar()->showMessage(checked ? tr("Grid visible") : tr("Grid hidden"));
        });

        menu.addSeparator();
        menu.addAction(tr("Create Sketch"), this, &MainWindow::onCreateSketch);
        menu.addAction(tr("Create Box"),    this, &MainWindow::onCreateBox);

        menu.exec(globalPos);
        return;
    }

    const auto& hit = m_selectionMgr->selection().front();
    bool hasFace = (hit.faceIndex >= 0);
    bool hasEdge = (hit.edgeIndex >= 0);

    if (hasFace) {
        // ── Right-click on face ─────────────────────────────────────────
        menu.addAction(tr("Press/Pull"), this, &MainWindow::onPressPull);
        menu.addAction(tr("Create Sketch on Face"), this, &MainWindow::onCreateSketch);
        menu.addAction(tr("Extrude"),               this, &MainWindow::onExtrudeSketch);
        menu.addSeparator();
        menu.addAction(tr("Fillet"),                this, &MainWindow::onFillet);
        menu.addAction(tr("Chamfer"),               this, &MainWindow::onChamfer);
        menu.addAction(tr("Shell (remove this face)"), this, &MainWindow::onShell);
        menu.addAction(tr("Draft"),                 this, &MainWindow::onDraft);
        menu.addAction(tr("Hole"),                  this, &MainWindow::onAddHole);
        menu.addSeparator();
        menu.addAction(tr("Appearance"), this, [this]() {
            statusBar()->showMessage(tr("Appearance \u2014 not yet implemented"));
        });
        menu.addAction(tr("Measure"), this, &MainWindow::onMeasure);
        menu.addSeparator();
        menu.addAction(tr("Delete"), this, &MainWindow::onDeleteSelectedFeature);
    } else if (hasEdge) {
        // ── Right-click on edge ─────────────────────────────────────────
        menu.addAction(tr("Fillet this Edge"),  this, &MainWindow::onFillet);
        menu.addAction(tr("Chamfer this Edge"), this, &MainWindow::onChamfer);
        menu.addSeparator();
        menu.addAction(tr("Measure"), this, &MainWindow::onMeasure);
        menu.addSeparator();
        menu.addAction(tr("Delete"), this, &MainWindow::onDeleteSelectedFeature);
    } else {
        // Body selected (no specific sub-entity) — fallback
        menu.addAction(tr("Mirror"),       this, &MainWindow::onMirrorLastBody);
        menu.addAction(tr("Rect Pattern"), this, &MainWindow::onRectangularPattern);
        menu.addAction(tr("Circ Pattern"), this, &MainWindow::onCircularPattern);
        menu.addAction(tr("Shell"),        this, &MainWindow::onShell);
        menu.addAction(tr("Hole"),         this, &MainWindow::onAddHole);
        menu.addSeparator();
        menu.addAction(tr("Measure"), this, &MainWindow::onMeasure);
        menu.addSeparator();
        menu.addAction(tr("Delete"), this, &MainWindow::onDeleteSelectedFeature);
    }

    menu.exec(globalPos);
}

// =============================================================================
// Confirmation toolbar (floating OK/Cancel during active commands)
// =============================================================================

void MainWindow::setupConfirmBar()
{
    m_confirmBar = new QWidget(m_viewport);
    m_confirmBar->setObjectName("ConfirmBar");

    auto* layout = new QHBoxLayout(m_confirmBar);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(8);

    m_confirmLabel = new QLabel(m_confirmBar);
    m_confirmLabel->setStyleSheet("color: #ddd; font-weight: bold; font-size: 12px;");

    m_confirmOkBtn = new QPushButton(m_confirmBar);
    m_confirmOkBtn->setText(tr("OK"));
    m_confirmOkBtn->setFixedSize(60, 28);
    m_confirmOkBtn->setStyleSheet(
        "QPushButton { background: #2a7d2a; color: white; border: none; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background: #38a838; }");

    m_confirmCancelBtn = new QPushButton(m_confirmBar);
    m_confirmCancelBtn->setText(tr("Cancel"));
    m_confirmCancelBtn->setFixedSize(60, 28);
    m_confirmCancelBtn->setStyleSheet(
        "QPushButton { background: #8a2a2a; color: white; border: none; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background: #b83838; }");

    layout->addWidget(m_confirmLabel);
    layout->addWidget(m_confirmOkBtn);
    layout->addWidget(m_confirmCancelBtn);

    m_confirmBar->setStyleSheet(
        "QWidget#ConfirmBar { background: rgba(30, 30, 30, 210); border-radius: 8px; }");
    m_confirmBar->adjustSize();
    m_confirmBar->setVisible(false);

    // Wire up buttons
    connect(m_confirmOkBtn, &QPushButton::clicked, this, [this]() {
        if (m_sketchEditor && m_sketchEditor->isEditing()) {
            m_sketchEditor->finishEditing();
        } else if (!m_editingFeatureId.isEmpty()) {
            onEditingCommitted(m_editingFeatureId);
        } else if (m_pendingCommand != PendingCommand::None) {
            onCommitPendingCommand();
        }
    });

    connect(m_confirmCancelBtn, &QPushButton::clicked, this, [this]() {
        if (m_sketchEditor && m_sketchEditor->isEditing()) {
            m_sketchEditor->finishEditing();
        } else if (!m_editingFeatureId.isEmpty()) {
            onEditingCancelled(m_editingFeatureId);
        } else if (m_pendingCommand != PendingCommand::None) {
            onCancelPendingCommand();
        }
    });
}

void MainWindow::showConfirmBar(const QString& toolName)
{
    if (!m_confirmBar)
        return;
    m_confirmLabel->setText(toolName);
    m_confirmBar->adjustSize();

    // Position at bottom-center of the viewport
    int barW = m_confirmBar->width();
    int barH = m_confirmBar->height();
    int vpW  = m_viewport->width();
    int vpH  = m_viewport->height();
    m_confirmBar->move((vpW - barW) / 2, vpH - barH - 20);
    m_confirmBar->raise();
    // Floating confirm bar disabled -- keyboard hints are shown in the status
    // bar instead (Enter/Escape).  The widget is kept alive so button
    // connections remain valid; it simply never becomes visible.
    // m_confirmBar->setVisible(true);
}

void MainWindow::hideConfirmBar()
{
    if (m_confirmBar)
        m_confirmBar->setVisible(false);
}

// =============================================================================
// Rich status bar (body count, mode, units)
// =============================================================================

void MainWindow::setupStatusBar()
{
    m_statusLeft   = new QLabel(this);
    m_statusCenter = new QLabel(this);
    m_statusRight  = new QLabel(this);

    m_statusLeft->setStyleSheet("padding-left: 8px; color: white;");
    m_statusCenter->setAlignment(Qt::AlignCenter);
    m_statusCenter->setStyleSheet("color: rgba(255,255,255,0.85);");
    m_statusRight->setAlignment(Qt::AlignRight);
    m_statusRight->setStyleSheet("padding-right: 8px; color: rgba(255,255,255,0.85);");

    statusBar()->addWidget(m_statusLeft, 1);
    statusBar()->addWidget(m_statusCenter, 1);
    statusBar()->addPermanentWidget(m_statusRight);

    m_statusLeft->setText(tr("Ready"));
    m_statusCenter->setText(tr("0 bodies, 0 selected"));
    m_statusRight->setText(tr("Grid: On | Solid+Edges | Persp"));
}

void MainWindow::updateStatusBarInfo()
{
    if (!m_document)
        return;

    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    int bodyCount = static_cast<int>(ids.size());

    // --- Left: Current status / active command ---
    QString leftText;
    if (m_sketchEditor && m_sketchEditor->isEditing())
        leftText = tr("Sketch Mode \u2014 L:Line R:Rect C:Circle Esc:Finish");
    else if (!m_editingFeatureId.isEmpty())
        leftText = tr("Editing \u2014 Enter:OK  Esc:Cancel");
    else if (m_pendingCommand != PendingCommand::None)
        leftText = tr("Select geometry\u2026  Enter:OK  Esc:Cancel");
    else {
        const auto& sel = m_selectionMgr->selection();
        if (!sel.empty() && !sel[0].bodyId.empty()) {
            const auto& hit = sel[0];
            if (hit.faceIndex >= 0) {
                leftText = tr("Selected: Face %1 on %2").arg(hit.faceIndex)
                               .arg(QString::fromStdString(hit.bodyId));
                try {
                    auto q = brep.query(hit.bodyId);
                    auto fi = q.faceInfo(hit.faceIndex);
                    leftText += QStringLiteral(" (")
                              + QString::fromLatin1(kernel::surfaceTypeName(fi.surfaceType))
                              + QStringLiteral(")");
                } catch (...) {}
            } else if (hit.edgeIndex >= 0) {
                leftText = tr("Selected: Edge %1 on %2").arg(hit.edgeIndex)
                               .arg(QString::fromStdString(hit.bodyId));
                try {
                    auto q = brep.query(hit.bodyId);
                    auto ei = q.edgeInfo(hit.edgeIndex);
                    leftText += QStringLiteral(" (")
                              + QString::fromLatin1(kernel::curveTypeName(ei.curveType))
                              + QStringLiteral(")");
                } catch (...) {}
            } else {
                leftText = tr("Selected: %1").arg(QString::fromStdString(hit.bodyId));
            }
        } else {
            leftText = tr("Ready");
        }
    }
    m_statusLeft->setText(leftText);

    // --- Center: body count + selection count ---
    int selCount = static_cast<int>(m_selectionMgr->selection().size());
    m_statusCenter->setText(
        tr("%1 %2, %3 selected")
            .arg(bodyCount)
            .arg(bodyCount == 1 ? tr("body") : tr("bodies"))
            .arg(selCount));

    // --- Right: Grid + View mode + Projection ---
    bool gridOn = m_viewport->showGrid();

    QString viewModeName;
    switch (m_viewport->viewMode()) {
    case ViewMode::SolidWithEdges: viewModeName = tr("Solid+Edges"); break;
    case ViewMode::Solid:          viewModeName = tr("Solid");       break;
    case ViewMode::Wireframe:      viewModeName = tr("Wireframe");   break;
    default:                       viewModeName = tr("Solid+Edges"); break;
    }

    QString projName = m_viewport->isPerspective() ? tr("Persp") : tr("Ortho");

    m_statusRight->setText(
        tr("Grid: %1 | %2 | %3")
            .arg(gridOn ? tr("On") : tr("Off"))
            .arg(viewModeName)
            .arg(projName));
}

// =============================================================================
// Marking menu (radial context menu on right-click-and-hold)
// =============================================================================

void MainWindow::setupMarkingMenu()
{
    m_markingMenu = new MarkingMenu(nullptr);  // popup window, no parent

    // Timer: fires after 200ms of right-button hold to trigger the marking menu.
    m_markingMenuTimer = new QTimer(this);
    m_markingMenuTimer->setSingleShot(true);
    m_markingMenuTimer->setInterval(200);
    connect(m_markingMenuTimer, &QTimer::timeout, this, [this]() {
        m_markingMenuShown = true;
        showMarkingMenuForContext(m_rightClickPos);
    });

    // Install an event filter on the viewport to intercept right-click events.
    m_viewport->installEventFilter(this);
}

void MainWindow::showMarkingMenuForContext(const QPoint& globalPos)
{
    std::vector<MarkingMenu::MenuItem> items;

    bool inSketch = m_sketchEditor && m_sketchEditor->isEditing();

    if (inSketch) {
        // Sketch mode items: N=Line, NE=Dimension, E=Circle, SE=Trim, S=Rectangle, SW=Finish, W=Arc, NW=Offset
        items.push_back({"Line", "L", {}, [this]() {
            m_sketchEditor->setTool(SketchTool::DrawLine);
        }});
        items.push_back({"Dimension", "D", {}, [this]() {
            m_sketchEditor->setTool(SketchTool::Dimension);
        }});
        items.push_back({"Circle", "C", {}, [this]() {
            m_sketchEditor->setTool(SketchTool::DrawCircle);
        }});
        items.push_back({"Trim", "T", {}, [this]() {
            m_sketchEditor->setTool(SketchTool::Trim);
        }});
        items.push_back({"Rectangle", "R", {}, [this]() {
            m_sketchEditor->setTool(SketchTool::DrawRectangle);
        }});
        items.push_back({"Finish Sketch", "", {}, [this]() {
            m_sketchEditor->finishEditing();
        }});
        items.push_back({"Arc", "A", {}, [this]() {
            m_sketchEditor->setTool(SketchTool::DrawArc);
        }});
        items.push_back({"Offset", "O", {}, [this]() {
            m_sketchEditor->setTool(SketchTool::Offset);
        }});
    } else if (m_selectionMgr->hasSelection()) {
        const auto& hit = m_selectionMgr->selection().front();
        bool hasFace = (hit.faceIndex >= 0);
        bool hasEdge = (hit.edgeIndex >= 0);

        if (hasFace) {
            // Face selected: N=Extrude, NE=Chamfer, E=Fillet, SE=Hole, S=Shell, SW=Select Loop, W=Draft, NW=Appearance
            items.push_back({"Extrude", "E", {}, [this]() { onExtrudeSketch(); }});
            items.push_back({"Chamfer", "", {}, [this]() { onChamfer(); }});
            items.push_back({"Fillet", "F", {}, [this]() { onFillet(); }});
            items.push_back({"Hole", "H", {}, [this]() { onAddHole(); }});
            items.push_back({"Shell", "", {}, [this]() { onShell(); }});
            items.push_back({"Sketch", "", {}, [this]() { onCreateSketch(); }});
            items.push_back({"Draft", "", {}, [this]() { onDraft(); }});
            items.push_back({"Measure", "M", {}, [this]() { onMeasure(); }});
        } else if (hasEdge) {
            // Edge selected: N=Fillet, E=Chamfer, S=Measure, W=Select
            items.push_back({"Fillet", "F", {}, [this]() { onFillet(); }});
            items.push_back({"Chamfer", "", {}, [this]() { onChamfer(); }});
            items.push_back({});  // empty E placeholder
            items.push_back({});  // empty SE placeholder
            items.push_back({"Measure", "M", {}, [this]() { onMeasure(); }});
            // Remove empty placeholders at the end -- only show actual items
        }
    }

    if (items.empty()) {
        // Nothing selected (default): N=Sketch, NE=Cylinder, E=Box, SE=Sphere, S=Measure, SW=Redo, W=Fit All, NW=Undo
        items.push_back({"Sketch", "", {}, [this]() { onCreateSketch(); }});
        items.push_back({"Cylinder", "", {}, [this]() { onCreateCylinder(); }});
        items.push_back({"Box", "B", {}, [this]() { onCreateBox(); }});
        items.push_back({"Sphere", "", {}, [this]() { onCreateSphere(); }});
        items.push_back({"Measure", "M", {}, [this]() { onMeasure(); }});
        items.push_back({"Redo", "Ctrl+Y", {}, [this]() { onRedo(); }});
        items.push_back({"Fit All", "", {}, [this]() { m_viewport->fitAll(); }});
        items.push_back({"Undo", "Ctrl+Z", {}, [this]() { onUndo(); }});
    }

    // Remove any empty-text items (placeholders)
    items.erase(std::remove_if(items.begin(), items.end(),
        [](const MarkingMenu::MenuItem& mi) { return mi.text.isEmpty(); }),
        items.end());

    m_markingMenu->setItems(items);
    m_markingMenu->showAt(globalPos);
}

// Event filter to intercept right-click on the viewport for marking menu vs context menu
bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_viewport) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::RightButton) {
                m_rightClickPos = me->globalPos();
                m_rightClickLocalPos = me->pos();
                m_markingMenuShown = false;
                m_markingMenuTimer->start();
                // Do not consume -- let viewport handle the press too (for camera state)
            }
        } else if (event->type() == QEvent::MouseButtonRelease &&
                   m_pendingSketchPlane) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                // Check if user clicked on an origin plane (ray-plane intersection)
                double ox, oy, oz, xdx, xdy, xdz, ydx, ydy, ydz;
                std::string planeName = hitTestOriginPlanes(me->pos(),
                    ox, oy, oz, xdx, xdy, xdz, ydx, ydy, ydz);
                if (!planeName.empty()) {
                    // Create sketch on this origin plane
                    features::SketchParams skParams;
                    skParams.planeId = planeName;
                    skParams.originX = ox; skParams.originY = oy; skParams.originZ = oz;
                    skParams.xDirX = xdx; skParams.xDirY = xdy; skParams.xDirZ = xdz;
                    skParams.yDirX = ydx; skParams.yDirY = ydy; skParams.yDirZ = ydz;

                    m_pendingSketchPlane = false;
                    m_pendingCommand = PendingCommand::None;
                    m_selectionMgr->clearSelection();
                    m_selectionMgr->setFilter(SelectionFilter::All);
                    if (m_selectAllAction) m_selectAllAction->setChecked(true);
                    hideConfirmBar();

                    m_document->executeCommand(
                        std::make_unique<document::AddSketchCommand>(std::move(skParams)));
                    refreshAllPanels();

                    auto& tl = m_document->timeline();
                    for (size_t i = tl.count(); i > 0; --i) {
                        auto& entry = tl.entry(i - 1);
                        if (entry.feature &&
                            entry.feature->type() == features::FeatureType::Sketch &&
                            !entry.isSuppressed && !entry.isRolledBack) {
                            auto* skFeat = static_cast<features::SketchFeature*>(entry.feature.get());
                            beginSketchEditing(skFeat);
                            break;
                        }
                    }
                    return true; // consume the event
                }
                // Otherwise, let it fall through to normal picking (for planar face selection)
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (m_markingMenuTimer->isActive()) {
                // If moved more than 5px, cancel the marking menu timer and fall back to normal context menu
                QPoint delta = me->pos() - m_rightClickLocalPos;
                if (delta.manhattanLength() > 5) {
                    m_markingMenuTimer->stop();
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::RightButton) {
                if (m_markingMenuTimer->isActive()) {
                    // Quick right-click: stop timer, show standard context menu
                    m_markingMenuTimer->stop();
                    showViewportContextMenu(me->globalPos());
                    return true;  // consume the release
                }
                // If marking menu is shown, it handles its own release via grabMouse
            }
        }
    }
    // Toolbar hover filter: temporarily change selection filter when hovering
    // over a command button (e.g., hover Fillet → filter to Edges)
    if (event->type() == QEvent::Enter) {
        QVariant prop = obj->property("_hoverFilter");
        if (prop.isValid()) {
            if (!m_hoverFilterActive) {
                m_savedHoverFilter = m_selectionMgr->filter();
                m_hoverFilterActive = true;
            }
            m_selectionMgr->setFilterSoft(static_cast<SelectionFilter>(prop.toInt()));
        }
    } else if (event->type() == QEvent::Leave) {
        QVariant prop = obj->property("_hoverFilter");
        if (prop.isValid()) {
            restoreHoverFilter();
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

// =============================================================================
// Command palette (Ctrl+K fuzzy command search)
// =============================================================================

void MainWindow::setupCommandPalette()
{
    m_commandPalette = new CommandPalette(this);

    // Keyboard shortcut: Ctrl+K
    auto* shortcutK = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_K), this);
    connect(shortcutK, &QShortcut::activated, this, [this]() {
        m_commandPalette->activate();
    });

    // Alternative: Ctrl+Shift+P
    auto* shortcutP = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P), this);
    connect(shortcutP, &QShortcut::activated, this, [this]() {
        m_commandPalette->activate();
    });

    // Register all available commands
    std::vector<CommandPalette::CommandEntry> cmds = {
        // ── File ────────────────────────────────────────────────────────
        {"New Document",     "Ctrl+N",       "File",   [this]() { onNewDocument(); }},
        {"Open...",          "Ctrl+O",       "File",   [this]() { onOpenDocument(); }},
        {"Save",             "Ctrl+S",       "File",   [this]() { onSaveDocument(); }},
        {"Import File...",   "Ctrl+I",       "File",   [this]() { onImportFile(); }},
        {"Import STL as Body...", "",       "File",   [this]() { onImportSTL(); }},
        {"Export STEP...",   "",             "File",   [this]() { onExportSTEP(); }},
        {"Export STL...",    "",             "File",   [this]() { onExportSTL(); }},

        // ── Edit ────────────────────────────────────────────────────────
        {"Undo",             "Ctrl+Z",       "Edit",   [this]() { onUndo(); }},
        {"Redo",             "Ctrl+Y",       "Edit",   [this]() { onRedo(); }},
        {"Delete",           "Del",          "Edit",   [this]() { onDeleteSelectedFeature(); }},

        // ── Model (Create) ──────────────────────────────────────────────
        {"Create Box",       "B",            "Model",  [this]() { onCreateBox(); }},
        {"Create Cylinder",  "",             "Model",  [this]() { onCreateCylinder(); }},
        {"Create Sphere",    "",             "Model",  [this]() { onCreateSphere(); }},

        // ── Model (Modify) ──────────────────────────────────────────────
        {"Extrude",          "E",            "Model",  [this]() { onExtrudeSketch(); }},
        {"Revolve",          "",             "Model",  [this]() { onRevolveSketch(); }},
        {"Fillet",           "F",            "Model",  [this]() { onFillet(); }},
        {"Chamfer",          "",             "Model",  [this]() { onChamfer(); }},
        {"Shell",            "",             "Model",  [this]() { onShell(); }},
        {"Draft",            "",             "Model",  [this]() { onDraft(); }},
        {"Hole",             "H",            "Model",  [this]() { onAddHole(); }},
        {"Mirror",           "",             "Model",  [this]() { onMirrorLastBody(); }},
        {"Rectangular Pattern", "",          "Model",  [this]() { onRectangularPattern(); }},
        {"Circular Pattern", "",             "Model",  [this]() { onCircularPattern(); }},
        {"Sweep",            "",             "Model",  [this]() { onSweepSketch(); }},
        {"Loft",             "",             "Model",  [this]() { onLoftTest(); }},

        // ── Sketch ──────────────────────────────────────────────────────
        {"Create Sketch",    "",             "Sketch", [this]() { onCreateSketch(); }},
        {"Edit Sketch",      "",             "Sketch", [this]() { onEditSketch(); }},
        {"Import DXF to Sketch...", "",      "Sketch", [this]() { onImportDxfToSketch(); }},
        {"Import SVG to Sketch...", "",      "Sketch", [this]() { onImportSvgToSketch(); }},

        // ── Assembly ────────────────────────────────────────────────────
        {"New Component",    "",             "Assembly",[this]() { onNewComponent(); }},
        {"Insert Component...", "",          "Assembly",[this]() { onInsertComponent(); }},
        {"Add Joint",        "J",            "Assembly",[this]() { onAddJoint(); }},
        {"Check Interference","",            "Assembly",[this]() { onCheckInterference(); }},

        // ── View ────────────────────────────────────────────────────────
        {"Fit All",          "Home",         "View",   [this]() { m_viewport->fitAll(); }},
        {"Front View",       "Num1",         "View",   [this]() { m_viewport->setStandardView(StandardView::Front); }},
        {"Back View",        "",             "View",   [this]() { m_viewport->setStandardView(StandardView::Back); }},
        {"Left View",        "",             "View",   [this]() { m_viewport->setStandardView(StandardView::Left); }},
        {"Right View",       "Num3",         "View",   [this]() { m_viewport->setStandardView(StandardView::Right); }},
        {"Top View",         "Num7",         "View",   [this]() { m_viewport->setStandardView(StandardView::Top); }},
        {"Bottom View",      "",             "View",   [this]() { m_viewport->setStandardView(StandardView::Bottom); }},
        {"Isometric View",   "Num0",         "View",   [this]() { m_viewport->setStandardView(StandardView::Isometric); }},
        {"Toggle Grid",      "G",            "View",   [this]() { m_viewport->setShowGrid(!m_viewport->showGrid()); }},
        {"Toggle Origin",    "O",            "View",   [this]() { m_viewport->setShowOrigin(!m_viewport->showOrigin()); }},

        // ── Tools ───────────────────────────────────────────────────────
        {"Measure",          "M",            "Tools",  [this]() { onMeasure(); }},

        // ── Section planes ──────────────────────────────────────────────
        {"Section X",        "",             "View",   [this]() { onSectionX(); }},
        {"Section Y",        "",             "View",   [this]() { onSectionY(); }},
        {"Section Z",        "",             "View",   [this]() { onSectionZ(); }},
        {"Clear Section",    "",             "View",   [this]() { onClearSection(); }},
    };

    m_commandPalette->setCommands(cmds);
}

// =============================================================================
// Viewport manipulator -- Extrude drag handle
// =============================================================================

void MainWindow::showExtrudeManipulator(const QString& featureId)
{
    // Find the extrude feature in the timeline
    auto& tl = m_document->timeline();
    features::ExtrudeFeature* extFeat = nullptr;

    for (size_t i = 0; i < tl.count(); ++i) {
        auto& entry = tl.entry(i);
        if (entry.id == featureId.toStdString() &&
            entry.feature &&
            entry.feature->type() == features::FeatureType::Extrude) {
            extFeat = static_cast<features::ExtrudeFeature*>(entry.feature.get());
            break;
        }
    }

    if (!extFeat) {
        hideManipulator();
        return;
    }

    // Determine the extrude origin and direction.
    // The origin is derived from the sketch plane center (if available) or (0,0,0).
    // The direction defaults to +Z for a standard sketch on the XY plane.
    QVector3D origin(0.0f, 0.0f, 0.0f);
    QVector3D direction(0.0f, 0.0f, 1.0f);

    // Try to find the parent sketch to determine the plane
    const auto& params = extFeat->params();
    if (!params.sketchId.empty()) {
        for (size_t i = 0; i < tl.count(); ++i) {
            auto& entry = tl.entry(i);
            if (entry.id == params.sketchId &&
                entry.feature &&
                entry.feature->type() == features::FeatureType::Sketch) {
                auto* skFeat = static_cast<features::SketchFeature*>(entry.feature.get());
                const auto& sp = skFeat->params();
                // Origin from sketch plane
                origin = QVector3D(
                    static_cast<float>(sp.originX),
                    static_cast<float>(sp.originY),
                    static_cast<float>(sp.originZ));
                // Normal = cross(xDir, yDir) from sketch plane
                QVector3D xDir(static_cast<float>(sp.xDirX),
                               static_cast<float>(sp.xDirY),
                               static_cast<float>(sp.xDirZ));
                QVector3D yDir(static_cast<float>(sp.yDirX),
                               static_cast<float>(sp.yDirY),
                               static_cast<float>(sp.yDirZ));
                direction = QVector3D::crossProduct(xDir, yDir).normalized();
                if (direction.length() < 0.5f)
                    direction = QVector3D(0, 0, 1);  // fallback
                break;
            }
        }
    }

    // Parse the current distance value
    double currentDist = 20.0;  // default
    {
        const std::string& expr = params.distanceExpr;
        // Simple parse: "50 mm" -> 50.0
        try {
            currentDist = std::stod(expr);
        } catch (...) {
            currentDist = 20.0;
        }
    }

    // Set direction sign from the feature params
    m_manipulator->showDistance(origin, direction, currentDist, 0.1, 500.0);
    if (params.direction == features::ExtentDirection::Negative)
        m_manipulator->flipDirection();

    // Connect value changes to live preview update
    disconnect(m_manipulator, nullptr, this, nullptr);  // Clean up old connections

    connect(m_manipulator, &ViewportManipulator::valueChanged, this,
            [this, featureId](double newValue) {
        // Find the feature and update its distance expression
        auto& tl2 = m_document->timeline();
        for (size_t i = 0; i < tl2.count(); ++i) {
            auto& entry = tl2.entry(i);
            if (entry.id == featureId.toStdString() &&
                entry.feature &&
                entry.feature->type() == features::FeatureType::Extrude) {
                auto* feat = static_cast<features::ExtrudeFeature*>(entry.feature.get());
                feat->params().distanceExpr =
                    QString::number(newValue, 'f', 2).toStdString() + " mm";
                break;
            }
        }

        // Trigger a preview update
        if (m_previewEngine && m_previewEngine->isActive()) {
            m_previewEngine->updatePreview();
        }
    });

    connect(m_manipulator, &ViewportManipulator::dragFinished, this,
            [this, featureId](double finalValue) {
        // Commit the value -- the preview engine will handle it when
        // the user presses Enter to finish editing.
        statusBar()->showMessage(
            tr("Extrude distance: %1 mm").arg(finalValue, 0, 'f', 1));
    });

    connect(m_manipulator, &ViewportManipulator::directionFlipped, this,
            [this, featureId](int newSign) {
        auto& tl2 = m_document->timeline();
        for (size_t i = 0; i < tl2.count(); ++i) {
            auto& entry = tl2.entry(i);
            if (entry.id == featureId.toStdString() &&
                entry.feature &&
                entry.feature->type() == features::FeatureType::Extrude) {
                auto* feat = static_cast<features::ExtrudeFeature*>(entry.feature.get());
                feat->params().direction = (newSign > 0)
                    ? features::ExtentDirection::Positive
                    : features::ExtentDirection::Negative;
                break;
            }
        }

        if (m_previewEngine && m_previewEngine->isActive()) {
            m_previewEngine->updatePreview();
        }

        m_viewport->update();
    });
}

void MainWindow::hideManipulator()
{
    if (m_manipulator) {
        disconnect(m_manipulator, nullptr, this, nullptr);
        m_manipulator->hide();
        m_viewport->update();
    }
}

// -----------------------------------------------------------------------------
// Fillet radius manipulator (single-click preview from tree)
// -----------------------------------------------------------------------------

void MainWindow::showFilletManipulator(const QString& featureId)
{
    auto& tl = m_document->timeline();
    features::FilletFeature* filletFeat = nullptr;

    for (size_t i = 0; i < tl.count(); ++i) {
        auto& entry = tl.entry(i);
        if (entry.id == featureId.toStdString() &&
            entry.feature &&
            entry.feature->type() == features::FeatureType::Fillet) {
            filletFeat = static_cast<features::FilletFeature*>(entry.feature.get());
            break;
        }
    }

    if (!filletFeat) {
        hideManipulator();
        return;
    }

    const auto& params = filletFeat->params();

    // Place the manipulator at the body origin with direction along +Y
    // (a reasonable default for radius visualization).
    QVector3D origin(0.0f, 0.0f, 0.0f);
    QVector3D direction(0.0f, 1.0f, 0.0f);

    double currentRadius = 2.0;
    try {
        currentRadius = std::stod(params.radiusExpr);
    } catch (...) {
        currentRadius = 2.0;
    }

    m_manipulator->showDistance(origin, direction, currentRadius, 0.1, 200.0);

    disconnect(m_manipulator, nullptr, this, nullptr);

    connect(m_manipulator, &ViewportManipulator::valueChanged, this,
            [this, featureId](double newValue) {
        auto& tl2 = m_document->timeline();
        for (size_t i = 0; i < tl2.count(); ++i) {
            auto& entry = tl2.entry(i);
            if (entry.id == featureId.toStdString() &&
                entry.feature &&
                entry.feature->type() == features::FeatureType::Fillet) {
                auto* feat = static_cast<features::FilletFeature*>(entry.feature.get());
                feat->params().radiusExpr =
                    QString::number(newValue, 'f', 2).toStdString() + " mm";
                break;
            }
        }

        if (m_previewEngine && m_previewEngine->isActive()) {
            m_previewEngine->updatePreview();
        }
    });

    connect(m_manipulator, &ViewportManipulator::dragFinished, this,
            [this, featureId](double finalValue) {
        statusBar()->showMessage(
            tr("Fillet radius: %1 mm").arg(finalValue, 0, 'f', 1));
    });
}

// -----------------------------------------------------------------------------
// Generic manipulator dispatch (used by single-click in feature tree)
// -----------------------------------------------------------------------------

void MainWindow::showManipulatorForFeature(const QString& featureId)
{
    auto& tl = m_document->timeline();
    for (size_t i = 0; i < tl.count(); ++i) {
        if (tl.entry(i).id != featureId.toStdString())
            continue;
        const auto* feat = tl.entry(i).feature.get();
        if (!feat) break;

        switch (feat->type()) {
        case features::FeatureType::Extrude:
            showExtrudeManipulator(featureId);
            return;
        case features::FeatureType::Fillet:
            showFilletManipulator(featureId);
            return;
        default:
            break;
        }
        break;
    }
    hideManipulator();
}

// =============================================================================
// Toolbar hover filter -- change selection filter when hovering command buttons
// =============================================================================

void MainWindow::installToolBarHoverFilters()
{
    // Install event filters on ribbon tool buttons.
    // When the user hovers over a command button, we temporarily switch
    // the selection filter to match what that command needs, guiding
    // the user to pre-select the right entity type.

    // Map tool names to their required selection filter
    QHash<QString, SelectionFilter> filterMap = {
        {"Fillet",  SelectionFilter::Edges},
        {"Chamfer", SelectionFilter::Edges},
        {"Extrude", SelectionFilter::Faces},
        {"Shell",   SelectionFilter::Faces},
        {"Draft",   SelectionFilter::Faces},
        {"Hole",    SelectionFilter::Faces},
    };

    // Find all QToolButtons in the ribbon that match these tool names
    if (!m_ribbonContainer) return;
    const auto buttons = m_ribbonContainer->findChildren<QToolButton*>("RibbonButton");
    for (auto* btn : buttons) {
        QString toolName = btn->property("_toolName").toString();
        auto it = filterMap.find(toolName);
        if (it != filterMap.end()) {
            btn->installEventFilter(this);
            btn->setProperty("_hoverFilter", static_cast<int>(it.value()));
        }
    }
}

void MainWindow::restoreHoverFilter()
{
    if (m_hoverFilterActive) {
        m_selectionMgr->setFilterSoft(m_savedHoverFilter);
        m_hoverFilterActive = false;
    }
}

// Override eventFilter for toolbar hover detection
// (eventFilter merged into the single definition above)

// ── Recent Files ─────────────────────────────────────────────────────────

void MainWindow::addToRecentFiles(const QString& path)
{
    QSettings settings;
    QStringList recent = settings.value("recentFiles").toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 10)
        recent.removeLast();
    settings.setValue("recentFiles", recent);
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu()
{
    m_recentFilesMenu->clear();
    QSettings settings;
    QStringList recent = settings.value("recentFiles").toStringList();

    for (int i = 0; i < recent.size(); ++i) {
        const QString& path = recent[i];
        QString label = QString("%1. %2").arg(i + 1).arg(QFileInfo(path).fileName());
        auto* action = m_recentFilesMenu->addAction(label);
        connect(action, &QAction::triggered, this, [this, path]() {
            if (m_document->load(path.toStdString())) {
                m_featureTree->setDocument(m_document.get());
                m_parameterTable->setDocument(m_document.get());
                m_properties->clear();
                setWindowTitle("kernelCAD \u2014 " + QFileInfo(path).baseName());
                refreshAllPanels();
                addToRecentFiles(path);
            } else {
                QMessageBox::warning(this, tr("Open Failed"),
                    tr("Could not open file: %1").arg(path));
            }
        });
    }

    if (recent.isEmpty()) {
        m_recentFilesMenu->addAction(tr("(No recent files)"))->setEnabled(false);
    } else {
        m_recentFilesMenu->addSeparator();
        m_recentFilesMenu->addAction(tr("Clear Recent"), this, [this]() {
            QSettings settings;
            settings.remove("recentFiles");
            updateRecentFilesMenu();
        });
    }
}

// ── Preferences ──────────────────────────────────────────────────────────

void MainWindow::onPreferences()
{
    PreferencesDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        QSettings s;

        // Apply auto-save interval
        int autoSaveMin = s.value("prefs/autoSaveInterval", 5).toInt();
        if (m_autoSave) {
            if (autoSaveMin > 0) {
                m_autoSave->setEnabled(true);
                m_autoSave->setInterval(autoSaveMin * 60);
            } else {
                m_autoSave->setEnabled(false);
            }
        }

        // Apply display settings to viewport
        int viewModeIdx = s.value("prefs/defaultViewMode", 0).toInt();
        switch (viewModeIdx) {
        case 0: m_viewport->setViewMode(ViewMode::SolidWithEdges); break;
        case 1: m_viewport->setViewMode(ViewMode::Solid);          break;
        case 2: m_viewport->setViewMode(ViewMode::Wireframe);      break;
        }

        m_viewport->setShowOrigin(s.value("prefs/showOrigin", true).toBool());
        m_viewport->setShowGrid(s.value("prefs/showGrid", true).toBool());

        m_viewport->update();
    }
}
