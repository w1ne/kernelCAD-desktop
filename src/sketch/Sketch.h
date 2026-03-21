#pragma once
#include "SketchEntity.h"
#include "SketchConstraint.h"
#include "SketchSolver.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <array>
#include <utility>
#include <cstddef>

namespace sketch {

/// Intersection between two sketch entities.
struct Intersection {
    double x = 0, y = 0;    // intersection point in sketch space
    double param1 = 0;       // parameter along entity 1 (0..1 for lines, angle for arcs/circles)
    double param2 = 0;       // parameter along entity 2
};

/// A 2D parametric sketch living on a plane in 3D space.
/// Contains points, lines, circles, arcs and constraints.
/// The solver resolves all constraints; detectProfiles finds closed loops.
class Sketch
{
public:
    Sketch();
    Sketch(const std::string& id, const std::string& planeId);
    ~Sketch();

    // ── Plane definition ─────────────────────────────────────────────────
    void setPlane(double ox, double oy, double oz,
                  double xDirX, double xDirY, double xDirZ,
                  double yDirX, double yDirY, double yDirZ);

    /// Transform a 2D sketch point to 3D world coordinates.
    void sketchToWorld(double sx, double sy,
                       double& wx, double& wy, double& wz) const;

    /// Transform a 3D world point to 2D sketch coordinates (projection onto plane).
    void worldToSketch(double wx, double wy, double wz,
                       double& sx, double& sy) const;

    /// Get the plane normal (cross product of xDir and yDir).
    void planeNormal(double& nx, double& ny, double& nz) const;

    /// Plane origin
    void planeOrigin(double& ox, double& oy, double& oz) const;

    /// Plane X direction
    void planeXDir(double& x, double& y, double& z) const;

    /// Plane Y direction
    void planeYDir(double& x, double& y, double& z) const;

    // ── Entity creation ──────────────────────────────────────────────────
    std::string addPoint(double x, double y, bool fixed = false);
    std::string addLine(const std::string& startPtId, const std::string& endPtId,
                        bool isConstruction = false);
    /// Convenience: creates two points and a line between them.
    std::string addLine(double x1, double y1, double x2, double y2);
    std::string addCircle(const std::string& centerPtId, double radius,
                          bool isConstruction = false);
    /// Convenience: creates center point and circle.
    std::string addCircle(double cx, double cy, double radius);
    std::string addArc(const std::string& centerPtId,
                       const std::string& startPtId, const std::string& endPtId,
                       double radius, bool isConstruction = false);
    /// Convenience: creates points and arc from center + angles.
    std::string addArc(double cx, double cy, double r,
                       double startAngle, double endAngle);

    std::string addSpline(const std::vector<std::string>& controlPointIds,
                          int degree = 3, bool isClosed = false);
    /// Convenience: create points and spline from coordinate pairs.
    std::string addSpline(const std::vector<std::pair<double,double>>& controlPoints,
                          int degree = 3);

    std::string addEllipse(const std::string& centerPtId, double majorRadius,
                           double minorRadius, double rotationAngle = 0.0,
                           bool isConstruction = false);
    /// Convenience: creates center point and ellipse.
    std::string addEllipse(double cx, double cy, double majorRadius,
                           double minorRadius, double rotationAngle = 0.0);

    // ── Constraint creation ──────────────────────────────────────────────
    std::string addConstraint(ConstraintType type,
                              const std::vector<std::string>& entityIds,
                              double value = 0.0);
    void removeConstraint(const std::string& id);

    // ── Auto-constraint inference ─────────────────────────────────────────
    /// Automatically infer and apply constraints based on geometric proximity.
    /// Called after entity creation.  Returns number of constraints added.
    int autoConstrain(double angleTolerance = 2.0,    // degrees
                      double distanceTolerance = 0.5); // mm

    // ── Solving ──────────────────────────────────────────────────────────
    SolveResult solve();
    bool isFullyConstrained() const;
    int freeDOF() const;

    // ── Trim / Extend / Offset ─────────────────────────────────────────
    /// Trim a line/circle/arc at intersection points.
    /// Removes the segment of entityId that contains the point (px, py).
    /// May split the entity into two new entities.
    /// Returns IDs of new entities created (0, 1, or 2).
    std::vector<std::string> trim(const std::string& entityId, double px, double py);

    /// Extend an entity's endpoint to the nearest intersection with another entity.
    /// endpointIndex: 0 = start, 1 = end
    /// Returns true if extension was found.
    bool extend(const std::string& entityId, int endpointIndex);

    /// Offset a set of curves by a distance.
    /// Positive = outward, negative = inward (for circles/arcs, increases/decreases radius).
    /// Returns IDs of new offset entities.
    std::vector<std::string> offset(const std::vector<std::string>& entityIds, double distance);

    /// Project a 3D edge onto the sketch plane, creating sketch geometry.
    /// Takes world-space polyline points, projects them to 2D sketch coords.
    /// Returns the ID of the new spline (or line for 2-point edges).
    std::string projectEdge(const std::vector<std::pair<double,double>>& projectedPoints);

    /// Offset a single sketch curve by a distance, creating a new parallel curve.
    /// For lines: parallel line offset perpendicular by distance.
    /// For circles: concentric circle with radius +/- distance.
    /// For arcs: concentric arc with radius +/- distance, same angles.
    std::string offsetCurve(const std::string& curveId, double distance);

    /// Trim a curve at its intersection points with other curves.
    /// Removes the segment nearest to the pick point (px, py).
    /// Returns true if the curve was successfully trimmed.
    bool trimCurve(const std::string& curveId, double pickX, double pickY);

    /// Create a fillet arc at the intersection of two lines.
    /// Finds the shared endpoint, creates a tangent arc, and trims the lines.
    std::string sketchFillet(const std::string& line1Id, const std::string& line2Id, double radius);

    /// Create a chamfer line at the intersection of two lines.
    /// Finds the shared endpoint, creates a connecting line at `distance` from the vertex,
    /// and trims the lines.
    std::string sketchChamfer(const std::string& line1Id, const std::string& line2Id, double distance);

    /// Remove a curve entity and its associated points (if not shared).
    void removeEntity(const std::string& entityId);

    // ── Intersection queries ──────────────────────────────────────────────
    /// Find intersections between two entities.
    std::vector<Intersection> findIntersections(const std::string& id1, const std::string& id2) const;

    /// Find all intersections of an entity with every other entity.
    std::vector<Intersection> findAllIntersections(const std::string& entityId) const;

    // ── Profile detection ────────────────────────────────────────────────
    /// Returns a list of profiles. Each profile is a list of curve entity IDs
    /// (lines, circles, arcs) that form a closed loop.
    std::vector<std::vector<std::string>> detectProfiles() const;

    // ── Entity access ────────────────────────────────────────────────────
    const std::string& getId() const { return m_id; }

    const SketchPoint&  point(const std::string& id) const;
    const SketchLine&   line(const std::string& id) const;
    const SketchCircle& circle(const std::string& id) const;
    const SketchArc&    arc(const std::string& id) const;
    const SketchSpline& spline(const std::string& id) const;
    const SketchEllipse& ellipse(const std::string& id) const;

    SketchPoint&  point(const std::string& id);
    SketchLine&   line(const std::string& id);
    SketchCircle& circle(const std::string& id);
    SketchArc&    arc(const std::string& id);
    SketchSpline& spline(const std::string& id);
    SketchEllipse& ellipse(const std::string& id);

    const std::unordered_map<std::string, SketchPoint>&      points()      const { return m_points; }
    const std::unordered_map<std::string, SketchLine>&       lines()       const { return m_lines; }
    const std::unordered_map<std::string, SketchCircle>&     circles()     const { return m_circles; }
    const std::unordered_map<std::string, SketchArc>&        arcs()        const { return m_arcs; }
    const std::unordered_map<std::string, SketchSpline>&     splines()     const { return m_splines; }
    const std::unordered_map<std::string, SketchEllipse>&    ellipses()    const { return m_ellipses; }
    const std::unordered_map<std::string, SketchConstraint>& constraints() const { return m_constraints; }

private:
    std::string m_id;
    std::string m_planeId;

    // Plane definition: origin + two basis vectors
    double m_originX = 0, m_originY = 0, m_originZ = 0;
    double m_xDirX = 1, m_xDirY = 0, m_xDirZ = 0;
    double m_yDirX = 0, m_yDirY = 1, m_yDirZ = 0;

    // Entity storage
    std::unordered_map<std::string, SketchPoint>      m_points;
    std::unordered_map<std::string, SketchLine>        m_lines;
    std::unordered_map<std::string, SketchCircle>      m_circles;
    std::unordered_map<std::string, SketchArc>         m_arcs;
    std::unordered_map<std::string, SketchSpline>      m_splines;
    std::unordered_map<std::string, SketchEllipse>     m_ellipses;
    std::unordered_map<std::string, SketchConstraint>  m_constraints;

    // ID counters
    int m_nextPointId  = 1;
    int m_nextLineId   = 1;
    int m_nextCircleId = 1;
    int m_nextArcId    = 1;
    int m_nextSplineId = 1;
    int m_nextEllipseId = 1;
    int m_nextConstraintId = 1;

    // ── Intersection math helpers ─────────────────────────────────────────
    /// Line(p1->p2) vs Line(p3->p4).
    static std::vector<Intersection> intersectLineLine(
        double x1, double y1, double x2, double y2,
        double x3, double y3, double x4, double y4);

    /// Line(p1->p2) vs circle centered at (cx,cy) with radius r.
    static std::vector<Intersection> intersectLineCircle(
        double x1, double y1, double x2, double y2,
        double cx, double cy, double r);

    /// Circle(c1,r1) vs Circle(c2,r2).
    static std::vector<Intersection> intersectCircleCircle(
        double cx1, double cy1, double r1,
        double cx2, double cy2, double r2);

    /// Filter intersection list to only those within arc angular range.
    static std::vector<Intersection> filterForArc(
        const std::vector<Intersection>& hits,
        double cx, double cy, double startAngle, double endAngle,
        bool isParam1);

    /// Compute the CCW angle from startAngle to endAngle (always positive, 0..2pi).
    static double arcSweep(double startAngle, double endAngle);

    /// Normalise angle to [0, 2pi).
    static double normaliseAngle(double a);

    /// Check if angle is within CCW arc from startAngle to endAngle.
    static bool angleInArc(double angle, double startAngle, double endAngle);

    /// Get arc start/end angles from the point entities.
    void arcAngles(const SketchArc& a, double& startAngle, double& endAngle) const;

    /// Check if a point is referenced by any entity other than the given one.
    bool isPointShared(const std::string& pointId, const std::string& excludeEntityId) const;
};

} // namespace sketch
