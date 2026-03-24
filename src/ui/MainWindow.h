#pragma once
#include <QMainWindow>
#include <QVariant>
#include <memory>
#include <string>
#include "SelectionManager.h"  // SelectionFilter enum
#include "CommandController.h" // PendingCommand enum
#include "ToolRegistration.h"  // registerAllTools free function (friend)

class QAction;
class QActionGroup;
class QMenu;
class QSlider;
class QLabel;
class QPushButton;
class QTimer;
class JointCreator;
class ViewportManipulator;
class PluginManager;

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
class FeatureDialog;
class SketchPalette;
class ParameterTablePanel;
class DrawingView;
class CommandController;

namespace features { class SketchFeature; }

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    // ── Public methods accessed by CommandController ──────────────────────

    /// Refresh feature tree, timeline panel, and viewport in one call.
    void refreshAllPanels();

    /// Show the confirmation toolbar with the given tool name.
    void showConfirmBar(const QString& toolName);

    /// Hide the confirmation toolbar.
    void hideConfirmBar();

    /// Push the current BRepModel meshes to the viewport for display.
    void updateViewport();

    /// Update the rich status bar segments.
    void updateStatusBarInfo();

    /// Hide the manipulator and disconnect signals.
    void hideManipulator();

    /// Enter sketch editing mode for the given sketch feature.
    void beginSketchEditing(features::SketchFeature* sketchFeat);

    // ── Accessors for CommandController ──────────────────────────────────

    SketchEditor* sketchEditor() const { return m_sketchEditor; }
    JointCreator* jointCreator() const { return m_jointCreator; }
    MeasureTool* measureTool() const { return m_measureTool; }
    bool measureActive() const { return m_measureActive; }
    void setMeasureActive(bool active) { m_measureActive = active; }
    document::PreviewEngine* previewEngine() const { return m_previewEngine.get(); }
    const QString& editingFeatureId() const { return m_editingFeatureId; }
    QAction* selectFacesAction() const { return m_selectFacesAction; }
    QAction* selectEdgesAction() const { return m_selectEdgesAction; }
    QAction* selectAllAction() const { return m_selectAllAction; }
    CommandController* commandController() const { return m_commandController; }

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
    void onImportSTL();
    void onExportSTEP();
    void onExportSTL();

    // Recent files
    void addToRecentFiles(const QString& path);
    void updateRecentFilesMenu();

    // Preferences
    void onPreferences();

    void onUndo();
    void onRedo();
    void onCreateDrawing();

    void onImportDxfToSketch();
    void onImportSvgToSketch();

    void onCheckInterference();

    /// Show the viewport right-click context menu at the given global position.
    void showViewportContextMenu(const QPoint& globalPos);

    /// Update the window title to show modified indicator.
    void updateWindowTitle();

    /// Update undo/redo action text and enabled state.
    void updateUndoRedoActions();

    /// Called when the user commits property edits (Enter or focus-out).
    void onEditingCommitted(const QString& featureId);

    /// Called when the user cancels property edits (Escape).
    void onEditingCancelled(const QString& featureId);

    /// Roll back the timeline to the given feature and show its properties for editing.
    void onEditFeature(const QString& featureId);

    /// Restore the timeline marker to the end after feature editing is complete.
    void onFinishEditing();

    /// Check if a click ray hits an origin plane (XY/XZ/YZ).
    /// Returns "XY", "XZ", "YZ", or empty string if no hit.
    /// Also fills planeOrigin, planeXDir, planeYDir for the matched plane.
    std::string hitTestOriginPlanes(const QPoint& screenPos,
                                    double& ox, double& oy, double& oz,
                                    double& xDirX, double& xDirY, double& xDirZ,
                                    double& yDirX, double& yDirY, double& yDirZ) const;

    std::unique_ptr<document::Document> m_document;
    std::unique_ptr<document::PreviewEngine> m_previewEngine;

    // Command controller (owns command handler methods)
    CommandController* m_commandController = nullptr;

    // Selection
    std::unique_ptr<SelectionManager> m_selectionMgr;

    // Panels
    Viewport3D*     m_viewport     = nullptr;
    FeatureTree*    m_featureTree  = nullptr;
    TimelinePanel*  m_timeline     = nullptr;
    PropertiesPanel* m_properties  = nullptr;
    ParameterTablePanel* m_parameterTable = nullptr;

    /// The feature ID currently being edited (empty when not in edit mode).
    QString m_editingFeatureId;

    /// The timeline marker position before entering edit mode (for cancel restore).
    size_t m_editOriginalMarkerPos = 0;

    // Recent files submenu
    QMenu* m_recentFilesMenu = nullptr;

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
    QWidget*    m_ribbonContainer = nullptr;  // top-level container for ribbon
    QTabWidget* m_ribbon          = nullptr;
    int         m_solidTabIndex   = 0;
    int         m_sketchTabIndex  = 0;
    int         m_assemblyTabIndex = 0;
    friend void ::registerAllTools(MainWindow* mw, CommandController* cmd);
    void setupToolBar();

    /// Ribbon helper: describes a single tool button in a ribbon group.
    struct ToolEntry {
        QString      name;
        QIcon        icon;
        QString      tooltip;
        std::function<void()> action;
    };

    /// Add a labelled group of tool buttons to a ribbon tab layout.
    /// dropdownExtras are additional commands shown below a separator in the group dropdown.
    void addToolGroup(QHBoxLayout* parentLayout, const QString& groupName,
                      const std::vector<ToolEntry>& tools,
                      const std::vector<ToolEntry>& dropdownExtras = {});

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
    void onSketchEditingFinished();

    // Measure tool
    MeasureTool* m_measureTool = nullptr;
    bool m_measureActive = false;

    // Joint creator for interactive face-to-face joint workflow
    JointCreator* m_jointCreator = nullptr;

    // Exploded view slider
    QSlider* m_explodeSlider = nullptr;

    // Auto-save
    document::AutoSave* m_autoSave = nullptr;

    // Interactive command dialog helper
    void executeInteractiveCommand(std::unique_ptr<document::InteractiveCommand> cmd);

    // ── Confirmation toolbar (floating OK/Cancel during active commands) ──
    QWidget*     m_confirmBar       = nullptr;
    QLabel*      m_confirmLabel     = nullptr;
    QPushButton* m_confirmOkBtn     = nullptr;
    QPushButton* m_confirmCancelBtn = nullptr;
    void setupConfirmBar();

    // ── Navigation bar (bottom-center viewport overlay) ────────────────────
    QWidget* m_navBar = nullptr;
    void setupNavBar();

    // ── Rich status bar segments ──────────────────────────────────────────
    QLabel* m_statusLeft   = nullptr;
    QLabel* m_statusCenter = nullptr;
    QLabel* m_statusRight  = nullptr;
    void setupStatusBar();

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

    // ── Feature dialog (floating command panel for feature creation) ─────
    FeatureDialog* m_featureDialog = nullptr;

    // ── Sketch palette (floating mode palette during sketch editing) ─────
    SketchPalette* m_sketchPalette = nullptr;

    // ── Viewport manipulator (drag handles for feature editing) ──────────
    ViewportManipulator* m_manipulator = nullptr;

    /// Show the distance manipulator for the currently-edited Extrude feature.
    void showExtrudeManipulator(const QString& featureId);

    /// Show the radius manipulator for a Fillet feature (single-click preview).
    void showFilletManipulator(const QString& featureId);

    /// Show the appropriate manipulator for a feature, if it is a dimensional type.
    /// Called on single-click in the feature tree for direct manipulation.
    void showManipulatorForFeature(const QString& featureId);

    // ── Toolbar hover filter ─────────────────────────────────────────────
    QAction* m_chamferAction = nullptr;
    QAction* m_shellAction   = nullptr;
    QAction* m_draftAction   = nullptr;

    /// Saved selection filter before a toolbar hover override.
    SelectionFilter m_savedHoverFilter = SelectionFilter::All;
    bool m_hoverFilterActive = false;

    /// True once the first body has been created and fitAll() was called.
    /// Reset on New Document so the next first body triggers auto-fit again.
    bool m_firstBodyFitDone = false;

    // ── Isolate body view ───────────────────────────────────────────────
    std::string m_isolatedBodyId;  ///< Empty = not isolated; non-empty = isolated body ID
    void onIsolateBody(const std::string& bodyId);
    void onShowAll();

    /// Install event filters on toolbar actions for hover-based filter switching.
    void installToolBarHoverFilters();

    /// Restore the selection filter after toolbar hover ends.
    void restoreHoverFilter();

    // ── Plugin subsystem ────────────────────────────────────────────────
    PluginManager* m_pluginManager = nullptr;
    QMenu*         m_pluginsMenu   = nullptr;

    /// Rebuild the Plugins menu after a rescan.
    void rebuildPluginMenu();

    /// Run a plugin by index, showing a parameter dialog if needed.
    void onRunPlugin(int pluginIndex);
};
