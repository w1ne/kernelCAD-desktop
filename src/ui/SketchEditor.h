#pragma once
#include <QObject>
#include <QPoint>
#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <chrono>

class QMouseEvent;
class QKeyEvent;
class QLineEdit;
class Viewport3D;

namespace sketch { class Sketch; }
#include "../sketch/SketchConstraint.h"

enum class SketchTool {
    None,              // default pointer/select mode
    DrawLine,          // click start -> click end
    DrawRectangle,     // click corner -> click opposite corner
    DrawCircle,        // click center -> click radius point
    DrawArc,           // click center -> click start -> click end
    DrawSpline,        // click multiple points, double-click or Enter to finish
    DrawEllipse,       // click center -> click to set major/minor radii
    DrawPolygon,       // click center -> move to set radius, creates N-sided polygon
    DrawSlot,          // click center1 -> click center2 -> move to set width
    DrawCircle3Point,  // 3 clicks define a circle through those points
    DrawArc3Point,     // 3 clicks (start, mid, end) define an arc
    DrawRectangleCenter, // click center -> click corner (symmetric rectangle)
    AddConstraint,     // pick entities to auto-constrain (infers type)
    Dimension,         // pick entity, enter value
    // Specific constraint tools (user knows exactly what they're applying)
    ConstrainCoincident,
    ConstrainParallel,
    ConstrainPerpendicular,
    ConstrainTangent,
    ConstrainEqual,
    ConstrainSymmetric,
    ConstrainHorizontal,
    ConstrainVertical,
    ConstrainConcentric,
    Trim,              // click on curve segment to remove it
    Extend,            // click near endpoint to extend to nearest intersection
    Offset,            // select curve, specify distance to create parallel copy
    ProjectEdge,       // click a 3D edge in the viewport, project onto sketch plane
    SketchFillet,      // pick two lines at a shared vertex, enter radius
    SketchChamfer      // pick two lines at a shared vertex, enter distance
};

/// Describes a picked sketch entity for constraint/dimension workflows.
struct SketchPickResult {
    enum Kind { Nothing, Point, Line, Circle, Arc, Spline, Ellipse };
    Kind kind = Nothing;
    std::string entityId;      // point/line/circle/arc id
    double sx = 0, sy = 0;    // sketch coords of the pick location
};

/// Type of geometric snap point for tooltip display.
enum class SnapType { None, Endpoint, Midpoint, Center, OnEdge, Intersection, Grid };

/// Visual inference line shown during sketch drawing to aid snapping.
struct InferenceLine {
    float x1, y1, x2, y2;  // sketch-plane coordinates
    enum Type { Horizontal, Vertical, Midpoint, Perpendicular, Tangent, Extension } type;
};

/// Mode controller that intercepts viewport input when a sketch is being edited.
/// Manages the interactive draw-commit workflow for sketch geometry.
class SketchEditor : public QObject
{
    Q_OBJECT
public:
    explicit SketchEditor(QObject* parent = nullptr);
    ~SketchEditor() override;

    /// Start editing a sketch (switches viewport to 2D sketch mode).
    void beginEditing(sketch::Sketch* sketch, Viewport3D* viewport);

    /// Finish editing and return to 3D mode.
    void finishEditing();

    bool isEditing() const { return m_isEditing; }

    /// Set the active drawing tool.
    void setTool(SketchTool tool);
    SketchTool currentTool() const { return m_tool; }

    /// Handle viewport events (called by Viewport3D when in sketch mode).
    /// Returns true if the event was consumed.
    bool handleMousePress(QMouseEvent* event);
    bool handleMouseMove(QMouseEvent* event);
    bool handleMouseRelease(QMouseEvent* event);
    bool handleKeyPress(QKeyEvent* event);

    /// Toggle auto-constraint inference after each entity is finalized.
    void setAutoConstrain(bool enabled) { m_autoConstrain = enabled; }
    bool autoConstrainEnabled() const { return m_autoConstrain; }

    /// Toggle global construction mode -- new entities are created as construction geometry.
    void setConstructionMode(bool enabled) { m_constructionMode = enabled; }
    bool constructionMode() const { return m_constructionMode; }

    /// Toggle grid snapping.
    void setGridSnap(bool enabled) { m_gridSnap = enabled; }
    bool gridSnapEnabled() const { return m_gridSnap; }

    /// Toggle construction flag on a nearby entity, or toggle global construction mode.
    void toggleConstruction(double sx, double sy);

    /// Polygon side count for DrawPolygon tool.
    int polygonSides() const { return m_polygonSides; }
    void setPolygonSides(int n) { m_polygonSides = std::max(3, n); }

    /// Spline drawing state: in-progress control points (for rubber-band rendering).
    const std::vector<std::pair<double,double>>& splinePoints() const { return m_splinePoints; }

    /// Access the sketch being edited (for overlay rendering).
    sketch::Sketch* currentSketch() const { return m_sketch; }

    /// Rubber-band state for overlay rendering.
    bool isDrawingInProgress() const { return m_drawingInProgress; }
    double rubberStartX() const { return m_startX; }
    double rubberStartY() const { return m_startY; }
    double rubberCurrentX() const { return m_currentX; }
    double rubberCurrentY() const { return m_currentY; }

    /// Drag state for overlay rendering (highlight dragged point).
    bool isDragging() const { return m_isDragging; }
    std::string dragPointId() const { return m_dragPointId; }

    /// First pick for Dimension / AddConstraint tool (for visual feedback).
    bool hasFirstPick() const { return m_firstPick.kind != SketchPickResult::Nothing; }
    const SketchPickResult& firstPick() const { return m_firstPick; }

    /// Selected constraint (for deletion).
    const std::string& selectedConstraintId() const { return m_selectedConstraintId; }

    /// Active inference lines (computed each mouse move, used by overlay renderer).
    const std::vector<InferenceLine>& inferenceLines() const { return m_inferenceLines; }

    /// Current snap type and position (for tooltip rendering in the viewport).
    SnapType snapType() const { return m_currentSnapType; }
    double snapX() const { return m_snapX; }
    double snapY() const { return m_snapY; }

signals:
    void sketchChanged();
    void toolChanged(SketchTool tool);
    void editingFinished();
    void constraintSelected(const QString& constraintId, const QString& typeName,
                            const QString& description);

    /// Emitted with context-specific hint text for the status bar.
    void statusHint(const QString& hint);

public:
    /// Undo the last sketch action (line draw, constraint add, etc.)
    /// Returns true if something was undone.
    bool undoLastAction();

private:
    sketch::Sketch* m_sketch = nullptr;
    Viewport3D* m_viewport = nullptr;
    SketchTool m_tool = SketchTool::None;
    bool m_isEditing = false;

    // Sketch-local undo stack: stores IDs of entities/constraints added
    struct SketchAction {
        enum Type { AddEntity, AddConstraint, RemoveEntity, RemoveConstraint };
        Type type;
        std::string entityId;       // for AddEntity/RemoveEntity
        std::string constraintId;   // for AddConstraint/RemoveConstraint
    };
    std::vector<SketchAction> m_sketchUndoStack;

    // In-progress drawing state
    bool m_drawingInProgress = false;
    double m_startX = 0, m_startY = 0;      // sketch coords of first click
    double m_currentX = 0, m_currentY = 0;   // current mouse in sketch coords

    // Arc drawing needs an extra intermediate click
    int m_arcClickCount = 0;
    double m_arcStartX = 0, m_arcStartY = 0;

    // Spline drawing: accumulated control points
    std::vector<std::pair<double,double>> m_splinePoints;

    // Auto-constraint toggle
    bool m_autoConstrain = false;

    // Construction mode toggle
    bool m_constructionMode = false;

    // Grid snap toggle (enabled by default)
    bool m_gridSnap = true;

    // Polygon side count
    int m_polygonSides = 6;

    // Slot tool: 3-click state (center1, center2, width)
    int m_slotClickCount = 0;
    double m_slotX1 = 0, m_slotY1 = 0;
    double m_slotX2 = 0, m_slotY2 = 0;

    // 3-point circle/arc click state
    int m_threePointClickCount = 0;
    double m_pt1X = 0, m_pt1Y = 0;
    double m_pt2X = 0, m_pt2Y = 0;

    // ── Drag state ──────────────────────────────────────────────────────
    enum class DragMode { Point, CircleRadius, ArcRadius };
    bool m_isDragging = false;
    DragMode m_dragMode = DragMode::Point;
    std::string m_dragPointId;
    std::string m_dragCircleId;  // for radius dragging

    // ── Constraint / Dimension tool state ───────────────────────────────
    SketchPickResult m_firstPick;   // first entity picked for two-entity tools
    std::string m_selectedConstraintId;
    std::chrono::steady_clock::time_point m_lastConstraintClickTime;  // for double-click detection

    // ── Offset tool state ────────────────────────────────────────────────
    std::string m_offsetEntityId;   // entity selected for offset
    bool m_offsetPending = false;   // true after first pick, waiting for distance

    // Convert screen position to sketch 2D coordinates.
    // Returns false if the ray does not intersect the sketch plane.
    bool screenToSketch(const QPoint& screenPos, double& sx, double& sy);

    // Grid snapping
    double snapToGrid(double val, double gridSize = 5.0);

    // ── Entity picking helpers ──────────────────────────────────────────
    /// Find the nearest sketch point within `threshold` sketch-space units.
    /// Returns the point id or empty string if none found.
    std::string findNearestPoint(double sx, double sy, double threshold = 5.0);

    /// Find the nearest sketch line within `threshold` sketch-space units.
    /// Returns the line id or empty string if none found.
    std::string findNearestLine(double sx, double sy, double threshold = 5.0);

    /// Find the nearest sketch circle within `threshold` sketch-space units.
    /// Returns the circle id or empty string if none found.
    std::string findNearestCircle(double sx, double sy, double threshold = 5.0);

    /// Find the nearest arc within `threshold` sketch-space units.
    std::string findNearestArc(double sx, double sy, double threshold = 5.0);

    /// Find the nearest ellipse within `threshold` sketch-space units.
    std::string findNearestEllipse(double sx, double sy, double threshold = 5.0);

    /// Find the nearest dimension constraint label near (sx, sy).
    std::string findNearestConstraint(double sx, double sy, double threshold = 8.0);

    /// Open an edit dialog for the currently selected constraint.
    void editSelectedConstraint();

    /// Composite pick: try point first, then line, circle, arc.
    SketchPickResult pickEntity(double sx, double sy, double threshold = 5.0);

    /// Auto-apply Horizontal/Vertical constraints to a newly finalized line
    /// if it is nearly axis-aligned (within 3 degrees).
    void autoConstrainLastEntity(const std::string& entityId);

    // Finalize the current draw operation
    void finalizeLine();
    void finalizeRectangle();
    void finalizeCircle();
    void finalizeArc();
    void finalizeSpline();
    void finalizeEllipse();
    void finalizePolygon();
    void finalizeSlot();
    void finalizeCircle3Point();
    void finalizeArc3Point();
    void finalizeRectangleCenter();

    // Cancel the current in-progress draw
    void cancelDraw();

    // ── Dimension / Constraint tool finalization ────────────────────────
    void handleDimensionPick(const SketchPickResult& pick);
    void handleConstraintPick(const SketchPickResult& pick);

    /// Apply a Horizontal constraint to the given line.
    void applyHorizontal(const std::string& lineId);
    /// Apply a Vertical constraint to the given line.
    void applyVertical(const std::string& lineId);

    /// Try to find the line that is closest to the cursor for H/V shortcuts.
    std::string findLineForShortcut(double sx, double sy);

    // ── Sketch fillet/chamfer pick handler ─────────────────────────────────
    void handleFilletChamferPick(const SketchPickResult& pick);

    // ── Find nearest curve (line, circle, or arc) for Trim/Offset tools ───
    std::string findNearestCurve(double sx, double sy, double threshold = 5.0);

    // ── Snap/inference system ───────────────────────────────────────────
    std::vector<InferenceLine> m_inferenceLines;
    float m_snapTolerance = 2.0f;  // sketch-space units

    // ── Line chain close-to-first-point (auto-close profile) ────────────
    std::string m_chainFirstPointId;  // ID of the first point in the current line chain
    double m_chainFirstX = 0, m_chainFirstY = 0;  // position for snap detection

    // ── Inline dimension input (replaces QInputDialog popups) ───────────
    QLineEdit* m_inlineInput = nullptr;
    std::string m_inlineDimConstraintId;  // constraint being edited (edit mode)
    std::string m_inlineDimEntityId;      // entity being dimensioned
    std::vector<std::string> m_inlineDimEntityIds;  // entity IDs for new constraint
    sketch::ConstraintType m_inlineDimType;  // constraint type for new dimension
    double m_inlineDimX = 0, m_inlineDimY = 0;  // position in sketch coords
    enum class InlineInputMode { None, NewDimension, EditDimension } m_inlineInputMode = InlineInputMode::None;

    void showInlineInput(double screenX, double screenY, double defaultValue, InlineInputMode mode);
    void commitInlineInput();
    void cancelInlineInput();
    SnapType m_currentSnapType = SnapType::None;
    double m_snapX = 0.0, m_snapY = 0.0;

    /// Compute inference lines for the given cursor position and return
    /// the snapped position (cursor may be adjusted to align H/V/midpoint).
    std::pair<double,double> computeInferences(double sketchX, double sketchY);
};
