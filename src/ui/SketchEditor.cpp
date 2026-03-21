#include "SketchEditor.h"
#include "Viewport3D.h"
#include "../sketch/Sketch.h"
#include "../sketch/SketchSolver.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QMatrix4x4>
#include <QInputDialog>
#include <QWidget>
#include <QtMath>
#include <cmath>
#include <algorithm>
#include <limits>

SketchEditor::SketchEditor(QObject* parent)
    : QObject(parent)
{
}

SketchEditor::~SketchEditor() = default;

// =============================================================================
// Begin / Finish editing
// =============================================================================

void SketchEditor::beginEditing(sketch::Sketch* sketch, Viewport3D* viewport)
{
    if (!sketch || !viewport)
        return;

    m_sketch = sketch;
    m_viewport = viewport;
    m_isEditing = true;
    m_tool = SketchTool::None;
    m_drawingInProgress = false;
    m_arcClickCount = 0;
    m_splinePoints.clear();
    m_isDragging = false;
    m_dragPointId.clear();
    m_firstPick = {};
    m_selectedConstraintId.clear();
    m_inferenceLines.clear();
    m_slotClickCount = 0;
    m_threePointClickCount = 0;

    // Tell the viewport about the sketch overlay
    m_viewport->setSketchEditor(this);
    m_viewport->update();
}

void SketchEditor::finishEditing()
{
    if (!m_isEditing)
        return;

    cancelDraw();

    m_isEditing = false;
    m_tool = SketchTool::None;
    m_isDragging = false;
    m_dragPointId.clear();
    m_firstPick = {};
    m_selectedConstraintId.clear();
    m_inferenceLines.clear();

    if (m_viewport) {
        m_viewport->setSketchEditor(nullptr);
        m_viewport->update();
    }

    m_sketch = nullptr;
    m_viewport = nullptr;

    emit editingFinished();
}

// =============================================================================
// Tool selection
// =============================================================================

void SketchEditor::setTool(SketchTool tool)
{
    if (m_tool != tool) {
        cancelDraw();
        m_firstPick = {};
        m_selectedConstraintId.clear();
        m_offsetPending = false;
        m_offsetEntityId.clear();
        m_tool = tool;
        emit toolChanged(m_tool);
    }
}

// =============================================================================
// Screen-to-sketch coordinate conversion
// =============================================================================

bool SketchEditor::screenToSketch(const QPoint& screenPos, double& sx, double& sy)
{
    if (!m_sketch || !m_viewport)
        return false;

    // Build the same MVP as the viewport
    QMatrix4x4 view = m_viewport->viewMatrix();
    QMatrix4x4 projection = m_viewport->projectionMatrix();
    QMatrix4x4 mvp = projection * view;
    QMatrix4x4 invMvp = mvp.inverted();

    // NDC from screen coords
    float ndcX = (2.0f * screenPos.x()) / m_viewport->width() - 1.0f;
    float ndcY = 1.0f - (2.0f * screenPos.y()) / m_viewport->height();

    // Unproject near and far points
    QVector4D nearPt = invMvp * QVector4D(ndcX, ndcY, -1.0f, 1.0f);
    QVector4D farPt  = invMvp * QVector4D(ndcX, ndcY,  1.0f, 1.0f);
    if (std::abs(nearPt.w()) > 1e-7f) nearPt /= nearPt.w();
    if (std::abs(farPt.w())  > 1e-7f) farPt  /= farPt.w();

    QVector3D rayOrigin = nearPt.toVector3D();
    QVector3D rayDir    = (farPt.toVector3D() - rayOrigin).normalized();

    // Intersect with the sketch plane
    double ox, oy, oz;
    m_sketch->planeOrigin(ox, oy, oz);
    double nx, ny, nz;
    m_sketch->planeNormal(nx, ny, nz);

    QVector3D planeOrigin(static_cast<float>(ox),
                          static_cast<float>(oy),
                          static_cast<float>(oz));
    QVector3D planeNormal(static_cast<float>(nx),
                          static_cast<float>(ny),
                          static_cast<float>(nz));

    float denom = QVector3D::dotProduct(rayDir, planeNormal);
    if (std::abs(denom) < 1e-7f)
        return false;  // ray parallel to plane

    float t = QVector3D::dotProduct(planeOrigin - rayOrigin, planeNormal) / denom;
    QVector3D worldHit = rayOrigin + t * rayDir;

    // Project world hit into sketch 2D coords
    m_sketch->worldToSketch(
        static_cast<double>(worldHit.x()),
        static_cast<double>(worldHit.y()),
        static_cast<double>(worldHit.z()),
        sx, sy);

    return true;
}

double SketchEditor::snapToGrid(double val, double gridSize)
{
    if (!m_gridSnap) return val;
    return std::round(val / gridSize) * gridSize;
}

// =============================================================================
// Entity picking helpers
// =============================================================================

std::string SketchEditor::findNearestPoint(double sx, double sy, double threshold)
{
    if (!m_sketch) return {};

    std::string bestId;
    double bestDist = threshold;

    for (const auto& [pid, pt] : m_sketch->points()) {
        double dx = pt.x - sx;
        double dy = pt.y - sy;
        double dist = std::sqrt(dx * dx + dy * dy);
        if (dist < bestDist) {
            bestDist = dist;
            bestId = pid;
        }
    }
    return bestId;
}

std::string SketchEditor::findNearestLine(double sx, double sy, double threshold)
{
    if (!m_sketch) return {};

    std::string bestId;
    double bestDist = threshold;

    for (const auto& [lid, ln] : m_sketch->lines()) {
        const auto& p1 = m_sketch->point(ln.startPointId);
        const auto& p2 = m_sketch->point(ln.endPointId);

        // Point-to-segment distance
        double lx = p2.x - p1.x;
        double ly = p2.y - p1.y;
        double lenSq = lx * lx + ly * ly;
        if (lenSq < 1e-12) continue;

        double t = ((sx - p1.x) * lx + (sy - p1.y) * ly) / lenSq;
        t = std::clamp(t, 0.0, 1.0);

        double closestX = p1.x + t * lx;
        double closestY = p1.y + t * ly;
        double dx = sx - closestX;
        double dy = sy - closestY;
        double dist = std::sqrt(dx * dx + dy * dy);

        if (dist < bestDist) {
            bestDist = dist;
            bestId = lid;
        }
    }
    return bestId;
}

std::string SketchEditor::findNearestCircle(double sx, double sy, double threshold)
{
    if (!m_sketch) return {};

    std::string bestId;
    double bestDist = threshold;

    for (const auto& [cid, circ] : m_sketch->circles()) {
        const auto& cp = m_sketch->point(circ.centerPointId);
        double dx = sx - cp.x;
        double dy = sy - cp.y;
        double distToCenter = std::sqrt(dx * dx + dy * dy);
        // Distance to the circumference
        double distToCirc = std::abs(distToCenter - circ.radius);
        // Also consider distance to center point (for picking the circle itself)
        double dist = std::min(distToCirc, distToCenter);
        if (dist < bestDist) {
            bestDist = dist;
            bestId = cid;
        }
    }
    return bestId;
}

std::string SketchEditor::findNearestArc(double sx, double sy, double threshold)
{
    if (!m_sketch) return {};

    std::string bestId;
    double bestDist = threshold;

    for (const auto& [aid, arc] : m_sketch->arcs()) {
        const auto& cp = m_sketch->point(arc.centerPointId);
        double dx = sx - cp.x;
        double dy = sy - cp.y;
        double distToCenter = std::sqrt(dx * dx + dy * dy);
        double distToArc = std::abs(distToCenter - arc.radius);
        if (distToArc < bestDist) {
            bestDist = distToArc;
            bestId = aid;
        }
    }
    return bestId;
}

std::string SketchEditor::findNearestEllipse(double sx, double sy, double threshold)
{
    if (!m_sketch) return {};

    std::string bestId;
    double bestDist = threshold;

    for (const auto& [eid, ell] : m_sketch->ellipses()) {
        const auto& cp = m_sketch->point(ell.centerPointId);
        double dx = sx - cp.x;
        double dy = sy - cp.y;
        // Rotate into ellipse local frame
        double cosA = std::cos(-ell.rotationAngle);
        double sinA = std::sin(-ell.rotationAngle);
        double lx = dx * cosA - dy * sinA;
        double ly = dx * sinA + dy * cosA;
        // Normalized distance from ellipse boundary
        double a = ell.majorRadius, b = ell.minorRadius;
        if (a < 1e-9 || b < 1e-9) continue;
        double norm = std::sqrt((lx * lx) / (a * a) + (ly * ly) / (b * b));
        // Distance from boundary ~ |norm - 1| * average_radius
        double avgR = (a + b) * 0.5;
        double dist = std::abs(norm - 1.0) * avgR;
        if (dist < bestDist) {
            bestDist = dist;
            bestId = eid;
        }
    }
    return bestId;
}

SketchPickResult SketchEditor::pickEntity(double sx, double sy, double threshold)
{
    SketchPickResult result;
    result.sx = sx;
    result.sy = sy;

    // Priority: point > line > circle > arc > ellipse
    std::string ptId = findNearestPoint(sx, sy, threshold);
    if (!ptId.empty()) {
        result.kind = SketchPickResult::Point;
        result.entityId = ptId;
        return result;
    }

    std::string lineId = findNearestLine(sx, sy, threshold);
    if (!lineId.empty()) {
        result.kind = SketchPickResult::Line;
        result.entityId = lineId;
        return result;
    }

    std::string circId = findNearestCircle(sx, sy, threshold);
    if (!circId.empty()) {
        result.kind = SketchPickResult::Circle;
        result.entityId = circId;
        return result;
    }

    std::string arcId = findNearestArc(sx, sy, threshold);
    if (!arcId.empty()) {
        result.kind = SketchPickResult::Arc;
        result.entityId = arcId;
        return result;
    }

    std::string ellId = findNearestEllipse(sx, sy, threshold);
    if (!ellId.empty()) {
        result.kind = SketchPickResult::Ellipse;
        result.entityId = ellId;
        return result;
    }

    return result;  // Nothing
}

// =============================================================================
// Mouse handlers
// =============================================================================

bool SketchEditor::handleMousePress(QMouseEvent* event)
{
    if (!m_isEditing || !m_sketch)
        return false;

    if (event->button() != Qt::LeftButton)
        return false;

    double sx, sy;
    if (!screenToSketch(event->pos(), sx, sy))
        return false;

    // ── DRAG: always try drag first, regardless of active tool ──────────
    // In Fusion 360, you can grab and drag any sketch entity at any time.
    // Points drag directly; circles/arcs drag to change radius.
    {
        // 1. Try dragging a point (endpoint, center, control point)
        std::string ptId = findNearestPoint(sx, sy, 5.0);
        if (!ptId.empty() && !m_drawingInProgress) {
            m_isDragging = true;
            m_dragPointId = ptId;
            m_dragMode = DragMode::Point;
            if (m_viewport) {
                m_viewport->setCursor(Qt::ClosedHandCursor);
                m_viewport->update();
            }
            return true;
        }

        // 2. Try dragging a circle edge (change radius)
        if (!m_drawingInProgress) {
            std::string circId = findNearestCircle(sx, sy, 5.0);
            if (!circId.empty()) {
                const auto& circ = m_sketch->circle(circId);
                m_isDragging = true;
                m_dragPointId = circ.centerPointId;  // reference center
                m_dragCircleId = circId;
                m_dragMode = DragMode::CircleRadius;
                if (m_viewport) {
                    m_viewport->setCursor(Qt::SizeAllCursor);
                    m_viewport->update();
                }
                return true;
            }

            // 3. Try dragging an arc edge (change radius)
            std::string arcId = findNearestArc(sx, sy, 5.0);
            if (!arcId.empty()) {
                const auto& a = m_sketch->arc(arcId);
                m_isDragging = true;
                m_dragPointId = a.centerPointId;
                m_dragCircleId = arcId;
                m_dragMode = DragMode::ArcRadius;
                if (m_viewport) {
                    m_viewport->setCursor(Qt::SizeAllCursor);
                    m_viewport->update();
                }
                return true;
            }
        }
    }

    // ── Pointer (None) tool: deselect constraints ────────────────────────
    if (m_tool == SketchTool::None) {
        m_selectedConstraintId.clear();
        return false;
    }

    // ── Dimension tool ──────────────────────────────────────────────────
    if (m_tool == SketchTool::Dimension) {
        SketchPickResult pick = pickEntity(sx, sy);
        handleDimensionPick(pick);
        if (m_viewport) m_viewport->update();
        return true;
    }

    // ── AddConstraint tool ──────────────────────────────────────────────
    if (m_tool == SketchTool::AddConstraint) {
        SketchPickResult pick = pickEntity(sx, sy);
        handleConstraintPick(pick);
        if (m_viewport) m_viewport->update();
        return true;
    }

    // ── Trim tool ─────────────────────────────────────────────────────
    if (m_tool == SketchTool::Trim) {
        // Find nearest curve entity to click
        SketchPickResult pick = pickEntity(sx, sy);
        if (pick.kind == SketchPickResult::Line ||
            pick.kind == SketchPickResult::Circle ||
            pick.kind == SketchPickResult::Arc) {
            m_sketch->trim(pick.entityId, sx, sy);
            m_sketch->solve();
            if (m_viewport) m_viewport->update();
            emit sketchChanged();
        }
        return true;
    }

    // ── Extend tool ─────────────────────────────────────────────────────
    if (m_tool == SketchTool::Extend) {
        // Find the nearest endpoint; determine which entity it belongs to
        // and which end (0=start, 1=end) it is
        std::string bestPointId = findNearestPoint(sx, sy, 10.0);
        if (!bestPointId.empty()) {
            // Find the line or arc that owns this endpoint
            for (const auto& [lid, ln] : m_sketch->lines()) {
                if (ln.startPointId == bestPointId) {
                    m_sketch->extend(lid, 0);
                    m_sketch->solve();
                    if (m_viewport) m_viewport->update();
                    emit sketchChanged();
                    return true;
                }
                if (ln.endPointId == bestPointId) {
                    m_sketch->extend(lid, 1);
                    m_sketch->solve();
                    if (m_viewport) m_viewport->update();
                    emit sketchChanged();
                    return true;
                }
            }
            for (const auto& [aid, arc] : m_sketch->arcs()) {
                if (arc.startPointId == bestPointId) {
                    m_sketch->extend(aid, 0);
                    m_sketch->solve();
                    if (m_viewport) m_viewport->update();
                    emit sketchChanged();
                    return true;
                }
                if (arc.endPointId == bestPointId) {
                    m_sketch->extend(aid, 1);
                    m_sketch->solve();
                    if (m_viewport) m_viewport->update();
                    emit sketchChanged();
                    return true;
                }
            }
        }
        return true;
    }

    // ── Offset tool ─────────────────────────────────────────────────────
    if (m_tool == SketchTool::Offset) {
        if (!m_offsetPending) {
            // First click: select entity
            SketchPickResult pick = pickEntity(sx, sy);
            if (pick.kind == SketchPickResult::Line ||
                pick.kind == SketchPickResult::Circle ||
                pick.kind == SketchPickResult::Arc) {
                m_offsetEntityId = pick.entityId;
                m_offsetPending = true;
            }
        } else {
            // Second click: compute distance from entity to click point
            double dist = 0.0;
            if (m_sketch->lines().count(m_offsetEntityId)) {
                const auto& ln = m_sketch->line(m_offsetEntityId);
                const auto& p1 = m_sketch->point(ln.startPointId);
                const auto& p2 = m_sketch->point(ln.endPointId);
                double lx = p2.x - p1.x, ly = p2.y - p1.y;
                double lenSq = lx*lx + ly*ly;
                if (lenSq > 1e-12) {
                    // Signed distance: positive = left side of line direction
                    dist = ((sx - p1.x) * ly - (sy - p1.y) * lx) / std::sqrt(lenSq);
                }
            } else if (m_sketch->circles().count(m_offsetEntityId)) {
                const auto& circ = m_sketch->circle(m_offsetEntityId);
                const auto& cp = m_sketch->point(circ.centerPointId);
                double dx = sx - cp.x, dy = sy - cp.y;
                double distToCenter = std::sqrt(dx*dx + dy*dy);
                dist = distToCenter - circ.radius;
            } else if (m_sketch->arcs().count(m_offsetEntityId)) {
                const auto& arc = m_sketch->arc(m_offsetEntityId);
                const auto& cp = m_sketch->point(arc.centerPointId);
                double dx = sx - cp.x, dy = sy - cp.y;
                double distToCenter = std::sqrt(dx*dx + dy*dy);
                dist = distToCenter - arc.radius;
            }

            if (std::abs(dist) > 0.1) {
                m_sketch->offset({m_offsetEntityId}, dist);
                m_sketch->solve();
                emit sketchChanged();
            }
            m_offsetPending = false;
            m_offsetEntityId.clear();
            if (m_viewport) m_viewport->update();
        }
        return true;
    }

    // ── Project Edge tool ──────────────────────────────────────────────
    if (m_tool == SketchTool::ProjectEdge) {
        // In project edge mode, we use the 3D hit point from the viewport
        // and project it onto the sketch plane.  For a real implementation,
        // you would ray-cast to find the 3D edge and sample its polyline.
        // Here we create a construction point at the projected click.
        if (m_sketch) {
            m_sketch->addPoint(sx, sy, /*fixed=*/true);
            m_sketch->solve();
            if (m_viewport) m_viewport->update();
            emit sketchChanged();
        }
        return true;
    }

    // ── Sketch Fillet / Chamfer tools ────────────────────────────────────
    if (m_tool == SketchTool::SketchFillet || m_tool == SketchTool::SketchChamfer) {
        SketchPickResult pick = pickEntity(sx, sy);
        handleFilletChamferPick(pick);
        if (m_viewport) m_viewport->update();
        return true;
    }

    // ── Draw tools: snap to grid ────────────────────────────────────────
    sx = snapToGrid(sx);
    sy = snapToGrid(sy);

    if (m_tool == SketchTool::DrawSpline) {
        // Double-click (close to last point) finalizes the spline
        if (!m_splinePoints.empty()) {
            double dx = sx - m_splinePoints.back().first;
            double dy = sy - m_splinePoints.back().second;
            if (std::sqrt(dx * dx + dy * dy) < 2.0) {
                // Treat as double-click: finalize
                finalizeSpline();
                if (m_viewport) m_viewport->update();
                return true;
            }
        }
        // Each click adds a control point
        m_splinePoints.push_back({sx, sy});
        m_drawingInProgress = true;
        m_currentX = sx;
        m_currentY = sy;
        if (m_viewport) m_viewport->update();
        return true;
    }

    if (m_tool == SketchTool::DrawArc) {
        // Arc needs 3 clicks: center, start, end
        if (!m_drawingInProgress) {
            // First click: center
            m_startX = sx;
            m_startY = sy;
            m_drawingInProgress = true;
            m_arcClickCount = 1;
            m_currentX = sx;
            m_currentY = sy;
        } else if (m_arcClickCount == 1) {
            // Second click: start point on arc
            m_arcStartX = sx;
            m_arcStartY = sy;
            m_arcClickCount = 2;
            m_currentX = sx;
            m_currentY = sy;
        } else if (m_arcClickCount == 2) {
            // Third click: end point
            m_currentX = sx;
            m_currentY = sy;
            finalizeArc();
        }
        if (m_viewport) m_viewport->update();
        return true;
    }

    // ── Slot tool: 3 clicks (center1, center2, width) ─────────────────
    if (m_tool == SketchTool::DrawSlot) {
        if (m_slotClickCount == 0) {
            m_slotX1 = sx; m_slotY1 = sy;
            m_slotClickCount = 1;
            m_drawingInProgress = true;
            m_startX = sx; m_startY = sy;
            m_currentX = sx; m_currentY = sy;
        } else if (m_slotClickCount == 1) {
            m_slotX2 = sx; m_slotY2 = sy;
            m_slotClickCount = 2;
            m_currentX = sx; m_currentY = sy;
        } else if (m_slotClickCount == 2) {
            m_currentX = sx; m_currentY = sy;
            finalizeSlot();
        }
        if (m_viewport) m_viewport->update();
        return true;
    }

    // ── 3-point circle: 3 clicks ──────────────────────────────────────
    if (m_tool == SketchTool::DrawCircle3Point) {
        if (m_threePointClickCount == 0) {
            m_pt1X = sx; m_pt1Y = sy;
            m_threePointClickCount = 1;
            m_drawingInProgress = true;
            m_startX = sx; m_startY = sy;
            m_currentX = sx; m_currentY = sy;
        } else if (m_threePointClickCount == 1) {
            m_pt2X = sx; m_pt2Y = sy;
            m_threePointClickCount = 2;
            m_currentX = sx; m_currentY = sy;
        } else if (m_threePointClickCount == 2) {
            m_currentX = sx; m_currentY = sy;
            finalizeCircle3Point();
        }
        if (m_viewport) m_viewport->update();
        return true;
    }

    // ── 3-point arc: 3 clicks (start, mid, end) ──────────────────────
    if (m_tool == SketchTool::DrawArc3Point) {
        if (m_threePointClickCount == 0) {
            m_pt1X = sx; m_pt1Y = sy;
            m_threePointClickCount = 1;
            m_drawingInProgress = true;
            m_startX = sx; m_startY = sy;
            m_currentX = sx; m_currentY = sy;
        } else if (m_threePointClickCount == 1) {
            m_pt2X = sx; m_pt2Y = sy;
            m_threePointClickCount = 2;
            m_currentX = sx; m_currentY = sy;
        } else if (m_threePointClickCount == 2) {
            m_currentX = sx; m_currentY = sy;
            finalizeArc3Point();
        }
        if (m_viewport) m_viewport->update();
        return true;
    }

    if (!m_drawingInProgress) {
        // First click: record start position
        m_startX = sx;
        m_startY = sy;
        m_currentX = sx;
        m_currentY = sy;
        m_drawingInProgress = true;
    } else {
        // Second click: finalize the operation
        m_currentX = sx;
        m_currentY = sy;

        switch (m_tool) {
        case SketchTool::DrawLine:            finalizeLine();            break;
        case SketchTool::DrawRectangle:       finalizeRectangle();       break;
        case SketchTool::DrawCircle:          finalizeCircle();          break;
        case SketchTool::DrawEllipse:         finalizeEllipse();         break;
        case SketchTool::DrawPolygon:         finalizePolygon();         break;
        case SketchTool::DrawRectangleCenter: finalizeRectangleCenter(); break;
        default: break;
        }
    }

    if (m_viewport) m_viewport->update();
    return true;
}

bool SketchEditor::handleMouseMove(QMouseEvent* event)
{
    if (!m_isEditing || !m_sketch)
        return false;

    double sx, sy;
    if (!screenToSketch(event->pos(), sx, sy))
        return false;

    // ── Drag mode: move point or change radius, re-solve ───────────────
    if (m_isDragging) {
        sx = snapToGrid(sx);
        sy = snapToGrid(sy);

        if (m_dragMode == DragMode::Point && !m_dragPointId.empty()) {
            // Drag a point — move it and re-solve constraints
            auto& pt = m_sketch->point(m_dragPointId);
            pt.x = sx;
            pt.y = sy;
            m_sketch->solve();
        } else if (m_dragMode == DragMode::CircleRadius && !m_dragCircleId.empty()) {
            // Drag circle edge — change radius based on distance from center
            auto& circ = m_sketch->circle(m_dragCircleId);
            const auto& center = m_sketch->point(circ.centerPointId);
            double dx = sx - center.x, dy = sy - center.y;
            double newRadius = std::sqrt(dx * dx + dy * dy);
            if (newRadius > 0.1) {
                circ.radius = newRadius;
                m_sketch->solve();
            }
        } else if (m_dragMode == DragMode::ArcRadius && !m_dragCircleId.empty()) {
            // Drag arc edge — change radius
            auto& a = m_sketch->arc(m_dragCircleId);
            const auto& center = m_sketch->point(a.centerPointId);
            double dx = sx - center.x, dy = sy - center.y;
            double newRadius = std::sqrt(dx * dx + dy * dy);
            if (newRadius > 0.1) {
                a.radius = newRadius;
                m_sketch->solve();
            }
        }

        m_currentX = sx;
        m_currentY = sy;

        if (m_viewport) m_viewport->update();
        emit sketchChanged();
        return true;
    }

    // ── Rubber-band for draw tools ──────────────────────────────────────
    if (!m_drawingInProgress) {
        // Still track snapped cursor position for snap indicators
        m_currentX = snapToGrid(sx);
        m_currentY = snapToGrid(sy);
        m_inferenceLines.clear();
        if (m_viewport) m_viewport->update();
        return false;
    }

    sx = snapToGrid(sx);
    sy = snapToGrid(sy);

    // Compute inference lines and apply snap adjustments
    auto snapped = computeInferences(sx, sy);
    sx = snapped.first;
    sy = snapped.second;

    m_currentX = sx;
    m_currentY = sy;

    if (m_viewport) m_viewport->update();
    return true;
}

bool SketchEditor::handleMouseRelease(QMouseEvent* event)
{
    if (!m_isEditing)
        return false;

    // ── End drag ────────────────────────────────────────────────────────
    if (m_isDragging) {
        // Final solve
        if (m_sketch) {
            m_sketch->solve();
        }
        m_isDragging = false;
        m_dragPointId.clear();
        m_dragCircleId.clear();
        m_dragMode = DragMode::Point;
        if (m_viewport) {
            m_viewport->setCursor(Qt::CrossCursor);
            m_viewport->update();
        }
        emit sketchChanged();
        return true;
    }

    Q_UNUSED(event);
    // We use click-click for draw tools, not click-drag, so release is not consumed.
    return false;
}

bool SketchEditor::handleKeyPress(QKeyEvent* event)
{
    if (!m_isEditing)
        return false;

    switch (event->key()) {
    case Qt::Key_Escape:
        if (m_drawingInProgress) {
            // First Escape while drawing: cancel current draw, stay in same tool
            cancelDraw();
            if (m_viewport) m_viewport->update();
        } else if (m_firstPick.kind != SketchPickResult::Nothing) {
            // Cancel the first pick for Dimension/Constraint tool
            m_firstPick = {};
            if (m_viewport) m_viewport->update();
        } else if (m_tool != SketchTool::None) {
            // Escape when a tool is active but not drawing: exit tool to pointer mode
            setTool(SketchTool::None);
        } else {
            // Escape when already in pointer mode: finish sketch editing
            finishEditing();
        }
        return true;

    case Qt::Key_Return:
    case Qt::Key_Enter:
        // If drawing a spline, finalize it instead of finishing editing
        if (m_tool == SketchTool::DrawSpline && !m_splinePoints.empty()) {
            finalizeSpline();
            if (m_viewport) m_viewport->update();
            return true;
        }
        finishEditing();
        return true;

    case Qt::Key_L:
        setTool(SketchTool::DrawLine);
        return true;

    case Qt::Key_R:
        setTool(SketchTool::DrawRectangle);
        return true;

    case Qt::Key_C:
        setTool(SketchTool::DrawCircle);
        return true;

    case Qt::Key_A:
        setTool(SketchTool::DrawArc);
        return true;

    case Qt::Key_S:
        setTool(SketchTool::DrawSpline);
        return true;

    case Qt::Key_D:
        setTool(SketchTool::Dimension);
        return true;

    case Qt::Key_K:
        setTool(SketchTool::AddConstraint);
        return true;

    case Qt::Key_T:
        setTool(SketchTool::Trim);
        return true;

    case Qt::Key_E:
        setTool(SketchTool::Extend);
        return true;

    case Qt::Key_O:
        setTool(SketchTool::Offset);
        return true;

    case Qt::Key_P:
        setTool(SketchTool::ProjectEdge);
        return true;

    case Qt::Key_F:
        setTool(SketchTool::SketchFillet);
        return true;

    case Qt::Key_G:
        setTool(SketchTool::SketchChamfer);
        return true;

    case Qt::Key_H: {
        // Add Horizontal constraint to nearest line
        double sx = m_currentX, sy = m_currentY;
        std::string lineId = findNearestLine(sx, sy, 10.0);
        if (!lineId.empty()) {
            applyHorizontal(lineId);
        }
        return true;
    }

    case Qt::Key_V: {
        // Add Vertical constraint to nearest line
        double sx = m_currentX, sy = m_currentY;
        std::string lineId = findNearestLine(sx, sy, 10.0);
        if (!lineId.empty()) {
            applyVertical(lineId);
        }
        return true;
    }

    case Qt::Key_X: {
        // Toggle construction mode on the nearest line or arc
        if (!m_sketch) return true;
        double sx = m_currentX, sy = m_currentY;
        std::string lineId = findNearestLine(sx, sy, 10.0);
        if (!lineId.empty()) {
            auto& ln = m_sketch->line(lineId);
            ln.isConstruction = !ln.isConstruction;
            if (m_viewport) m_viewport->update();
            emit sketchChanged();
        } else {
            std::string arcId = findNearestArc(sx, sy, 10.0);
            if (!arcId.empty()) {
                auto& a = m_sketch->arc(arcId);
                a.isConstruction = !a.isConstruction;
                if (m_viewport) m_viewport->update();
                emit sketchChanged();
            }
        }
        return true;
    }

    case Qt::Key_Delete:
        // Remove selected constraint
        if (!m_selectedConstraintId.empty() && m_sketch) {
            m_sketch->removeConstraint(m_selectedConstraintId);
            m_selectedConstraintId.clear();
            m_sketch->solve();
            if (m_viewport) m_viewport->update();
            emit sketchChanged();
        }
        return true;

    default:
        break;
    }

    return false;
}

// =============================================================================
// Dimension tool pick handler
// =============================================================================

void SketchEditor::handleDimensionPick(const SketchPickResult& pick)
{
    if (!m_sketch) return;

    // If nothing was picked and we have no first pick, ignore
    if (pick.kind == SketchPickResult::Nothing && m_firstPick.kind == SketchPickResult::Nothing)
        return;

    // ── Single-entity dimension: circle/arc → Radius ────────────────────
    if (m_firstPick.kind == SketchPickResult::Nothing) {
        // First pick
        if (pick.kind == SketchPickResult::Circle) {
            // Radius constraint on a circle — single pick is enough
            const auto& circ = m_sketch->circle(pick.entityId);
            bool ok = false;
            double val = QInputDialog::getDouble(
                m_viewport, "Radius", "Enter radius value:",
                circ.radius, 0.001, 1e6, 3, &ok);
            if (ok) {
                m_sketch->addConstraint(
                    sketch::ConstraintType::Radius,
                    {pick.entityId}, val);
                m_sketch->solve();
                emit sketchChanged();
            }
            m_firstPick = {};
            return;
        }

        if (pick.kind == SketchPickResult::Arc) {
            const auto& a = m_sketch->arc(pick.entityId);
            bool ok = false;
            double val = QInputDialog::getDouble(
                m_viewport, "Radius", "Enter arc radius value:",
                a.radius, 0.001, 1e6, 3, &ok);
            if (ok) {
                m_sketch->addConstraint(
                    sketch::ConstraintType::Radius,
                    {pick.entityId}, val);
                m_sketch->solve();
                emit sketchChanged();
            }
            m_firstPick = {};
            return;
        }

        if (pick.kind == SketchPickResult::Line) {
            // Line solo → Distance constraint (line length).
            // But user might want to pick a second entity, so store as first pick.
            // If the user clicks empty space next, we treat it as line-length.
            m_firstPick = pick;
            return;
        }

        if (pick.kind == SketchPickResult::Point) {
            // Store as first pick, wait for second
            m_firstPick = pick;
            return;
        }
        return;
    }

    // ── Second pick ─────────────────────────────────────────────────────

    // First pick was a Line, second pick is Nothing → line length
    if (m_firstPick.kind == SketchPickResult::Line &&
        pick.kind == SketchPickResult::Nothing) {
        const auto& ln = m_sketch->line(m_firstPick.entityId);
        const auto& p1 = m_sketch->point(ln.startPointId);
        const auto& p2 = m_sketch->point(ln.endPointId);
        double len = std::sqrt((p2.x - p1.x) * (p2.x - p1.x) +
                               (p2.y - p1.y) * (p2.y - p1.y));
        bool ok = false;
        double val = QInputDialog::getDouble(
            m_viewport, "Distance", "Enter line length:",
            len, 0.001, 1e6, 3, &ok);
        if (ok) {
            m_sketch->addConstraint(
                sketch::ConstraintType::Distance,
                {ln.startPointId, ln.endPointId}, val);
            m_sketch->solve();
            emit sketchChanged();
        }
        m_firstPick = {};
        return;
    }

    // Two points → Distance
    if (m_firstPick.kind == SketchPickResult::Point &&
        pick.kind == SketchPickResult::Point) {
        const auto& p1 = m_sketch->point(m_firstPick.entityId);
        const auto& p2 = m_sketch->point(pick.entityId);
        double dist = std::sqrt((p2.x - p1.x) * (p2.x - p1.x) +
                                (p2.y - p1.y) * (p2.y - p1.y));
        bool ok = false;
        double val = QInputDialog::getDouble(
            m_viewport, "Distance", "Enter distance between points:",
            dist, 0.001, 1e6, 3, &ok);
        if (ok) {
            m_sketch->addConstraint(
                sketch::ConstraintType::Distance,
                {m_firstPick.entityId, pick.entityId}, val);
            m_sketch->solve();
            emit sketchChanged();
        }
        m_firstPick = {};
        return;
    }

    // Point + Line → DistancePointLine
    if ((m_firstPick.kind == SketchPickResult::Point && pick.kind == SketchPickResult::Line) ||
        (m_firstPick.kind == SketchPickResult::Line && pick.kind == SketchPickResult::Point)) {
        std::string ptId = (m_firstPick.kind == SketchPickResult::Point)
                               ? m_firstPick.entityId : pick.entityId;
        std::string lnId = (m_firstPick.kind == SketchPickResult::Line)
                               ? m_firstPick.entityId : pick.entityId;
        // Compute current distance
        const auto& pt = m_sketch->point(ptId);
        const auto& ln = m_sketch->line(lnId);
        const auto& lp1 = m_sketch->point(ln.startPointId);
        const auto& lp2 = m_sketch->point(ln.endPointId);
        double lx = lp2.x - lp1.x, ly = lp2.y - lp1.y;
        double lenSq = lx * lx + ly * ly;
        double dist = 0;
        if (lenSq > 1e-12) {
            dist = std::abs((pt.x - lp1.x) * ly - (pt.y - lp1.y) * lx) / std::sqrt(lenSq);
        }
        bool ok = false;
        double val = QInputDialog::getDouble(
            m_viewport, "Distance", "Enter point-to-line distance:",
            dist, 0.0, 1e6, 3, &ok);
        if (ok) {
            m_sketch->addConstraint(
                sketch::ConstraintType::DistancePointLine,
                {ptId, lnId}, val);
            m_sketch->solve();
            emit sketchChanged();
        }
        m_firstPick = {};
        return;
    }

    // Line + second pick is Nothing → line length (same as above)
    if (m_firstPick.kind == SketchPickResult::Line) {
        const auto& ln = m_sketch->line(m_firstPick.entityId);
        const auto& p1 = m_sketch->point(ln.startPointId);
        const auto& p2 = m_sketch->point(ln.endPointId);
        double len = std::sqrt((p2.x - p1.x) * (p2.x - p1.x) +
                               (p2.y - p1.y) * (p2.y - p1.y));
        bool ok = false;
        double val = QInputDialog::getDouble(
            m_viewport, "Distance", "Enter line length:",
            len, 0.001, 1e6, 3, &ok);
        if (ok) {
            m_sketch->addConstraint(
                sketch::ConstraintType::Distance,
                {ln.startPointId, ln.endPointId}, val);
            m_sketch->solve();
            emit sketchChanged();
        }
        m_firstPick = {};
        return;
    }

    // Point + empty → just reset (no meaningful single-point dimension)
    m_firstPick = {};
}

// =============================================================================
// AddConstraint tool pick handler
// =============================================================================

void SketchEditor::handleConstraintPick(const SketchPickResult& pick)
{
    if (!m_sketch) return;

    if (pick.kind == SketchPickResult::Nothing && m_firstPick.kind == SketchPickResult::Nothing)
        return;

    // ── Single-entity constraints ───────────────────────────────────────

    // First pick
    if (m_firstPick.kind == SketchPickResult::Nothing) {
        if (pick.kind == SketchPickResult::Line) {
            // Line alone → check if roughly horizontal or vertical
            const auto& ln = m_sketch->line(pick.entityId);
            const auto& p1 = m_sketch->point(ln.startPointId);
            const auto& p2 = m_sketch->point(ln.endPointId);
            double dx = std::abs(p2.x - p1.x);
            double dy = std::abs(p2.y - p1.y);
            double len = std::sqrt(dx * dx + dy * dy);
            if (len < 1e-6) {
                m_firstPick = {};
                return;
            }
            double hRatio = dy / len;  // 0 = perfectly horizontal
            double vRatio = dx / len;  // 0 = perfectly vertical

            constexpr double kThreshold = 0.3;  // ~17 degrees tolerance
            if (hRatio < kThreshold) {
                // Roughly horizontal
                applyHorizontal(pick.entityId);
                m_firstPick = {};
                return;
            } else if (vRatio < kThreshold) {
                // Roughly vertical
                applyVertical(pick.entityId);
                m_firstPick = {};
                return;
            }
            // Not clearly H or V — wait for second pick
        }
        m_firstPick = pick;
        return;
    }

    // ── Second pick ─────────────────────────────────────────────────────

    // Two points → Coincident
    if (m_firstPick.kind == SketchPickResult::Point &&
        pick.kind == SketchPickResult::Point) {
        m_sketch->addConstraint(
            sketch::ConstraintType::Coincident,
            {m_firstPick.entityId, pick.entityId});
        m_sketch->solve();
        emit sketchChanged();
        m_firstPick = {};
        return;
    }

    // Point + Line → PointOnLine
    if ((m_firstPick.kind == SketchPickResult::Point && pick.kind == SketchPickResult::Line) ||
        (m_firstPick.kind == SketchPickResult::Line && pick.kind == SketchPickResult::Point)) {
        std::string ptId = (m_firstPick.kind == SketchPickResult::Point)
                               ? m_firstPick.entityId : pick.entityId;
        std::string lnId = (m_firstPick.kind == SketchPickResult::Line)
                               ? m_firstPick.entityId : pick.entityId;
        m_sketch->addConstraint(
            sketch::ConstraintType::PointOnLine,
            {ptId, lnId});
        m_sketch->solve();
        emit sketchChanged();
        m_firstPick = {};
        return;
    }

    // Two lines → Parallel or Perpendicular
    if (m_firstPick.kind == SketchPickResult::Line &&
        pick.kind == SketchPickResult::Line) {
        const auto& ln1 = m_sketch->line(m_firstPick.entityId);
        const auto& ln2 = m_sketch->line(pick.entityId);
        const auto& a1 = m_sketch->point(ln1.startPointId);
        const auto& a2 = m_sketch->point(ln1.endPointId);
        const auto& b1 = m_sketch->point(ln2.startPointId);
        const auto& b2 = m_sketch->point(ln2.endPointId);

        double d1x = a2.x - a1.x, d1y = a2.y - a1.y;
        double d2x = b2.x - b1.x, d2y = b2.y - b1.y;
        double len1 = std::sqrt(d1x * d1x + d1y * d1y);
        double len2 = std::sqrt(d2x * d2x + d2y * d2y);

        if (len1 > 1e-6 && len2 > 1e-6) {
            double dot = (d1x * d2x + d1y * d2y) / (len1 * len2);
            double cross = (d1x * d2y - d1y * d2x) / (len1 * len2);

            // If mostly parallel (|dot| > 0.7) → Parallel
            // If mostly perpendicular (|cross| > 0.7 or |dot| < 0.3) → Perpendicular
            if (std::abs(dot) > 0.7) {
                m_sketch->addConstraint(
                    sketch::ConstraintType::Parallel,
                    {m_firstPick.entityId, pick.entityId});
            } else if (std::abs(cross) > 0.7) {
                m_sketch->addConstraint(
                    sketch::ConstraintType::Perpendicular,
                    {m_firstPick.entityId, pick.entityId});
            }
            m_sketch->solve();
            emit sketchChanged();
        }
        m_firstPick = {};
        return;
    }

    // Point + Circle → PointOnCircle
    if ((m_firstPick.kind == SketchPickResult::Point && pick.kind == SketchPickResult::Circle) ||
        (m_firstPick.kind == SketchPickResult::Circle && pick.kind == SketchPickResult::Point)) {
        std::string ptId = (m_firstPick.kind == SketchPickResult::Point)
                               ? m_firstPick.entityId : pick.entityId;
        std::string circId = (m_firstPick.kind == SketchPickResult::Circle)
                                 ? m_firstPick.entityId : pick.entityId;
        m_sketch->addConstraint(
            sketch::ConstraintType::PointOnCircle,
            {ptId, circId});
        m_sketch->solve();
        emit sketchChanged();
        m_firstPick = {};
        return;
    }

    // Two circles → Concentric
    if (m_firstPick.kind == SketchPickResult::Circle &&
        pick.kind == SketchPickResult::Circle) {
        m_sketch->addConstraint(
            sketch::ConstraintType::Concentric,
            {m_firstPick.entityId, pick.entityId});
        m_sketch->solve();
        emit sketchChanged();
        m_firstPick = {};
        return;
    }

    // Fallback: reset picks
    m_firstPick = {};
}

// =============================================================================
// Horizontal / Vertical helpers
// =============================================================================

void SketchEditor::applyHorizontal(const std::string& lineId)
{
    if (!m_sketch) return;
    m_sketch->addConstraint(
        sketch::ConstraintType::Horizontal, {lineId});
    m_sketch->solve();
    if (m_viewport) m_viewport->update();
    emit sketchChanged();
}

void SketchEditor::applyVertical(const std::string& lineId)
{
    if (!m_sketch) return;
    m_sketch->addConstraint(
        sketch::ConstraintType::Vertical, {lineId});
    m_sketch->solve();
    if (m_viewport) m_viewport->update();
    emit sketchChanged();
}

std::string SketchEditor::findLineForShortcut(double sx, double sy)
{
    return findNearestLine(sx, sy, 10.0);
}

// =============================================================================
// Auto-constraint: apply H/V to nearly axis-aligned lines on draw
// =============================================================================

void SketchEditor::autoConstrainLastEntity(const std::string& entityId)
{
    if (!m_sketch) return;

    // Check if the entity is a line
    const auto& allLines = m_sketch->lines();
    auto lineIt = allLines.find(entityId);
    if (lineIt != allLines.end()) {
        const auto& ln = lineIt->second;
        const auto& p1 = m_sketch->point(ln.startPointId);
        const auto& p2 = m_sketch->point(ln.endPointId);

        double dx = p2.x - p1.x;
        double dy = p2.y - p1.y;
        double angle = std::atan2(std::abs(dy), std::abs(dx)) * 180.0 / M_PI;

        // Check that no H/V constraint already exists for this line
        bool hasHV = false;
        for (const auto& [cid, con] : m_sketch->constraints()) {
            if (con.type == sketch::ConstraintType::Horizontal ||
                con.type == sketch::ConstraintType::Vertical) {
                for (const auto& eid : con.entityIds) {
                    if (eid == entityId) { hasHV = true; break; }
                }
                if (hasHV) break;
            }
        }
        if (hasHV) return;

        // Auto horizontal if within 3 degrees
        if (angle < 3.0) {
            m_sketch->addConstraint(sketch::ConstraintType::Horizontal, {entityId});
            m_sketch->solve();
        }
        // Auto vertical if within 3 degrees of 90
        else if (std::abs(angle - 90.0) < 3.0) {
            m_sketch->addConstraint(sketch::ConstraintType::Vertical, {entityId});
            m_sketch->solve();
        }
    }
}

// =============================================================================
// Finalize operations
// =============================================================================

void SketchEditor::finalizeLine()
{
    if (!m_sketch) return;

    double x1 = m_startX, y1 = m_startY;
    double x2 = m_currentX, y2 = m_currentY;

    // Skip degenerate lines
    double len = std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
    if (len < 0.1) {
        m_drawingInProgress = false;
        return;
    }

    std::string lineId = m_sketch->addLine(x1, y1, x2, y2);
    m_sketch->solve();

    // Always auto-apply H/V constraints to nearly-aligned lines
    autoConstrainLastEntity(lineId);

    // Auto-dimension: add a Distance constraint showing the line length
    {
        const auto& ln = m_sketch->line(lineId);
        m_sketch->addConstraint(sketch::ConstraintType::Distance,
                                {ln.startPointId, ln.endPointId}, len);
    }

    if (m_autoConstrain)
        m_sketch->autoConstrain();

    m_drawingInProgress = false;
    emit sketchChanged();

    // Chain: start next line from the endpoint of the previous one
    m_startX = x2;
    m_startY = y2;
    m_currentX = x2;
    m_currentY = y2;
    m_drawingInProgress = true;
}

void SketchEditor::finalizeRectangle()
{
    if (!m_sketch) return;

    double x1 = m_startX, y1 = m_startY;
    double x2 = m_currentX, y2 = m_currentY;

    // Skip degenerate rectangles
    if (std::abs(x2 - x1) < 0.1 || std::abs(y2 - y1) < 0.1) {
        m_drawingInProgress = false;
        return;
    }

    // Create 4 corner points and 4 lines
    std::string p0 = m_sketch->addPoint(x1, y1);
    std::string p1 = m_sketch->addPoint(x2, y1);
    std::string p2 = m_sketch->addPoint(x2, y2);
    std::string p3 = m_sketch->addPoint(x1, y2);

    std::string lb = m_sketch->addLine(p0, p1);  // bottom
    std::string lr = m_sketch->addLine(p1, p2);  // right
    std::string lt = m_sketch->addLine(p2, p3);  // top
    std::string ll = m_sketch->addLine(p3, p0);  // left

    m_sketch->solve();

    // Auto-apply H/V constraints to rectangle edges
    autoConstrainLastEntity(lb);
    autoConstrainLastEntity(lr);
    autoConstrainLastEntity(lt);
    autoConstrainLastEntity(ll);

    // Auto-dimension: width (bottom edge) and height (right edge)
    {
        double width  = std::abs(x2 - x1);
        double height = std::abs(y2 - y1);
        m_sketch->addConstraint(sketch::ConstraintType::Distance, {p0, p1}, width);
        m_sketch->addConstraint(sketch::ConstraintType::Distance, {p1, p2}, height);
    }

    if (m_autoConstrain)
        m_sketch->autoConstrain();

    m_drawingInProgress = false;
    emit sketchChanged();
}

void SketchEditor::finalizeCircle()
{
    if (!m_sketch) return;

    double cx = m_startX, cy = m_startY;
    double dx = m_currentX - cx, dy = m_currentY - cy;
    double radius = std::sqrt(dx * dx + dy * dy);

    if (radius < 0.1) {
        m_drawingInProgress = false;
        return;
    }

    std::string circleId = m_sketch->addCircle(cx, cy, radius);
    m_sketch->solve();

    // Auto-dimension: add a Radius constraint showing the circle's radius
    m_sketch->addConstraint(sketch::ConstraintType::Radius, {circleId}, radius);

    if (m_autoConstrain)
        m_sketch->autoConstrain();

    m_drawingInProgress = false;
    emit sketchChanged();
}

void SketchEditor::finalizeArc()
{
    if (!m_sketch) return;

    double cx = m_startX, cy = m_startY;
    double dx1 = m_arcStartX - cx, dy1 = m_arcStartY - cy;
    double radius = std::sqrt(dx1 * dx1 + dy1 * dy1);

    if (radius < 0.1) {
        m_drawingInProgress = false;
        m_arcClickCount = 0;
        return;
    }

    double startAngle = std::atan2(dy1, dx1);
    double endAngle = std::atan2(m_currentY - cy, m_currentX - cx);

    m_sketch->addArc(cx, cy, radius, startAngle, endAngle);
    m_sketch->solve();

    if (m_autoConstrain)
        m_sketch->autoConstrain();

    m_drawingInProgress = false;
    m_arcClickCount = 0;
    emit sketchChanged();
}

void SketchEditor::finalizeSpline()
{
    if (!m_sketch || m_splinePoints.size() < 2) {
        m_splinePoints.clear();
        m_drawingInProgress = false;
        return;
    }

    m_sketch->addSpline(m_splinePoints);
    m_sketch->solve();

    if (m_autoConstrain)
        m_sketch->autoConstrain();

    m_splinePoints.clear();
    m_drawingInProgress = false;
    emit sketchChanged();
}

void SketchEditor::finalizeEllipse()
{
    if (!m_sketch) return;

    double cx = m_startX, cy = m_startY;
    double dx = m_currentX - cx, dy = m_currentY - cy;
    double majorR = std::sqrt(dx * dx + dy * dy);

    if (majorR < 0.1) {
        m_drawingInProgress = false;
        return;
    }

    double rotation = std::atan2(dy, dx);
    double minorR = majorR * 0.5;  // default aspect ratio; user can edit after

    m_sketch->addEllipse(cx, cy, majorR, minorR, rotation);
    m_sketch->solve();

    if (m_autoConstrain)
        m_sketch->autoConstrain();

    m_drawingInProgress = false;
    emit sketchChanged();
}

void SketchEditor::finalizePolygon()
{
    if (!m_sketch) return;

    double cx = m_startX, cy = m_startY;
    double dx = m_currentX - cx, dy = m_currentY - cy;
    double radius = std::sqrt(dx * dx + dy * dy);

    if (radius < 0.1) {
        m_drawingInProgress = false;
        return;
    }

    int n = m_polygonSides;
    double baseAngle = std::atan2(dy, dx);
    bool construction = m_constructionMode;

    // Create N points around the center
    std::vector<std::string> ptIds;
    ptIds.reserve(n);
    for (int i = 0; i < n; ++i) {
        double angle = baseAngle + 2.0 * M_PI * i / n;
        double px = cx + radius * std::cos(angle);
        double py = cy + radius * std::sin(angle);
        ptIds.push_back(m_sketch->addPoint(px, py));
    }

    // Create N lines forming the polygon
    std::vector<std::string> lineIds;
    lineIds.reserve(n);
    for (int i = 0; i < n; ++i) {
        int next = (i + 1) % n;
        lineIds.push_back(m_sketch->addLine(ptIds[i], ptIds[next], construction));
    }

    // Add coincident constraints at each vertex to close the polygon
    // (already closed by sharing point IDs)

    m_sketch->solve();

    // Auto-apply H/V constraints to nearly-aligned polygon edges
    for (const auto& lid : lineIds)
        autoConstrainLastEntity(lid);

    if (m_autoConstrain)
        m_sketch->autoConstrain();

    m_drawingInProgress = false;
    emit sketchChanged();
}

void SketchEditor::finalizeSlot()
{
    if (!m_sketch) return;

    double x1 = m_slotX1, y1 = m_slotY1;
    double x2 = m_slotX2, y2 = m_slotY2;

    // Width = distance from slot axis to the click point
    double axDx = x2 - x1, axDy = y2 - y1;
    double axLen = std::sqrt(axDx * axDx + axDy * axDy);
    if (axLen < 0.1) {
        m_drawingInProgress = false;
        m_slotClickCount = 0;
        return;
    }

    // Perpendicular distance from axis to current mouse = half-width
    double px = m_currentX - x1, py = m_currentY - y1;
    double halfWidth = std::abs(px * axDy - py * axDx) / axLen;
    if (halfWidth < 0.1) halfWidth = 2.0;

    // Normal direction perpendicular to axis
    double nx = -axDy / axLen, ny = axDx / axLen;
    bool construction = m_constructionMode;

    // 4 corner points
    std::string p1 = m_sketch->addPoint(x1 + halfWidth * nx, y1 + halfWidth * ny);
    std::string p2 = m_sketch->addPoint(x2 + halfWidth * nx, y2 + halfWidth * ny);
    std::string p3 = m_sketch->addPoint(x2 - halfWidth * nx, y2 - halfWidth * ny);
    std::string p4 = m_sketch->addPoint(x1 - halfWidth * nx, y1 - halfWidth * ny);

    // Two parallel lines
    std::string sl1 = m_sketch->addLine(p1, p2, construction);
    std::string sl2 = m_sketch->addLine(p3, p4, construction);

    // Two semicircular arcs at each end
    std::string c1 = m_sketch->addPoint(x1, y1);  // center of arc 1
    std::string c2 = m_sketch->addPoint(x2, y2);  // center of arc 2
    m_sketch->addArc(c1, p4, p1, halfWidth, construction);
    m_sketch->addArc(c2, p2, p3, halfWidth, construction);

    m_sketch->solve();

    // Auto-apply H/V constraints to slot lines
    autoConstrainLastEntity(sl1);
    autoConstrainLastEntity(sl2);

    if (m_autoConstrain)
        m_sketch->autoConstrain();

    m_drawingInProgress = false;
    m_slotClickCount = 0;
    emit sketchChanged();
}

/// Compute circumscribed circle center from 3 points.
/// Returns false if the points are collinear.
static bool circumscribedCircle(double ax, double ay, double bx, double by,
                                double cx, double cy,
                                double& outCx, double& outCy, double& outR)
{
    double D = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    if (std::abs(D) < 1e-9)
        return false;

    double aSq = ax * ax + ay * ay;
    double bSq = bx * bx + by * by;
    double cSq = cx * cx + cy * cy;

    outCx = (aSq * (by - cy) + bSq * (cy - ay) + cSq * (ay - by)) / D;
    outCy = (aSq * (cx - bx) + bSq * (ax - cx) + cSq * (bx - ax)) / D;
    outR = std::sqrt((outCx - ax) * (outCx - ax) + (outCy - ay) * (outCy - ay));
    return true;
}

void SketchEditor::finalizeCircle3Point()
{
    if (!m_sketch) return;

    double cx, cy, r;
    if (!circumscribedCircle(m_pt1X, m_pt1Y, m_pt2X, m_pt2Y,
                             m_currentX, m_currentY, cx, cy, r)) {
        m_drawingInProgress = false;
        m_threePointClickCount = 0;
        return;
    }

    if (r < 0.1) {
        m_drawingInProgress = false;
        m_threePointClickCount = 0;
        return;
    }

    m_sketch->addCircle(cx, cy, r);
    m_sketch->solve();

    if (m_autoConstrain)
        m_sketch->autoConstrain();

    m_drawingInProgress = false;
    m_threePointClickCount = 0;
    emit sketchChanged();
}

void SketchEditor::finalizeArc3Point()
{
    if (!m_sketch) return;

    double cx, cy, r;
    if (!circumscribedCircle(m_pt1X, m_pt1Y, m_pt2X, m_pt2Y,
                             m_currentX, m_currentY, cx, cy, r)) {
        m_drawingInProgress = false;
        m_threePointClickCount = 0;
        return;
    }

    if (r < 0.1) {
        m_drawingInProgress = false;
        m_threePointClickCount = 0;
        return;
    }

    // Create arc from start (pt1) to end (current), ensuring it passes through pt2
    double startAngle = std::atan2(m_pt1Y - cy, m_pt1X - cx);
    double midAngle   = std::atan2(m_pt2Y - cy, m_pt2X - cx);
    double endAngle   = std::atan2(m_currentY - cy, m_currentX - cx);

    // Determine correct winding: if mid is not between start and end (CCW),
    // swap start/end to ensure the arc passes through the midpoint
    auto normalizeAngle = [](double a) -> double {
        a = std::fmod(a, 2.0 * M_PI);
        if (a < 0) a += 2.0 * M_PI;
        return a;
    };

    double sa = normalizeAngle(startAngle);
    double ma = normalizeAngle(midAngle);
    double ea = normalizeAngle(endAngle);

    // Check if mid is in the CCW sweep from start to end
    double sweep = normalizeAngle(ea - sa);
    double midInSweep = normalizeAngle(ma - sa);

    if (midInSweep > sweep) {
        // Mid is outside the CCW arc, so swap start and end
        std::swap(sa, ea);
        startAngle = sa;
        endAngle = ea;
    } else {
        startAngle = sa;
        endAngle = ea;
    }

    m_sketch->addArc(cx, cy, r, startAngle, endAngle);
    m_sketch->solve();

    if (m_autoConstrain)
        m_sketch->autoConstrain();

    m_drawingInProgress = false;
    m_threePointClickCount = 0;
    emit sketchChanged();
}

void SketchEditor::finalizeRectangleCenter()
{
    if (!m_sketch) return;

    double cx = m_startX, cy = m_startY;
    double dx = std::abs(m_currentX - cx);
    double dy = std::abs(m_currentY - cy);

    if (dx < 0.1 || dy < 0.1) {
        m_drawingInProgress = false;
        return;
    }

    bool construction = m_constructionMode;

    // Create 4 corner points symmetrically around center
    std::string p0 = m_sketch->addPoint(cx - dx, cy - dy);
    std::string p1 = m_sketch->addPoint(cx + dx, cy - dy);
    std::string p2 = m_sketch->addPoint(cx + dx, cy + dy);
    std::string p3 = m_sketch->addPoint(cx - dx, cy + dy);

    std::string rcl0 = m_sketch->addLine(p0, p1, construction);
    std::string rcl1 = m_sketch->addLine(p1, p2, construction);
    std::string rcl2 = m_sketch->addLine(p2, p3, construction);
    std::string rcl3 = m_sketch->addLine(p3, p0, construction);

    m_sketch->solve();

    // Auto-apply H/V constraints to center-rectangle edges
    autoConstrainLastEntity(rcl0);
    autoConstrainLastEntity(rcl1);
    autoConstrainLastEntity(rcl2);
    autoConstrainLastEntity(rcl3);

    if (m_autoConstrain)
        m_sketch->autoConstrain();

    m_drawingInProgress = false;
    emit sketchChanged();
}

// =============================================================================
// Toggle Construction
// =============================================================================

void SketchEditor::toggleConstruction(double sx, double sy)
{
    if (!m_sketch) return;

    // Try to find a nearby entity and toggle its construction flag
    std::string lineId = findNearestLine(sx, sy, 10.0);
    if (!lineId.empty()) {
        auto& ln = m_sketch->line(lineId);
        ln.isConstruction = !ln.isConstruction;
        if (m_viewport) m_viewport->update();
        emit sketchChanged();
        return;
    }

    std::string circId = findNearestCircle(sx, sy, 10.0);
    if (!circId.empty()) {
        auto& c = m_sketch->circle(circId);
        c.isConstruction = !c.isConstruction;
        if (m_viewport) m_viewport->update();
        emit sketchChanged();
        return;
    }

    std::string arcId = findNearestArc(sx, sy, 10.0);
    if (!arcId.empty()) {
        auto& a = m_sketch->arc(arcId);
        a.isConstruction = !a.isConstruction;
        if (m_viewport) m_viewport->update();
        emit sketchChanged();
        return;
    }

    std::string ellId = findNearestEllipse(sx, sy, 10.0);
    if (!ellId.empty()) {
        auto& e = m_sketch->ellipse(ellId);
        e.isConstruction = !e.isConstruction;
        if (m_viewport) m_viewport->update();
        emit sketchChanged();
        return;
    }

    // No entity nearby -- toggle global construction mode
    m_constructionMode = !m_constructionMode;
    if (m_viewport) m_viewport->update();
}

void SketchEditor::cancelDraw()
{
    m_drawingInProgress = false;
    m_arcClickCount = 0;
    m_splinePoints.clear();
    m_offsetPending = false;
    m_offsetEntityId.clear();
    m_slotClickCount = 0;
    m_threePointClickCount = 0;
    if (m_viewport) m_viewport->update();
}

// =============================================================================
// Sketch Fillet / Chamfer pick handler
// =============================================================================

void SketchEditor::handleFilletChamferPick(const SketchPickResult& pick)
{
    if (!m_sketch) return;

    if (pick.kind != SketchPickResult::Line) {
        // Only lines are supported for 2D fillet/chamfer
        m_firstPick = {};
        return;
    }

    if (m_firstPick.kind == SketchPickResult::Nothing) {
        // First pick: store line
        m_firstPick = pick;
        return;
    }

    // Second pick: must be a different line
    if (m_firstPick.entityId == pick.entityId) {
        m_firstPick = {};
        return;
    }

    if (m_tool == SketchTool::SketchFillet) {
        bool ok = false;
        double radius = QInputDialog::getDouble(
            m_viewport, "Sketch Fillet", "Enter fillet radius:",
            3.0, 0.001, 1e6, 3, &ok);
        if (ok) {
            m_sketch->sketchFillet(m_firstPick.entityId, pick.entityId, radius);
            m_sketch->solve();
            emit sketchChanged();
        }
    } else {
        // SketchChamfer
        bool ok = false;
        double distance = QInputDialog::getDouble(
            m_viewport, "Sketch Chamfer", "Enter chamfer distance:",
            3.0, 0.001, 1e6, 3, &ok);
        if (ok) {
            m_sketch->sketchChamfer(m_firstPick.entityId, pick.entityId, distance);
            m_sketch->solve();
            emit sketchChanged();
        }
    }

    m_firstPick = {};
}

// =============================================================================
// Find nearest curve (line, circle, or arc)
// =============================================================================

std::string SketchEditor::findNearestCurve(double sx, double sy, double threshold)
{
    std::string lineId = findNearestLine(sx, sy, threshold);
    if (!lineId.empty()) return lineId;

    std::string circId = findNearestCircle(sx, sy, threshold);
    if (!circId.empty()) return circId;

    std::string arcId = findNearestArc(sx, sy, threshold);
    if (!arcId.empty()) return arcId;

    return {};
}

// =============================================================================
// Snap / Inference computation
// =============================================================================

std::pair<double,double> SketchEditor::computeInferences(double sketchX, double sketchY)
{
    m_inferenceLines.clear();

    if (!m_sketch)
        return {sketchX, sketchY};

    double snappedX = sketchX;
    double snappedY = sketchY;
    const double tol = static_cast<double>(m_snapTolerance);

    // Collect all existing sketch points for inference checks
    const auto& points = m_sketch->points();

    // --- Horizontal alignment: cursor Y close to an existing point's Y ---
    double bestHDist = tol;
    float hSnapY = 0.0f;
    float hRefX = 0.0f;
    bool foundH = false;
    for (const auto& [pid, pt] : points) {
        double dy = std::abs(sketchY - pt.y);
        if (dy < bestHDist) {
            bestHDist = dy;
            hSnapY = static_cast<float>(pt.y);
            hRefX = static_cast<float>(pt.x);
            foundH = true;
        }
    }
    if (foundH) {
        snappedY = static_cast<double>(hSnapY);
        InferenceLine inf;
        inf.type = InferenceLine::Horizontal;
        inf.y1 = hSnapY;
        inf.y2 = hSnapY;
        // Draw from the reference point toward the cursor
        inf.x1 = std::min(hRefX, static_cast<float>(sketchX)) - 10.0f;
        inf.x2 = std::max(hRefX, static_cast<float>(sketchX)) + 10.0f;
        m_inferenceLines.push_back(inf);
    }

    // --- Vertical alignment: cursor X close to an existing point's X ---
    double bestVDist = tol;
    float vSnapX = 0.0f;
    float vRefY = 0.0f;
    bool foundV = false;
    for (const auto& [pid, pt] : points) {
        double dx = std::abs(sketchX - pt.x);
        if (dx < bestVDist) {
            bestVDist = dx;
            vSnapX = static_cast<float>(pt.x);
            vRefY = static_cast<float>(pt.y);
            foundV = true;
        }
    }
    if (foundV) {
        snappedX = static_cast<double>(vSnapX);
        InferenceLine inf;
        inf.type = InferenceLine::Vertical;
        inf.x1 = vSnapX;
        inf.x2 = vSnapX;
        inf.y1 = std::min(vRefY, static_cast<float>(sketchY)) - 10.0f;
        inf.y2 = std::max(vRefY, static_cast<float>(sketchY)) + 10.0f;
        m_inferenceLines.push_back(inf);
    }

    // --- Midpoint snap: cursor near the midpoint of any existing line ---
    for (const auto& [lid, ln] : m_sketch->lines()) {
        const auto& p1 = m_sketch->point(ln.startPointId);
        const auto& p2 = m_sketch->point(ln.endPointId);
        double mx = (p1.x + p2.x) / 2.0;
        double my = (p1.y + p2.y) / 2.0;
        double dx = sketchX - mx;
        double dy = sketchY - my;
        double dist = std::sqrt(dx * dx + dy * dy);
        if (dist < tol) {
            snappedX = mx;
            snappedY = my;
            InferenceLine inf;
            inf.type = InferenceLine::Midpoint;
            inf.x1 = static_cast<float>(mx);
            inf.y1 = static_cast<float>(my);
            inf.x2 = static_cast<float>(mx);
            inf.y2 = static_cast<float>(my);
            m_inferenceLines.push_back(inf);
            break;  // one midpoint snap is enough
        }
    }

    return {snappedX, snappedY};
}
