#pragma once
#include <QObject>
#include <QVariant>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Forward declarations
namespace document { class Document; }
class MainWindow;
class Viewport3D;
class SelectionManager;
class FeatureDialog;
class PropertiesPanel;

struct SelectionHit;

enum class PendingCommand { None, Fillet, Chamfer, Shell, Draft, Hole, SketchPlane, PressPull, Extrude };

class CommandController : public QObject {
    Q_OBJECT
public:
    CommandController(MainWindow* mainWindow,
                     document::Document* doc,
                     SelectionManager* selMgr,
                     Viewport3D* viewport,
                     FeatureDialog* featureDialog,
                     PropertiesPanel* properties,
                     QObject* parent = nullptr);

    // ── Primitive creation ──────────────────────────────────────────────
    void onCreateBox();
    void onCreateCylinder();
    void onCreateSphere();
    void onCreateTorus();
    void onCreatePipe();

    // ── Sketch ──────────────────────────────────────────────────────────
    void onCreateSketch();
    void onEditSketch();

    // ── Form (extrude / revolve / sweep / loft) ─────────────────────────
    void onExtrudeSketch();
    void onRevolveSketch();
    void onSweepSketch();
    void onLoftTest();
    void onSweepTest();

    // ── Modify ──────────────────────────────────────────────────────────
    void onFillet();
    void onChamfer();
    void onShell();
    void onDraft();
    void onPressPull();
    void onAddHole();

    // ── Pattern ─────────────────────────────────────────────────────────
    void onMirrorLastBody();
    void onCircularPattern();
    void onRectangularPattern();

    // ── Construction geometry ───────────────────────────────────────────
    void onConstructPlane();
    void onConstructAxis();
    void onConstructPoint();

    // ── Assembly ────────────────────────────────────────────────────────
    void onNewComponent();
    void onInsertComponent();
    void onAddJoint();
    void onAddSliderJoint();
    void onAddCylindricalJoint();
    void onAddPinSlotJoint();
    void onAddBallJoint();
    void onCheckInterference();
    void onUnstitch();

    // ── Delete ──────────────────────────────────────────────────────────
    void onDeleteSelectedFeature();

    // ── Measure ─────────────────────────────────────────────────────────
    void onMeasure();

    // ── Property / selection / pending command ──────────────────────────
    void onPropertyChanged(const QString& featureId, const QString& propertyName,
                           const QVariant& newValue);
    void onSelectionChanged();
    void onCommitPendingCommand();
    void onCancelPendingCommand();

    // ── Helpers ─────────────────────────────────────────────────────────
    std::vector<int> collectSelectedEdges(std::string& bodyIdOut) const;
    std::vector<int> collectSelectedFaces(std::string& bodyIdOut) const;

    // ── State accessors ─────────────────────────────────────────────────
    PendingCommand pendingCommand() const { return m_pendingCommand; }
    QString lastCommandName() const { return m_lastCommandName; }
    std::function<void()> lastCommandCallback() const { return m_lastCommandCallback; }
    bool pendingSketchPlane() const { return m_pendingSketchPlane; }

    /// Handle the plane/face selection to start a sketch on the chosen plane.
    void handleSketchPlaneSelection(const SelectionHit& hit);

    /// Clear the pending sketch/command state (used by eventFilter for origin plane picks).
    void clearPendingState();


private:
    MainWindow* m_mainWindow;
    document::Document* m_document;
    SelectionManager* m_selectionMgr;
    Viewport3D* m_viewport;
    FeatureDialog* m_featureDialog;
    PropertiesPanel* m_properties;

    PendingCommand m_pendingCommand = PendingCommand::None;
    QString m_lastCommandName;
    std::function<void()> m_lastCommandCallback;

    /// True when waiting for the user to pick a plane or planar face to start a sketch.
    bool m_pendingSketchPlane = false;
};
