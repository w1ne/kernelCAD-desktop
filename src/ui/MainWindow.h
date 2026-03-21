#pragma once
#include <QMainWindow>
#include <QVariant>
#include <memory>
#include "SelectionManager.h"  // SelectionFilter enum

class QAction;
class QActionGroup;
class QSlider;
class QLabel;
class QPushButton;
class QTimer;
class JointCreator;
class ViewportManipulator;

namespace document { class Document; class PreviewEngine; class InteractiveCommand; class AutoSave; }

class QToolBar;
class QTabWidget;
class QToolButton;
class QHBoxLayout;
class QFrame;

class Viewport3D;
class FeatureTree;
class TimelinePanel;
class PropertiesPanel;
class SelectionManager;
class SketchEditor;
class MeasureTool;
class MarkingMenu;
class CommandPalette;

namespace features { class SketchFeature; }

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUI();
    void setupMenuBar();
    void setupDocks();
    void connectSignals();

    // Actions
    void onNewDocument();
    void onOpenDocument();
    void onSaveDocument();
    void onImportFile();
    void onExportSTEP();
    void onExportSTL();
    void onCreateBox();
    void onCreateCylinder();
    void onCreateSphere();
    void onCreateSketch();
    void onEditSketch();
    void onExtrudeSketch();
    void onRevolveSketch();
    void onShell();
    void onFillet();
    void onChamfer();
    void onDraft();

    /// Commit a pending selection-driven command (triggered by Enter key).
    void onCommitPendingCommand();

    /// Cancel a pending command and restore default selection state.
    void onCancelPendingCommand();
    void onConstructPlane();
    void onConstructAxis();
    void onConstructPoint();
    void onMirrorLastBody();
    void onCircularPattern();
    void onRectangularPattern();
    void onAddHole();
    void onSweepTest();
    void onLoftTest();
    void onSweepSketch();
    void onNewComponent();
    void onInsertComponent();
    void onAddJoint();
    void onCheckInterference();
    void onUndo();
    void onRedo();

    /// Delete the currently selected feature (from feature tree or viewport selection).
    void onDeleteSelectedFeature();

    /// Show the viewport right-click context menu at the given global position.
    void showViewportContextMenu(const QPoint& globalPos);

    /// Push the current BRepModel meshes to the viewport for display.
    void updateViewport();

    /// Refresh feature tree, timeline panel, and viewport in one call.
    void refreshAllPanels();

    /// Update the window title to show modified indicator.
    void updateWindowTitle();

    /// Update undo/redo action text and enabled state.
    void updateUndoRedoActions();

    /// Called when the 3D selection changes -- updates properties panel and highlights.
    void onSelectionChanged();

    /// Called when the user edits a property in the properties panel.
    /// During a preview session, this updates the feature params and
    /// triggers a live preview update (no full recompute).
    void onPropertyChanged(const QString& featureId, const QString& propertyName,
                           const QVariant& newValue);

    /// Called when the user commits property edits (Enter or focus-out).
    void onEditingCommitted(const QString& featureId);

    /// Called when the user cancels property edits (Escape).
    void onEditingCancelled(const QString& featureId);

    /// Roll back the timeline to the given feature and show its properties for editing.
    void onEditFeature(const QString& featureId);

    /// Restore the timeline marker to the end after feature editing is complete.
    void onFinishEditing();

    std::unique_ptr<document::Document> m_document;
    std::unique_ptr<document::PreviewEngine> m_previewEngine;

    // Selection
    std::unique_ptr<SelectionManager> m_selectionMgr;

    // Panels
    Viewport3D*     m_viewport     = nullptr;
    FeatureTree*    m_featureTree  = nullptr;
    TimelinePanel*  m_timeline     = nullptr;
    PropertiesPanel* m_properties  = nullptr;

    /// The feature ID currently being edited (empty when not in edit mode).
    QString m_editingFeatureId;

    /// The timeline marker position before entering edit mode (for cancel restore).
    size_t m_editOriginalMarkerPos = 0;

    // Edit menu actions (need references for dynamic text updates)
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;

    // View menu filter actions
    QAction* m_selectAllAction   = nullptr;
    QAction* m_selectFacesAction = nullptr;
    QAction* m_selectEdgesAction = nullptr;
    QAction* m_selectBodiesAction = nullptr;
    QActionGroup* m_filterGroup  = nullptr;

    // Standard view actions
    QAction* m_viewFrontAction      = nullptr;
    QAction* m_viewBackAction       = nullptr;
    QAction* m_viewLeftAction       = nullptr;
    QAction* m_viewRightAction      = nullptr;
    QAction* m_viewTopAction        = nullptr;
    QAction* m_viewBottomAction     = nullptr;
    QAction* m_viewIsometricAction  = nullptr;
    QAction* m_viewFitAllAction     = nullptr;

    // View mode actions
    QAction* m_viewSolidWithEdgesAction = nullptr;
    QAction* m_viewSolidAction          = nullptr;
    QAction* m_viewWireframeAction      = nullptr;
    QActionGroup* m_viewModeGroup       = nullptr;

    // Section plane actions and slider
    QAction* m_sectionXAction = nullptr;
    QAction* m_sectionYAction = nullptr;
    QAction* m_sectionZAction = nullptr;
    QAction* m_clearSectionAction = nullptr;
    QSlider* m_sectionSlider = nullptr;
    int m_sectionAxis = -1;  // -1 = none, 0 = X, 1 = Y, 2 = Z

    void onSectionX();
    void onSectionY();
    void onSectionZ();
    void onClearSection();
    void onSectionSliderChanged(int value);

    // ── Ribbon toolbar (tabbed icon bar) ───────────────────────────────────
    QWidget*    m_ribbonContainer = nullptr;  // top-level container for quick-access + ribbon
    QTabWidget* m_ribbon          = nullptr;
    QWidget*    m_quickAccessBar  = nullptr;
    int         m_solidTabIndex   = 0;
    int         m_sketchTabIndex  = 0;
    int         m_assemblyTabIndex = 0;
    void setupToolBar();

    /// Ribbon helper: describes a single tool button in a ribbon group.
    struct ToolEntry {
        QString      name;
        QIcon        icon;
        QString      tooltip;
        std::function<void()> action;
    };

    /// Add a labelled group of tool buttons to a ribbon tab layout.
    void addToolGroup(QHBoxLayout* parentLayout, const QString& groupName,
                      const std::vector<ToolEntry>& tools);

    /// Add a thin vertical separator between groups.
    void addGroupSeparator(QHBoxLayout* layout);

    // Toolbar actions that also carry global shortcuts
    QAction* m_extrudeAction  = nullptr;
    QAction* m_filletAction   = nullptr;
    QAction* m_holeAction     = nullptr;
    QAction* m_jointAction    = nullptr;
    QAction* m_measureAction  = nullptr;
    QAction* m_moveAction     = nullptr;
    QAction* m_deleteAction   = nullptr;

    // Sketch editing
    SketchEditor* m_sketchEditor = nullptr;
    QToolBar*     m_sketchToolBar = nullptr;
    features::SketchFeature* m_activeSketchFeature = nullptr;

    void setupSketchToolBar();
    void showSketchToolBar(bool visible);
    void beginSketchEditing(features::SketchFeature* sketchFeat);
    void onSketchEditingFinished();

    // Measure tool
    MeasureTool* m_measureTool = nullptr;
    bool m_measureActive = false;
    void onMeasure();

    // Joint creator for interactive face-to-face joint workflow
    JointCreator* m_jointCreator = nullptr;

    // Exploded view slider
    QSlider* m_explodeSlider = nullptr;

    // Auto-save
    document::AutoSave* m_autoSave = nullptr;

    // Interactive command dialog helper
    void executeInteractiveCommand(std::unique_ptr<document::InteractiveCommand> cmd);

    // Pending selection-driven command workflow
    enum class PendingCommand { None, Fillet, Chamfer, Shell, Draft, Hole };
    PendingCommand m_pendingCommand = PendingCommand::None;

    /// Helper: collect selected edge indices for a single body from the current selection.
    /// Returns the bodyId via the out parameter. Edges are only collected from the first body.
    std::vector<int> collectSelectedEdges(std::string& bodyIdOut) const;

    /// Helper: collect selected face indices for a single body from the current selection.
    std::vector<int> collectSelectedFaces(std::string& bodyIdOut) const;

    // ── Confirmation toolbar (floating OK/Cancel during active commands) ──
    QWidget*     m_confirmBar       = nullptr;
    QLabel*      m_confirmLabel     = nullptr;
    QPushButton* m_confirmOkBtn     = nullptr;
    QPushButton* m_confirmCancelBtn = nullptr;
    void setupConfirmBar();
    void showConfirmBar(const QString& toolName);
    void hideConfirmBar();

    // ── Rich status bar segments ──────────────────────────────────────────
    QLabel* m_statusLeft   = nullptr;
    QLabel* m_statusCenter = nullptr;
    QLabel* m_statusRight  = nullptr;
    void setupStatusBar();
    void updateStatusBarInfo();

    // ── Marking menu (radial context menu) ──────────────────────────────
    MarkingMenu* m_markingMenu = nullptr;
    QTimer*      m_markingMenuTimer = nullptr;
    QPoint       m_rightClickPos;        // global position of right-click
    QPoint       m_rightClickLocalPos;   // viewport-local position
    bool         m_markingMenuShown = false;
    void setupMarkingMenu();
    void showMarkingMenuForContext(const QPoint& globalPos);

    // ── Command palette ─────────────────────────────────────────────────
    CommandPalette* m_commandPalette = nullptr;
    void setupCommandPalette();

    // ── Viewport manipulator (drag handles for feature editing) ──────────
    ViewportManipulator* m_manipulator = nullptr;

    /// Show the distance manipulator for the currently-edited Extrude feature.
    void showExtrudeManipulator(const QString& featureId);

    /// Hide the manipulator and disconnect signals.
    void hideManipulator();

    // ── Toolbar hover filter ─────────────────────────────────────────────
    QAction* m_chamferAction = nullptr;
    QAction* m_shellAction   = nullptr;
    QAction* m_draftAction   = nullptr;

    /// Saved selection filter before a toolbar hover override.
    SelectionFilter m_savedHoverFilter = SelectionFilter::All;
    bool m_hoverFilterActive = false;

    /// Install event filters on toolbar actions for hover-based filter switching.
    void installToolBarHoverFilters();

    /// Restore the selection filter after toolbar hover ends.
    void restoreHoverFilter();
};
