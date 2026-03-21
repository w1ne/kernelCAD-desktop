#include "Sketch.h"
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <set>

namespace sketch {

Sketch::Sketch() = default;
Sketch::Sketch(const std::string& id, const std::string& planeId)
    : m_id(id), m_planeId(planeId) {}
Sketch::~Sketch() = default;

// ── Plane definition ─────────────────────────────────────────────────────

void Sketch::setPlane(double ox, double oy, double oz,
                      double xDirX, double xDirY, double xDirZ,
                      double yDirX, double yDirY, double yDirZ)
{
    m_originX = ox; m_originY = oy; m_originZ = oz;
    m_xDirX = xDirX; m_xDirY = xDirY; m_xDirZ = xDirZ;
    m_yDirX = yDirX; m_yDirY = yDirY; m_yDirZ = yDirZ;
}

void Sketch::sketchToWorld(double sx, double sy,
                           double& wx, double& wy, double& wz) const
{
    wx = m_originX + sx * m_xDirX + sy * m_yDirX;
    wy = m_originY + sx * m_xDirY + sy * m_yDirY;
    wz = m_originZ + sx * m_xDirZ + sy * m_yDirZ;
}

void Sketch::worldToSketch(double wx, double wy, double wz,
                           double& sx, double& sy) const
{
    // Project (w - origin) onto xDir and yDir via dot products.
    double dx = wx - m_originX;
    double dy = wy - m_originY;
    double dz = wz - m_originZ;
    sx = dx * m_xDirX + dy * m_xDirY + dz * m_xDirZ;
    sy = dx * m_yDirX + dy * m_yDirY + dz * m_yDirZ;
}

void Sketch::planeNormal(double& nx, double& ny, double& nz) const
{
    // Cross product of xDir x yDir
    nx = m_xDirY * m_yDirZ - m_xDirZ * m_yDirY;
    ny = m_xDirZ * m_yDirX - m_xDirX * m_yDirZ;
    nz = m_xDirX * m_yDirY - m_xDirY * m_yDirX;
    double len = std::sqrt(nx*nx + ny*ny + nz*nz);
    if (len > 1e-12) { nx /= len; ny /= len; nz /= len; }
}

void Sketch::planeOrigin(double& ox, double& oy, double& oz) const
{
    ox = m_originX; oy = m_originY; oz = m_originZ;
}

void Sketch::planeXDir(double& x, double& y, double& z) const
{
    x = m_xDirX; y = m_xDirY; z = m_xDirZ;
}

void Sketch::planeYDir(double& x, double& y, double& z) const
{
    x = m_yDirX; y = m_yDirY; z = m_yDirZ;
}

// ── Entity creation ──────────────────────────────────────────────────────

std::string Sketch::addPoint(double x, double y, bool fixed)
{
    std::ostringstream oss;
    oss << "pt_" << m_nextPointId++;
    std::string id = oss.str();

    SketchPoint pt;
    pt.id = id;
    pt.x = x;
    pt.y = y;
    pt.isFixed = fixed;
    m_points[id] = pt;
    return id;
}

std::string Sketch::addLine(const std::string& startPtId, const std::string& endPtId,
                            bool isConstruction)
{
    if (m_points.find(startPtId) == m_points.end())
        throw std::runtime_error("Sketch::addLine: start point '" + startPtId + "' not found");
    if (m_points.find(endPtId) == m_points.end())
        throw std::runtime_error("Sketch::addLine: end point '" + endPtId + "' not found");

    std::ostringstream oss;
    oss << "ln_" << m_nextLineId++;
    std::string id = oss.str();

    SketchLine ln;
    ln.id = id;
    ln.startPointId = startPtId;
    ln.endPointId = endPtId;
    ln.isConstruction = isConstruction;
    m_lines[id] = ln;
    return id;
}

std::string Sketch::addLine(double x1, double y1, double x2, double y2)
{
    auto p1 = addPoint(x1, y1);
    auto p2 = addPoint(x2, y2);
    return addLine(p1, p2);
}

std::string Sketch::addCircle(const std::string& centerPtId, double radius,
                              bool isConstruction)
{
    if (m_points.find(centerPtId) == m_points.end())
        throw std::runtime_error("Sketch::addCircle: center point '" + centerPtId + "' not found");

    std::ostringstream oss;
    oss << "cir_" << m_nextCircleId++;
    std::string id = oss.str();

    SketchCircle c;
    c.id = id;
    c.centerPointId = centerPtId;
    c.radius = radius;
    c.isConstruction = isConstruction;
    m_circles[id] = c;
    return id;
}

std::string Sketch::addCircle(double cx, double cy, double radius)
{
    auto cpt = addPoint(cx, cy);
    return addCircle(cpt, radius);
}

std::string Sketch::addArc(const std::string& centerPtId,
                           const std::string& startPtId, const std::string& endPtId,
                           double radius, bool isConstruction)
{
    if (m_points.find(centerPtId) == m_points.end())
        throw std::runtime_error("Sketch::addArc: center point not found");
    if (m_points.find(startPtId) == m_points.end())
        throw std::runtime_error("Sketch::addArc: start point not found");
    if (m_points.find(endPtId) == m_points.end())
        throw std::runtime_error("Sketch::addArc: end point not found");

    std::ostringstream oss;
    oss << "arc_" << m_nextArcId++;
    std::string id = oss.str();

    SketchArc a;
    a.id = id;
    a.centerPointId = centerPtId;
    a.startPointId = startPtId;
    a.endPointId = endPtId;
    a.radius = radius;
    a.isConstruction = isConstruction;
    m_arcs[id] = a;
    return id;
}

std::string Sketch::addArc(double cx, double cy, double r,
                           double startAngle, double endAngle)
{
    auto cpt = addPoint(cx, cy);
    auto spt = addPoint(cx + r * std::cos(startAngle), cy + r * std::sin(startAngle));
    auto ept = addPoint(cx + r * std::cos(endAngle),   cy + r * std::sin(endAngle));
    return addArc(cpt, spt, ept, r);
}

// ── Spline creation ─────────────────────────────────────────────────────

std::string Sketch::addSpline(const std::vector<std::string>& controlPointIds,
                              int degree, bool isClosed)
{
    if (controlPointIds.size() < 2)
        throw std::runtime_error("Sketch::addSpline: need at least 2 control points");
    for (const auto& pid : controlPointIds) {
        if (m_points.find(pid) == m_points.end())
            throw std::runtime_error("Sketch::addSpline: control point '" + pid + "' not found");
    }
    // Clamp degree to be valid for the number of control points
    int maxDegree = static_cast<int>(controlPointIds.size()) - 1;
    if (degree > maxDegree)
        degree = maxDegree;
    if (degree < 1)
        degree = 1;

    std::ostringstream oss;
    oss << "spl_" << m_nextSplineId++;
    std::string id = oss.str();

    SketchSpline s;
    s.id = id;
    s.controlPointIds = controlPointIds;
    s.degree = degree;
    s.isClosed = isClosed;
    m_splines[id] = s;
    return id;
}

std::string Sketch::addSpline(const std::vector<std::pair<double,double>>& controlPoints,
                              int degree)
{
    std::vector<std::string> ptIds;
    ptIds.reserve(controlPoints.size());
    for (const auto& [px, py] : controlPoints) {
        ptIds.push_back(addPoint(px, py));
    }
    return addSpline(ptIds, degree);
}

// ── Ellipse creation ─────────────────────────────────────────────────────

std::string Sketch::addEllipse(const std::string& centerPtId, double majorRadius,
                               double minorRadius, double rotationAngle,
                               bool isConstruction)
{
    if (m_points.find(centerPtId) == m_points.end())
        throw std::runtime_error("Sketch::addEllipse: center point '" + centerPtId + "' not found");

    std::ostringstream oss;
    oss << "ell_" << m_nextEllipseId++;
    std::string id = oss.str();

    SketchEllipse e;
    e.id = id;
    e.centerPointId = centerPtId;
    e.majorRadius = majorRadius;
    e.minorRadius = minorRadius;
    e.rotationAngle = rotationAngle;
    e.isConstruction = isConstruction;
    m_ellipses[id] = e;
    return id;
}

std::string Sketch::addEllipse(double cx, double cy, double majorRadius,
                               double minorRadius, double rotationAngle)
{
    auto cpt = addPoint(cx, cy);
    return addEllipse(cpt, majorRadius, minorRadius, rotationAngle);
}

// ── Constraint creation ──────────────────────────────────────────────────

std::string Sketch::addConstraint(ConstraintType type,
                                  const std::vector<std::string>& entityIds,
                                  double value)
{
    std::ostringstream oss;
    oss << "con_" << m_nextConstraintId++;
    std::string id = oss.str();

    SketchConstraint c;
    c.id = id;
    c.type = type;
    c.entityIds = entityIds;
    c.value = value;
    c.dofRemoved = SketchConstraint::defaultDofRemoved(type);

    // For Fix constraints, capture the point's current position
    if (type == ConstraintType::Fix && !entityIds.empty()) {
        auto pit = m_points.find(entityIds[0]);
        if (pit != m_points.end()) {
            c.value  = pit->second.x;  // x_fixed
            c.value2 = pit->second.y;  // y_fixed
        }
    }

    m_constraints[id] = c;
    return id;
}

void Sketch::removeConstraint(const std::string& id)
{
    m_constraints.erase(id);
}

// ── Auto-constraint inference ────────────────────────────────────────────

int Sketch::autoConstrain(double angleTolerance, double distanceTolerance)
{
    int added = 0;
    const double angTolRad = angleTolerance * M_PI / 180.0;

    // Collect all existing constraint entity-id sets to avoid duplicates.
    auto hasConstraint = [&](ConstraintType type,
                             const std::vector<std::string>& ids) -> bool {
        for (const auto& [cid, c] : m_constraints) {
            if (c.type != type) continue;
            if (c.entityIds == ids) return true;
            // Check reversed pair for symmetric constraints
            if (ids.size() == 2 && c.entityIds.size() == 2 &&
                c.entityIds[0] == ids[1] && c.entityIds[1] == ids[0])
                return true;
        }
        return false;
    };

    // ── Horizontal / Vertical on lines ───────────────────────────────────
    for (const auto& [lid, ln] : m_lines) {
        if (ln.isConstruction) continue;
        const auto& p1 = m_points.at(ln.startPointId);
        const auto& p2 = m_points.at(ln.endPointId);
        double dx = p2.x - p1.x;
        double dy = p2.y - p1.y;
        double len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-9) continue;

        double angle = std::atan2(std::abs(dy), std::abs(dx));

        // Horizontal: angle near 0
        if (angle < angTolRad) {
            if (!hasConstraint(ConstraintType::Horizontal, {lid})) {
                addConstraint(ConstraintType::Horizontal, {lid});
                ++added;
            }
        }
        // Vertical: angle near 90 deg
        else if (std::abs(angle - M_PI / 2.0) < angTolRad) {
            if (!hasConstraint(ConstraintType::Vertical, {lid})) {
                addConstraint(ConstraintType::Vertical, {lid});
                ++added;
            }
        }
    }

    // ── Coincident endpoints ─────────────────────────────────────────────
    {
        // Gather all "endpoint" point IDs (used by lines, arcs, splines)
        std::vector<std::string> endpointIds;
        for (const auto& [lid, ln] : m_lines) {
            endpointIds.push_back(ln.startPointId);
            endpointIds.push_back(ln.endPointId);
        }
        for (const auto& [aid, a] : m_arcs) {
            endpointIds.push_back(a.startPointId);
            endpointIds.push_back(a.endPointId);
        }
        for (const auto& [sid, sp] : m_splines) {
            if (!sp.controlPointIds.empty()) {
                endpointIds.push_back(sp.controlPointIds.front());
                if (sp.controlPointIds.size() > 1)
                    endpointIds.push_back(sp.controlPointIds.back());
            }
        }

        // Remove duplicates
        std::sort(endpointIds.begin(), endpointIds.end());
        endpointIds.erase(std::unique(endpointIds.begin(), endpointIds.end()),
                          endpointIds.end());

        for (size_t i = 0; i < endpointIds.size(); ++i) {
            for (size_t j = i + 1; j < endpointIds.size(); ++j) {
                const auto& pi = m_points.at(endpointIds[i]);
                const auto& pj = m_points.at(endpointIds[j]);
                double dx = pi.x - pj.x;
                double dy = pi.y - pj.y;
                double dist = std::sqrt(dx * dx + dy * dy);
                if (dist <= distanceTolerance) {
                    if (!hasConstraint(ConstraintType::Coincident,
                                       {endpointIds[i], endpointIds[j]})) {
                        addConstraint(ConstraintType::Coincident,
                                      {endpointIds[i], endpointIds[j]});
                        ++added;
                    }
                }
            }
        }
    }

    // ── Perpendicular between lines ──────────────────────────────────────
    {
        std::vector<std::string> lineIds;
        for (const auto& [lid, ln] : m_lines) {
            if (!ln.isConstruction)
                lineIds.push_back(lid);
        }
        for (size_t i = 0; i < lineIds.size(); ++i) {
            for (size_t j = i + 1; j < lineIds.size(); ++j) {
                const auto& ln1 = m_lines.at(lineIds[i]);
                const auto& ln2 = m_lines.at(lineIds[j]);
                const auto& a1 = m_points.at(ln1.startPointId);
                const auto& a2 = m_points.at(ln1.endPointId);
                const auto& b1 = m_points.at(ln2.startPointId);
                const auto& b2 = m_points.at(ln2.endPointId);
                double d1x = a2.x - a1.x, d1y = a2.y - a1.y;
                double d2x = b2.x - b1.x, d2y = b2.y - b1.y;
                double len1 = std::sqrt(d1x * d1x + d1y * d1y);
                double len2 = std::sqrt(d2x * d2x + d2y * d2y);
                if (len1 < 1e-9 || len2 < 1e-9) continue;

                double dot = (d1x * d2x + d1y * d2y) / (len1 * len2);
                // Check perpendicular (dot product near 0)
                if (std::abs(dot) < std::sin(angTolRad)) {
                    if (!hasConstraint(ConstraintType::Perpendicular,
                                       {lineIds[i], lineIds[j]})) {
                        addConstraint(ConstraintType::Perpendicular,
                                      {lineIds[i], lineIds[j]});
                        ++added;
                    }
                }
            }
        }
    }

    // ── Equal length lines ───────────────────────────────────────────────
    {
        std::vector<std::string> lineIds;
        for (const auto& [lid, ln] : m_lines) {
            if (!ln.isConstruction)
                lineIds.push_back(lid);
        }
        for (size_t i = 0; i < lineIds.size(); ++i) {
            for (size_t j = i + 1; j < lineIds.size(); ++j) {
                const auto& ln1 = m_lines.at(lineIds[i]);
                const auto& ln2 = m_lines.at(lineIds[j]);
                const auto& a1 = m_points.at(ln1.startPointId);
                const auto& a2 = m_points.at(ln1.endPointId);
                const auto& b1 = m_points.at(ln2.startPointId);
                const auto& b2 = m_points.at(ln2.endPointId);
                double len1 = std::sqrt((a2.x-a1.x)*(a2.x-a1.x) + (a2.y-a1.y)*(a2.y-a1.y));
                double len2 = std::sqrt((b2.x-b1.x)*(b2.x-b1.x) + (b2.y-b1.y)*(b2.y-b1.y));
                if (len1 < 1e-9 || len2 < 1e-9) continue;
                if (std::abs(len1 - len2) <= distanceTolerance) {
                    if (!hasConstraint(ConstraintType::Equal,
                                       {lineIds[i], lineIds[j]})) {
                        addConstraint(ConstraintType::Equal,
                                      {lineIds[i], lineIds[j]});
                        ++added;
                    }
                }
            }
        }
    }

    // ── Tangent: line meets arc/circle with aligned tangent direction ─────
    for (const auto& [lid, ln] : m_lines) {
        if (ln.isConstruction) continue;
        const auto& lp1 = m_points.at(ln.startPointId);
        const auto& lp2 = m_points.at(ln.endPointId);
        double ldx = lp2.x - lp1.x, ldy = lp2.y - lp1.y;
        double llen = std::sqrt(ldx * ldx + ldy * ldy);
        if (llen < 1e-9) continue;

        // Check arcs
        for (const auto& [aid, arc] : m_arcs) {
            if (arc.isConstruction) continue;
            const auto& cp = m_points.at(arc.centerPointId);
            // Check if line start or end is near arc start or end
            for (const std::string& lpId : {ln.startPointId, ln.endPointId}) {
                for (const std::string& apId : {arc.startPointId, arc.endPointId}) {
                    const auto& lp = m_points.at(lpId);
                    const auto& ap = m_points.at(apId);
                    double dd = std::sqrt((lp.x-ap.x)*(lp.x-ap.x) + (lp.y-ap.y)*(lp.y-ap.y));
                    if (dd > distanceTolerance) continue;
                    // Check tangent direction: line direction vs radius-perpendicular
                    double rx = ap.x - cp.x, ry = ap.y - cp.y;
                    double rlen = std::sqrt(rx * rx + ry * ry);
                    if (rlen < 1e-9) continue;
                    // Tangent direction is perpendicular to radius: (-ry, rx)/rlen
                    double dot = std::abs(ldx * rx + ldy * ry) / (llen * rlen);
                    if (dot < std::sin(angTolRad * 2.0)) {  // slightly relaxed
                        if (!hasConstraint(ConstraintType::Tangent, {lid, aid})) {
                            addConstraint(ConstraintType::Tangent, {lid, aid});
                            ++added;
                        }
                    }
                }
            }
        }
    }

    // ── Concentric circles/arcs ──────────────────────────────────────────
    {
        struct CircInfo { std::string id; std::string centerId; };
        std::vector<CircInfo> circInfos;
        for (const auto& [cid, c] : m_circles)
            circInfos.push_back({cid, c.centerPointId});
        for (const auto& [aid, a] : m_arcs)
            circInfos.push_back({aid, a.centerPointId});

        for (size_t i = 0; i < circInfos.size(); ++i) {
            for (size_t j = i + 1; j < circInfos.size(); ++j) {
                const auto& ci = m_points.at(circInfos[i].centerId);
                const auto& cj = m_points.at(circInfos[j].centerId);
                double dx = ci.x - cj.x;
                double dy = ci.y - cj.y;
                double dist = std::sqrt(dx * dx + dy * dy);
                if (dist <= distanceTolerance) {
                    if (!hasConstraint(ConstraintType::Concentric,
                                       {circInfos[i].id, circInfos[j].id})) {
                        addConstraint(ConstraintType::Concentric,
                                      {circInfos[i].id, circInfos[j].id});
                        ++added;
                    }
                }
            }
        }
    }

    return added;
}

// ── Solving ──────────────────────────────────────────────────────────────

SolveResult Sketch::solve()
{
    if (m_points.empty()) {
        return SolveResult{SolveStatus::Solved, 0, 0.0, {}};
    }

    SketchSolver solver;
    solver.setEntities(m_points, m_lines, m_circles, m_arcs, &m_ellipses);
    solver.setConstraints(m_constraints);
    return solver.solve();
}

bool Sketch::isFullyConstrained() const
{
    return freeDOF() == 0;
}

int Sketch::freeDOF() const
{
    // Count mutable DOF
    int totalParams = 0;
    for (const auto& [id, pt] : m_points) {
        if (!pt.isFixed)
            totalParams += SketchPoint::DOF;
    }
    for (const auto& [id, circ] : m_circles)
        totalParams += SketchCircle::DOF;
    for (const auto& [id, arc] : m_arcs)
        totalParams += SketchArc::DOF;
    for (const auto& [id, ell] : m_ellipses)
        totalParams += SketchEllipse::DOF;

    // Count constrained DOF
    int constrained = 0;
    for (const auto& [id, c] : m_constraints)
        constrained += c.dofRemoved;

    return totalParams - constrained;
}

// ── Profile detection ────────────────────────────────────────────────────

std::vector<std::vector<std::string>> Sketch::detectProfiles() const
{
    // Find closed loops of curves by building a graph of connected endpoints.
    // Each non-construction line contributes edges from start to end point.
    // Each non-construction arc contributes edges from start to end point.
    // Each non-construction circle is a self-contained closed profile.

    std::vector<std::vector<std::string>> profiles;

    // Each standalone circle is a profile
    for (const auto& [cid, circ] : m_circles) {
        if (!circ.isConstruction)
            profiles.push_back({cid});
    }

    // Each standalone ellipse is a profile
    for (const auto& [eid, ell] : m_ellipses) {
        if (!ell.isConstruction)
            profiles.push_back({eid});
    }

    // Build adjacency: pointId -> list of (curveId, otherPointId)
    struct Edge {
        std::string curveId;
        std::string otherPointId;
    };
    std::unordered_map<std::string, std::vector<Edge>> adj;
    std::vector<std::string> curveIds;

    for (const auto& [lid, ln] : m_lines) {
        if (ln.isConstruction) continue;
        curveIds.push_back(lid);
        adj[ln.startPointId].push_back({lid, ln.endPointId});
        adj[ln.endPointId].push_back({lid, ln.startPointId});
    }
    for (const auto& [aid, a] : m_arcs) {
        if (a.isConstruction) continue;
        curveIds.push_back(aid);
        adj[a.startPointId].push_back({aid, a.endPointId});
        adj[a.endPointId].push_back({aid, a.startPointId});
    }
    for (const auto& [sid, sp] : m_splines) {
        if (sp.isConstruction) continue;
        if (sp.controlPointIds.size() < 2) continue;
        curveIds.push_back(sid);
        const std::string& firstPt = sp.controlPointIds.front();
        const std::string& lastPt  = sp.controlPointIds.back();
        if (sp.isClosed) {
            // Closed spline: loop back to the start
            adj[firstPt].push_back({sid, firstPt});
        } else {
            adj[firstPt].push_back({sid, lastPt});
            adj[lastPt].push_back({sid, firstPt});
        }
    }

    if (curveIds.empty())
        return profiles;

    // Find cycles via greedy chain-walk.  For each unused curve, walk a chain
    // back to the starting point.  Works well for manifold graphs (degree 2).
    std::set<std::string> globalUsed;

    for (const auto& seedCurve : curveIds) {
        if (globalUsed.count(seedCurve)) continue;

        // Determine start point from this curve
        std::string startPt, firstOtherPt;
        {
            auto lit = m_lines.find(seedCurve);
            if (lit != m_lines.end()) {
                startPt = lit->second.startPointId;
                firstOtherPt = lit->second.endPointId;
            } else {
                auto ait = m_arcs.find(seedCurve);
                if (ait != m_arcs.end()) {
                    startPt = ait->second.startPointId;
                    firstOtherPt = ait->second.endPointId;
                } else {
                    auto sit = m_splines.find(seedCurve);
                    if (sit != m_splines.end() && sit->second.controlPointIds.size() >= 2) {
                        startPt = sit->second.controlPointIds.front();
                        firstOtherPt = sit->second.controlPointIds.back();
                    } else {
                        continue;
                    }
                }
            }
        }

        // Walk chain
        std::vector<std::string> loop;
        std::set<std::string> usedInLoop;
        loop.push_back(seedCurve);
        usedInLoop.insert(seedCurve);
        std::string currentPt = firstOtherPt;

        int maxSteps = static_cast<int>(curveIds.size());
        bool closed = false;
        for (int step = 0; step < maxSteps; ++step) {
            if (currentPt == startPt) {
                closed = true;
                break;
            }
            auto it = adj.find(currentPt);
            if (it == adj.end()) break;

            bool found = false;
            for (const auto& edge : it->second) {
                if (usedInLoop.count(edge.curveId) == 0 &&
                    globalUsed.count(edge.curveId) == 0) {
                    loop.push_back(edge.curveId);
                    usedInLoop.insert(edge.curveId);
                    currentPt = edge.otherPointId;
                    found = true;
                    break;
                }
            }
            if (!found) break;
        }

        if (closed && !loop.empty()) {
            for (const auto& cid : loop)
                globalUsed.insert(cid);
            profiles.push_back(loop);
        }
    }

    return profiles;
}

// ── Entity access ────────────────────────────────────────────────────────

const SketchPoint& Sketch::point(const std::string& id) const
{
    auto it = m_points.find(id);
    if (it == m_points.end())
        throw std::runtime_error("Sketch::point: '" + id + "' not found");
    return it->second;
}

const SketchLine& Sketch::line(const std::string& id) const
{
    auto it = m_lines.find(id);
    if (it == m_lines.end())
        throw std::runtime_error("Sketch::line: '" + id + "' not found");
    return it->second;
}

const SketchCircle& Sketch::circle(const std::string& id) const
{
    auto it = m_circles.find(id);
    if (it == m_circles.end())
        throw std::runtime_error("Sketch::circle: '" + id + "' not found");
    return it->second;
}

const SketchArc& Sketch::arc(const std::string& id) const
{
    auto it = m_arcs.find(id);
    if (it == m_arcs.end())
        throw std::runtime_error("Sketch::arc: '" + id + "' not found");
    return it->second;
}

SketchPoint& Sketch::point(const std::string& id)
{
    auto it = m_points.find(id);
    if (it == m_points.end())
        throw std::runtime_error("Sketch::point: '" + id + "' not found");
    return it->second;
}

SketchLine& Sketch::line(const std::string& id)
{
    auto it = m_lines.find(id);
    if (it == m_lines.end())
        throw std::runtime_error("Sketch::line: '" + id + "' not found");
    return it->second;
}

SketchCircle& Sketch::circle(const std::string& id)
{
    auto it = m_circles.find(id);
    if (it == m_circles.end())
        throw std::runtime_error("Sketch::circle: '" + id + "' not found");
    return it->second;
}

SketchArc& Sketch::arc(const std::string& id)
{
    auto it = m_arcs.find(id);
    if (it == m_arcs.end())
        throw std::runtime_error("Sketch::arc: '" + id + "' not found");
    return it->second;
}

const SketchSpline& Sketch::spline(const std::string& id) const
{
    auto it = m_splines.find(id);
    if (it == m_splines.end())
        throw std::runtime_error("Sketch::spline: '" + id + "' not found");
    return it->second;
}

SketchSpline& Sketch::spline(const std::string& id)
{
    auto it = m_splines.find(id);
    if (it == m_splines.end())
        throw std::runtime_error("Sketch::spline: '" + id + "' not found");
    return it->second;
}

const SketchEllipse& Sketch::ellipse(const std::string& id) const
{
    auto it = m_ellipses.find(id);
    if (it == m_ellipses.end())
        throw std::runtime_error("Sketch::ellipse: '" + id + "' not found");
    return it->second;
}

SketchEllipse& Sketch::ellipse(const std::string& id)
{
    auto it = m_ellipses.find(id);
    if (it == m_ellipses.end())
        throw std::runtime_error("Sketch::ellipse: '" + id + "' not found");
    return it->second;
}

// ══════════════════════════════════════════════════════════════════════════════
// Intersection math helpers
// ══════════════════════════════════════════════════════════════════════════════

static constexpr double kEps = 1e-9;

double Sketch::normaliseAngle(double a)
{
    a = std::fmod(a, 2.0 * M_PI);
    if (a < 0) a += 2.0 * M_PI;
    return a;
}

double Sketch::arcSweep(double startAngle, double endAngle)
{
    double sweep = normaliseAngle(endAngle - startAngle);
    if (sweep < kEps) sweep = 2.0 * M_PI; // full circle
    return sweep;
}

bool Sketch::angleInArc(double angle, double startAngle, double endAngle)
{
    double a  = normaliseAngle(angle - startAngle);
    double sw = arcSweep(startAngle, endAngle);
    return a <= sw + kEps;
}

void Sketch::arcAngles(const SketchArc& a, double& startAngle, double& endAngle) const
{
    const auto& cp = m_points.at(a.centerPointId);
    const auto& sp = m_points.at(a.startPointId);
    const auto& ep = m_points.at(a.endPointId);
    startAngle = std::atan2(sp.y - cp.y, sp.x - cp.x);
    endAngle   = std::atan2(ep.y - cp.y, ep.x - cp.x);
}

std::vector<Intersection> Sketch::intersectLineLine(
    double x1, double y1, double x2, double y2,
    double x3, double y3, double x4, double y4)
{
    double dx1 = x2 - x1, dy1 = y2 - y1;
    double dx2 = x4 - x3, dy2 = y4 - y3;
    double denom = dx1 * dy2 - dy1 * dx2;
    if (std::abs(denom) < kEps)
        return {};  // parallel or coincident

    double t = ((x3 - x1) * dy2 - (y3 - y1) * dx2) / denom;
    double u = ((x3 - x1) * dy1 - (y3 - y1) * dx1) / denom;

    // Only accept if intersection is within both segments
    if (t < -kEps || t > 1.0 + kEps || u < -kEps || u > 1.0 + kEps)
        return {};

    Intersection ix;
    ix.x = x1 + t * dx1;
    ix.y = y1 + t * dy1;
    ix.param1 = t;
    ix.param2 = u;
    return {ix};
}

std::vector<Intersection> Sketch::intersectLineCircle(
    double x1, double y1, double x2, double y2,
    double cx, double cy, double r)
{
    // Direction vector of line
    double dx = x2 - x1, dy = y2 - y1;
    double len2 = dx * dx + dy * dy;
    if (len2 < kEps) return {};

    // Offset from circle center to line start
    double fx = x1 - cx, fy = y1 - cy;

    double a = len2;
    double b = 2.0 * (fx * dx + fy * dy);
    double c = fx * fx + fy * fy - r * r;
    double disc = b * b - 4.0 * a * c;
    if (disc < 0) return {};

    std::vector<Intersection> result;
    double sqrtDisc = std::sqrt(disc);
    for (double sign : {-1.0, 1.0}) {
        double t = (-b + sign * sqrtDisc) / (2.0 * a);
        // Accept if t is within line segment [0,1]
        if (t < -kEps || t > 1.0 + kEps) continue;

        Intersection ix;
        ix.x = x1 + t * dx;
        ix.y = y1 + t * dy;
        ix.param1 = t;
        // param2 = angle on circle
        ix.param2 = std::atan2(ix.y - cy, ix.x - cx);
        result.push_back(ix);
    }
    // Deduplicate if discriminant was ~0
    if (result.size() == 2) {
        double dd = (result[0].x - result[1].x) * (result[0].x - result[1].x) +
                    (result[0].y - result[1].y) * (result[0].y - result[1].y);
        if (dd < kEps * kEps) result.pop_back();
    }
    return result;
}

std::vector<Intersection> Sketch::intersectCircleCircle(
    double cx1, double cy1, double r1,
    double cx2, double cy2, double r2)
{
    double dx = cx2 - cx1, dy = cy2 - cy1;
    double d = std::sqrt(dx * dx + dy * dy);
    if (d < kEps) return {};  // concentric
    if (d > r1 + r2 + kEps) return {};  // too far
    if (d < std::abs(r1 - r2) - kEps) return {};  // one inside the other

    double a = (r1 * r1 - r2 * r2 + d * d) / (2.0 * d);
    double h2 = r1 * r1 - a * a;
    if (h2 < 0) h2 = 0;
    double h = std::sqrt(h2);

    double mx = cx1 + a * dx / d;
    double my = cy1 + a * dy / d;

    std::vector<Intersection> result;
    if (h < kEps) {
        // Tangent
        Intersection ix;
        ix.x = mx; ix.y = my;
        ix.param1 = std::atan2(my - cy1, mx - cx1);
        ix.param2 = std::atan2(my - cy2, mx - cx2);
        result.push_back(ix);
    } else {
        double ox = h * dy / d;
        double oy = h * dx / d;  // note: perpendicular direction
        for (double sign : {-1.0, 1.0}) {
            Intersection ix;
            ix.x = mx + sign * ox;
            ix.y = my - sign * oy;
            ix.param1 = std::atan2(ix.y - cy1, ix.x - cx1);
            ix.param2 = std::atan2(ix.y - cy2, ix.x - cx2);
            result.push_back(ix);
        }
    }
    return result;
}

std::vector<Intersection> Sketch::filterForArc(
    const std::vector<Intersection>& hits,
    double cx, double cy, double startAngle, double endAngle,
    bool isParam1)
{
    std::vector<Intersection> result;
    for (auto ix : hits) {
        double angle = std::atan2(ix.y - cy, ix.x - cx);
        if (angleInArc(angle, startAngle, endAngle)) {
            // Recalculate param as fraction of arc sweep
            double frac = normaliseAngle(angle - startAngle) / arcSweep(startAngle, endAngle);
            if (isParam1) ix.param1 = frac;
            else          ix.param2 = frac;
            result.push_back(ix);
        }
    }
    return result;
}

// ── Public intersection API ───────────────────────────────────────────────

std::vector<Intersection> Sketch::findIntersections(const std::string& id1,
                                                     const std::string& id2) const
{
    // Determine entity types and dispatch
    auto isLine   = [&](const std::string& id) { return m_lines.count(id) > 0; };
    auto isCircle = [&](const std::string& id) { return m_circles.count(id) > 0; };
    auto isArc    = [&](const std::string& id) { return m_arcs.count(id) > 0; };

    // Helper lambdas to get geometry
    auto lineGeom = [&](const std::string& id, double& x1, double& y1, double& x2, double& y2) {
        const auto& ln = m_lines.at(id);
        const auto& p1 = m_points.at(ln.startPointId);
        const auto& p2 = m_points.at(ln.endPointId);
        x1 = p1.x; y1 = p1.y; x2 = p2.x; y2 = p2.y;
    };
    auto circGeom = [&](const std::string& id, double& cx, double& cy, double& r) {
        const auto& c = m_circles.at(id);
        const auto& cp = m_points.at(c.centerPointId);
        cx = cp.x; cy = cp.y; r = c.radius;
    };
    auto arcGeom = [&](const std::string& id, double& cx, double& cy, double& r,
                       double& sa, double& ea) {
        const auto& a = m_arcs.at(id);
        const auto& cp = m_points.at(a.centerPointId);
        cx = cp.x; cy = cp.y; r = a.radius;
        arcAngles(a, sa, ea);
    };

    // Line-Line
    if (isLine(id1) && isLine(id2)) {
        double x1,y1,x2,y2, x3,y3,x4,y4;
        lineGeom(id1, x1,y1,x2,y2);
        lineGeom(id2, x3,y3,x4,y4);
        return intersectLineLine(x1,y1,x2,y2, x3,y3,x4,y4);
    }

    // Line-Circle
    if (isLine(id1) && isCircle(id2)) {
        double x1,y1,x2,y2, cx,cy,r;
        lineGeom(id1, x1,y1,x2,y2);
        circGeom(id2, cx,cy,r);
        return intersectLineCircle(x1,y1,x2,y2, cx,cy,r);
    }
    if (isCircle(id1) && isLine(id2)) {
        double x1,y1,x2,y2, cx,cy,r;
        lineGeom(id2, x1,y1,x2,y2);
        circGeom(id1, cx,cy,r);
        auto hits = intersectLineCircle(x1,y1,x2,y2, cx,cy,r);
        for (auto& h : hits) std::swap(h.param1, h.param2);
        return hits;
    }

    // Line-Arc
    if (isLine(id1) && isArc(id2)) {
        double x1,y1,x2,y2, cx,cy,r,sa,ea;
        lineGeom(id1, x1,y1,x2,y2);
        arcGeom(id2, cx,cy,r,sa,ea);
        auto hits = intersectLineCircle(x1,y1,x2,y2, cx,cy,r);
        return filterForArc(hits, cx,cy,sa,ea, false);
    }
    if (isArc(id1) && isLine(id2)) {
        double x1,y1,x2,y2, cx,cy,r,sa,ea;
        lineGeom(id2, x1,y1,x2,y2);
        arcGeom(id1, cx,cy,r,sa,ea);
        auto hits = intersectLineCircle(x1,y1,x2,y2, cx,cy,r);
        // swap params and filter for arc on param1 side
        for (auto& h : hits) std::swap(h.param1, h.param2);
        return filterForArc(hits, cx,cy,sa,ea, true);
    }

    // Circle-Circle
    if (isCircle(id1) && isCircle(id2)) {
        double cx1,cy1,r1, cx2,cy2,r2;
        circGeom(id1, cx1,cy1,r1);
        circGeom(id2, cx2,cy2,r2);
        return intersectCircleCircle(cx1,cy1,r1, cx2,cy2,r2);
    }

    // Circle-Arc
    if (isCircle(id1) && isArc(id2)) {
        double cx1,cy1,r1, cx2,cy2,r2,sa,ea;
        circGeom(id1, cx1,cy1,r1);
        arcGeom(id2, cx2,cy2,r2,sa,ea);
        auto hits = intersectCircleCircle(cx1,cy1,r1, cx2,cy2,r2);
        return filterForArc(hits, cx2,cy2,sa,ea, false);
    }
    if (isArc(id1) && isCircle(id2)) {
        double cx1,cy1,r1,sa,ea, cx2,cy2,r2;
        arcGeom(id1, cx1,cy1,r1,sa,ea);
        circGeom(id2, cx2,cy2,r2);
        auto hits = intersectCircleCircle(cx1,cy1,r1, cx2,cy2,r2);
        return filterForArc(hits, cx1,cy1,sa,ea, true);
    }

    // Arc-Arc
    if (isArc(id1) && isArc(id2)) {
        double cx1,cy1,r1,sa1,ea1, cx2,cy2,r2,sa2,ea2;
        arcGeom(id1, cx1,cy1,r1,sa1,ea1);
        arcGeom(id2, cx2,cy2,r2,sa2,ea2);
        auto hits = intersectCircleCircle(cx1,cy1,r1, cx2,cy2,r2);
        // Filter for both arcs
        std::vector<Intersection> filtered;
        for (auto& h : hits) {
            if (angleInArc(std::atan2(h.y-cy1, h.x-cx1), sa1, ea1) &&
                angleInArc(std::atan2(h.y-cy2, h.x-cx2), sa2, ea2)) {
                h.param1 = normaliseAngle(std::atan2(h.y-cy1, h.x-cx1) - sa1) / arcSweep(sa1,ea1);
                h.param2 = normaliseAngle(std::atan2(h.y-cy2, h.x-cx2) - sa2) / arcSweep(sa2,ea2);
                filtered.push_back(h);
            }
        }
        return filtered;
    }

    return {};
}

std::vector<Intersection> Sketch::findAllIntersections(const std::string& entityId) const
{
    std::vector<Intersection> all;
    auto collect = [&](const std::string& otherId) {
        if (otherId == entityId) return;
        auto hits = findIntersections(entityId, otherId);
        all.insert(all.end(), hits.begin(), hits.end());
    };

    for (const auto& [id, _] : m_lines)   collect(id);
    for (const auto& [id, _] : m_circles) collect(id);
    for (const auto& [id, _] : m_arcs)    collect(id);
    return all;
}

// ── Point sharing check ───────────────────────────────────────────────────

bool Sketch::isPointShared(const std::string& pointId, const std::string& excludeEntityId) const
{
    for (const auto& [id, ln] : m_lines) {
        if (id == excludeEntityId) continue;
        if (ln.startPointId == pointId || ln.endPointId == pointId) return true;
    }
    for (const auto& [id, c] : m_circles) {
        if (id == excludeEntityId) continue;
        if (c.centerPointId == pointId) return true;
    }
    for (const auto& [id, a] : m_arcs) {
        if (id == excludeEntityId) continue;
        if (a.centerPointId == pointId || a.startPointId == pointId || a.endPointId == pointId)
            return true;
    }
    for (const auto& [id, s] : m_splines) {
        if (id == excludeEntityId) continue;
        for (const auto& pid : s.controlPointIds)
            if (pid == pointId) return true;
    }
    for (const auto& [id, e] : m_ellipses) {
        if (id == excludeEntityId) continue;
        if (e.centerPointId == pointId) return true;
    }
    return false;
}

// ── Remove entity ─────────────────────────────────────────────────────────

void Sketch::removeEntity(const std::string& entityId)
{
    // Remove associated constraints
    std::vector<std::string> constraintsToRemove;
    for (const auto& [cid, con] : m_constraints) {
        for (const auto& eid : con.entityIds) {
            if (eid == entityId) {
                constraintsToRemove.push_back(cid);
                break;
            }
        }
    }
    for (const auto& cid : constraintsToRemove)
        m_constraints.erase(cid);

    // Collect points to possibly remove
    std::vector<std::string> pointIds;

    auto lineIt = m_lines.find(entityId);
    if (lineIt != m_lines.end()) {
        pointIds.push_back(lineIt->second.startPointId);
        pointIds.push_back(lineIt->second.endPointId);
        m_lines.erase(lineIt);
    }
    auto circIt = m_circles.find(entityId);
    if (circIt != m_circles.end()) {
        pointIds.push_back(circIt->second.centerPointId);
        m_circles.erase(circIt);
    }
    auto arcIt = m_arcs.find(entityId);
    if (arcIt != m_arcs.end()) {
        pointIds.push_back(arcIt->second.centerPointId);
        pointIds.push_back(arcIt->second.startPointId);
        pointIds.push_back(arcIt->second.endPointId);
        m_arcs.erase(arcIt);
    }
    auto ellIt = m_ellipses.find(entityId);
    if (ellIt != m_ellipses.end()) {
        pointIds.push_back(ellIt->second.centerPointId);
        m_ellipses.erase(ellIt);
    }

    // Remove points that are no longer used by any entity
    for (const auto& pid : pointIds) {
        if (!isPointShared(pid, entityId)) {
            // Also remove constraints referencing this point
            std::vector<std::string> ptCons;
            for (const auto& [cid, con] : m_constraints) {
                for (const auto& eid : con.entityIds) {
                    if (eid == pid) { ptCons.push_back(cid); break; }
                }
            }
            for (const auto& cid : ptCons)
                m_constraints.erase(cid);
            m_points.erase(pid);
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// Trim
// ══════════════════════════════════════════════════════════════════════════════

std::vector<std::string> Sketch::trim(const std::string& entityId, double px, double py)
{
    // Find all intersections of this entity with others
    auto allHits = findAllIntersections(entityId);
    if (allHits.empty())
        return {};  // nothing to trim against

    // Sort by param1 (parameter along the target entity)
    std::sort(allHits.begin(), allHits.end(),
              [](const Intersection& a, const Intersection& b) {
                  return a.param1 < b.param1;
              });

    // Deduplicate close intersections
    {
        std::vector<Intersection> deduped;
        for (const auto& h : allHits) {
            if (!deduped.empty()) {
                double dx = h.x - deduped.back().x;
                double dy = h.y - deduped.back().y;
                if (dx*dx + dy*dy < kEps*kEps) continue;
            }
            deduped.push_back(h);
        }
        allHits = std::move(deduped);
    }
    if (allHits.empty()) return {};

    std::vector<std::string> newIds;

    // ── Trim a LINE ─────────────────────────────────────────────────────
    if (m_lines.count(entityId)) {
        const auto& ln = m_lines.at(entityId);
        const auto& p1 = m_points.at(ln.startPointId);
        const auto& p2 = m_points.at(ln.endPointId);

        // Compute parameter of click point on line
        double dx = p2.x - p1.x, dy = p2.y - p1.y;
        double len2 = dx*dx + dy*dy;
        double tClick = 0.5;
        if (len2 > kEps) tClick = ((px - p1.x)*dx + (py - p1.y)*dy) / len2;
        tClick = std::clamp(tClick, 0.0, 1.0);

        // Find the segment containing the click: between two consecutive intersection params
        // Segments: [0, t0], [t0, t1], ..., [tn, 1]
        std::vector<double> params;
        params.push_back(0.0);
        for (const auto& h : allHits) {
            double t = std::clamp(h.param1, 0.0, 1.0);
            params.push_back(t);
        }
        params.push_back(1.0);

        // Find which segment the click falls in
        int clickSeg = -1;
        for (size_t i = 0; i + 1 < params.size(); ++i) {
            if (tClick >= params[i] - kEps && tClick <= params[i+1] + kEps) {
                clickSeg = static_cast<int>(i);
                break;
            }
        }
        if (clickSeg < 0) return {};

        // Remove the original entity
        bool wasConstruction = ln.isConstruction;
        std::string origStartPtId = ln.startPointId;
        std::string origEndPtId = ln.endPointId;
        double sx = p1.x, sy_ = p1.y, ex = p2.x, ey = p2.y;
        removeEntity(entityId);

        // Create new line segments for portions NOT clicked
        for (size_t i = 0; i + 1 < params.size(); ++i) {
            if (static_cast<int>(i) == clickSeg) continue;
            double t0 = params[i], t1 = params[i+1];
            if (t1 - t0 < kEps) continue;

            double nx1 = sx + t0 * (ex - sx), ny1 = sy_ + t0 * (ey - sy_);
            double nx2 = sx + t1 * (ex - sx), ny2 = sy_ + t1 * (ey - sy_);
            auto newLnId = addLine(nx1, ny1, nx2, ny2);
            if (wasConstruction)
                m_lines[newLnId].isConstruction = true;
            newIds.push_back(newLnId);
        }
        return newIds;
    }

    // ── Trim a CIRCLE ───────────────────────────────────────────────────
    if (m_circles.count(entityId)) {
        const auto& circ = m_circles.at(entityId);
        const auto& cp = m_points.at(circ.centerPointId);
        double r = circ.radius;
        double cx = cp.x, cy = cp.y;

        // Param is angle; recompute intersection angles
        std::vector<double> angles;
        for (const auto& h : allHits) {
            angles.push_back(normaliseAngle(std::atan2(h.y - cy, h.x - cx)));
        }
        // Sort angles
        std::sort(angles.begin(), angles.end());

        // Find which arc segment contains click point
        double clickAngle = normaliseAngle(std::atan2(py - cy, px - cx));

        // Segments between consecutive intersection angles (wrapping)
        int n = static_cast<int>(angles.size());
        int clickSeg = -1;
        for (int i = 0; i < n; ++i) {
            double a0 = angles[i];
            double a1 = angles[(i + 1) % n];
            if (angleInArc(clickAngle, a0, a1)) {
                clickSeg = i;
                break;
            }
        }
        if (clickSeg < 0) return {};

        bool wasConstruction = circ.isConstruction;
        removeEntity(entityId);

        // Create arcs for the remaining segments
        for (int i = 0; i < n; ++i) {
            if (i == clickSeg) continue;
            double a0 = angles[i];
            double a1 = angles[(i + 1) % n];
            auto newArcId = addArc(cx, cy, r, a0, a1);
            if (wasConstruction)
                m_arcs[newArcId].isConstruction = true;
            newIds.push_back(newArcId);
        }
        return newIds;
    }

    // ── Trim an ARC ─────────────────────────────────────────────────────
    if (m_arcs.count(entityId)) {
        const auto& a = m_arcs.at(entityId);
        const auto& cp = m_points.at(a.centerPointId);
        double r = a.radius;
        double cx = cp.x, cy = cp.y;
        double sa, ea;
        arcAngles(a, sa, ea);

        // Intersection params are fractions of arc sweep.  Convert to angles.
        std::vector<double> angles;
        double sweep = arcSweep(sa, ea);
        for (const auto& h : allHits) {
            double angle = sa + h.param1 * sweep;
            angles.push_back(normaliseAngle(angle));
        }

        // Build ordered list: start, intersections (sorted by param), end
        std::vector<double> params;
        params.push_back(0.0);
        for (const auto& h : allHits)
            params.push_back(h.param1);
        params.push_back(1.0);

        // Click parameter
        double clickAngle = std::atan2(py - cy, px - cx);
        double clickParam = normaliseAngle(clickAngle - sa) / sweep;
        clickParam = std::clamp(clickParam, 0.0, 1.0);

        // Find segment containing click
        int clickSeg = -1;
        for (size_t i = 0; i + 1 < params.size(); ++i) {
            if (clickParam >= params[i] - kEps && clickParam <= params[i+1] + kEps) {
                clickSeg = static_cast<int>(i);
                break;
            }
        }
        if (clickSeg < 0) return {};

        bool wasConstruction = a.isConstruction;
        removeEntity(entityId);

        // Create new arcs for remaining segments
        for (size_t i = 0; i + 1 < params.size(); ++i) {
            if (static_cast<int>(i) == clickSeg) continue;
            double t0 = params[i], t1 = params[i+1];
            if (t1 - t0 < kEps) continue;
            double newSa = sa + t0 * sweep;
            double newEa = sa + t1 * sweep;
            auto newArcId = addArc(cx, cy, r, newSa, newEa);
            if (wasConstruction)
                m_arcs[newArcId].isConstruction = true;
            newIds.push_back(newArcId);
        }
        return newIds;
    }

    return {};
}

// ══════════════════════════════════════════════════════════════════════════════
// Extend
// ══════════════════════════════════════════════════════════════════════════════

bool Sketch::extend(const std::string& entityId, int endpointIndex)
{
    // ── Extend a LINE ───────────────────────────────────────────────────
    if (m_lines.count(entityId)) {
        auto& ln = m_lines.at(entityId);
        auto& startPt = m_points.at(ln.startPointId);
        auto& endPt   = m_points.at(ln.endPointId);

        // The endpoint to extend
        double epx, epy;    // the endpoint we're extending
        double opx, opy;    // the other endpoint (anchor)
        if (endpointIndex == 0) {
            epx = startPt.x; epy = startPt.y;
            opx = endPt.x;   opy = endPt.y;
        } else {
            epx = endPt.x;   epy = endPt.y;
            opx = startPt.x; opy = startPt.y;
        }

        // Direction from anchor toward endpoint (the extension direction)
        double dirX = epx - opx, dirY = epy - opy;
        double dirLen = std::sqrt(dirX*dirX + dirY*dirY);
        if (dirLen < kEps) return false;
        dirX /= dirLen; dirY /= dirLen;

        // Shoot a ray from the endpoint in dirX,dirY direction.
        // We create a very long virtual line and intersect with all other entities.
        double rayEndX = epx + dirX * 1e6;
        double rayEndY = epy + dirY * 1e6;

        // Find the nearest intersection with any other entity along the extension direction
        double bestDist = 1e18;
        double bestX = 0, bestY = 0;
        bool found = false;

        auto tryIntersect = [&](const std::string& otherId) {
            if (otherId == entityId) return;

            // We need to intersect the RAY (from endpoint forward) with the other entity.
            // For lines, we intersect with the segment.
            // For circles/arcs, we intersect with the full curve.

            if (m_lines.count(otherId)) {
                const auto& oln = m_lines.at(otherId);
                const auto& op1 = m_points.at(oln.startPointId);
                const auto& op2 = m_points.at(oln.endPointId);
                auto hits = intersectLineLine(epx, epy, rayEndX, rayEndY,
                                              op1.x, op1.y, op2.x, op2.y);
                for (const auto& h : hits) {
                    double dx = h.x - epx, dy = h.y - epy;
                    double dist = std::sqrt(dx*dx + dy*dy);
                    if (dist > kEps && dist < bestDist) {
                        bestDist = dist; bestX = h.x; bestY = h.y; found = true;
                    }
                }
            }
            if (m_circles.count(otherId)) {
                const auto& oc = m_circles.at(otherId);
                const auto& ocp = m_points.at(oc.centerPointId);
                auto hits = intersectLineCircle(epx, epy, rayEndX, rayEndY,
                                                ocp.x, ocp.y, oc.radius);
                for (const auto& h : hits) {
                    double dx = h.x - epx, dy = h.y - epy;
                    double dist = std::sqrt(dx*dx + dy*dy);
                    if (dist > kEps && dist < bestDist) {
                        bestDist = dist; bestX = h.x; bestY = h.y; found = true;
                    }
                }
            }
            if (m_arcs.count(otherId)) {
                const auto& oa = m_arcs.at(otherId);
                const auto& ocp = m_points.at(oa.centerPointId);
                double sa, ea;
                arcAngles(oa, sa, ea);
                auto hits = intersectLineCircle(epx, epy, rayEndX, rayEndY,
                                                ocp.x, ocp.y, oa.radius);
                hits = filterForArc(hits, ocp.x, ocp.y, sa, ea, false);
                for (const auto& h : hits) {
                    double dx = h.x - epx, dy = h.y - epy;
                    double dist = std::sqrt(dx*dx + dy*dy);
                    if (dist > kEps && dist < bestDist) {
                        bestDist = dist; bestX = h.x; bestY = h.y; found = true;
                    }
                }
            }
        };

        for (const auto& [id, _] : m_lines)   tryIntersect(id);
        for (const auto& [id, _] : m_circles) tryIntersect(id);
        for (const auto& [id, _] : m_arcs)    tryIntersect(id);

        if (!found) return false;

        // Move the endpoint
        if (endpointIndex == 0) {
            startPt.x = bestX; startPt.y = bestY;
        } else {
            endPt.x = bestX; endPt.y = bestY;
        }
        return true;
    }

    // ── Extend an ARC ───────────────────────────────────────────────────
    if (m_arcs.count(entityId)) {
        auto& a = m_arcs.at(entityId);
        const auto& cp = m_points.at(a.centerPointId);
        double cx = cp.x, cy = cp.y;
        double r = a.radius;
        double sa, ea;
        arcAngles(a, sa, ea);

        // Determine extension direction
        double extAngle;
        if (endpointIndex == 0) {
            // Extend start backwards (decrease startAngle CCW)
            extAngle = sa;
        } else {
            // Extend end forwards (increase endAngle CCW)
            extAngle = ea;
        }

        // Find nearest intersection along the arc extension direction
        // We scan a reasonable angular range (up to 2*pi) for intersections
        double bestAngularDist = 2.0 * M_PI + 1.0;
        double bestAngle = extAngle;
        bool found = false;

        auto tryIntersect = [&](const std::string& otherId) {
            if (otherId == entityId) return;

            // Intersect the full circle of this arc with the other entity
            std::vector<Intersection> hits;
            if (m_lines.count(otherId)) {
                const auto& oln = m_lines.at(otherId);
                const auto& op1 = m_points.at(oln.startPointId);
                const auto& op2 = m_points.at(oln.endPointId);
                hits = intersectLineCircle(op1.x, op1.y, op2.x, op2.y, cx, cy, r);
            } else if (m_circles.count(otherId)) {
                const auto& oc = m_circles.at(otherId);
                const auto& ocp = m_points.at(oc.centerPointId);
                hits = intersectCircleCircle(cx, cy, r, ocp.x, ocp.y, oc.radius);
            } else if (m_arcs.count(otherId)) {
                const auto& oa = m_arcs.at(otherId);
                const auto& ocp = m_points.at(oa.centerPointId);
                hits = intersectCircleCircle(cx, cy, r, ocp.x, ocp.y, oa.radius);
                double osa, oea;
                arcAngles(oa, osa, oea);
                hits = filterForArc(hits, ocp.x, ocp.y, osa, oea, false);
            }

            for (const auto& h : hits) {
                double hitAngle = std::atan2(h.y - cy, h.x - cx);
                // Check that the hit is OUTSIDE the current arc
                if (angleInArc(hitAngle, sa, ea)) continue;

                double angDist;
                if (endpointIndex == 0) {
                    // Extending start backwards: angular distance from hit to current start
                    angDist = normaliseAngle(sa - hitAngle);
                } else {
                    // Extending end forwards: angular distance from current end to hit
                    angDist = normaliseAngle(hitAngle - ea);
                }
                if (angDist > kEps && angDist < bestAngularDist) {
                    bestAngularDist = angDist;
                    bestAngle = hitAngle;
                    found = true;
                }
            }
        };

        for (const auto& [id, _] : m_lines)   tryIntersect(id);
        for (const auto& [id, _] : m_circles) tryIntersect(id);
        for (const auto& [id, _] : m_arcs)    tryIntersect(id);

        if (!found) return false;

        // Move the appropriate endpoint
        if (endpointIndex == 0) {
            auto& sp = m_points.at(a.startPointId);
            sp.x = cx + r * std::cos(bestAngle);
            sp.y = cy + r * std::sin(bestAngle);
        } else {
            auto& ep = m_points.at(a.endPointId);
            ep.x = cx + r * std::cos(bestAngle);
            ep.y = cy + r * std::sin(bestAngle);
        }
        return true;
    }

    return false;
}

// ══════════════════════════════════════════════════════════════════════════════
// Offset
// ══════════════════════════════════════════════════════════════════════════════

std::vector<std::string> Sketch::offset(const std::vector<std::string>& entityIds, double distance)
{
    std::vector<std::string> newIds;

    for (const auto& eid : entityIds) {
        // ── Offset a LINE ───────────────────────────────────────────────
        if (m_lines.count(eid)) {
            const auto& ln = m_lines.at(eid);
            const auto& p1 = m_points.at(ln.startPointId);
            const auto& p2 = m_points.at(ln.endPointId);

            double dx = p2.x - p1.x, dy = p2.y - p1.y;
            double len = std::sqrt(dx*dx + dy*dy);
            if (len < kEps) continue;

            // Normal direction (perpendicular to line, pointing left)
            double nx = -dy / len, ny = dx / len;

            double nx1 = p1.x + distance * nx, ny1 = p1.y + distance * ny;
            double nx2 = p2.x + distance * nx, ny2 = p2.y + distance * ny;

            auto newLnId = addLine(nx1, ny1, nx2, ny2);
            if (ln.isConstruction)
                m_lines[newLnId].isConstruction = true;
            newIds.push_back(newLnId);
            continue;
        }

        // ── Offset a CIRCLE ─────────────────────────────────────────────
        if (m_circles.count(eid)) {
            const auto& circ = m_circles.at(eid);
            const auto& cp = m_points.at(circ.centerPointId);
            double newR = circ.radius + distance;
            if (newR < kEps) continue;  // skip degenerate

            auto newCircId = addCircle(cp.x, cp.y, newR);
            if (circ.isConstruction)
                m_circles[newCircId].isConstruction = true;
            newIds.push_back(newCircId);
            continue;
        }

        // ── Offset an ARC ───────────────────────────────────────────────
        if (m_arcs.count(eid)) {
            const auto& a = m_arcs.at(eid);
            const auto& cp = m_points.at(a.centerPointId);
            double newR = a.radius + distance;
            if (newR < kEps) continue;

            double sa, ea;
            arcAngles(a, sa, ea);
            auto newArcId = addArc(cp.x, cp.y, newR, sa, ea);
            if (a.isConstruction)
                m_arcs[newArcId].isConstruction = true;
            newIds.push_back(newArcId);
            continue;
        }
    }

    return newIds;
}

// ══════════════════════════════════════════════════════════════════════════════
// Additional operations (projectEdge, offsetCurve, trimCurve, sketchFillet, sketchChamfer)
// ══════════════════════════════════════════════════════════════════════════════

std::string Sketch::projectEdge(const std::vector<std::pair<double,double>>& projectedPoints)
{
    if (projectedPoints.size() < 2)
        return {};
    if (projectedPoints.size() == 2) {
        auto p1 = addPoint(projectedPoints[0].first, projectedPoints[0].second);
        auto p2 = addPoint(projectedPoints[1].first, projectedPoints[1].second);
        auto lineId = addLine(p1, p2, /*isConstruction=*/true);
        return lineId;
    }
    auto splineId = addSpline(projectedPoints, 3);
    if (m_splines.count(splineId))
        m_splines[splineId].isConstruction = true;
    return splineId;
}

std::string Sketch::offsetCurve(const std::string& curveId, double distance)
{
    auto results = offset({curveId}, distance);
    if (results.empty()) return {};
    return results.front();
}

bool Sketch::trimCurve(const std::string& curveId, double pickX, double pickY)
{
    auto newIds = trim(curveId, pickX, pickY);
    return !newIds.empty() || (!m_lines.count(curveId) && !m_circles.count(curveId) && !m_arcs.count(curveId));
}

std::string Sketch::sketchFillet(const std::string& line1Id, const std::string& line2Id, double radius)
{
    if (!m_lines.count(line1Id) || !m_lines.count(line2Id))
        return {};

    const auto& ln1 = m_lines.at(line1Id);
    const auto& ln2 = m_lines.at(line2Id);

    // Find shared endpoint
    std::string sharedPtId;
    std::string ln1Other, ln2Other;
    if (ln1.startPointId == ln2.startPointId) {
        sharedPtId = ln1.startPointId; ln1Other = ln1.endPointId; ln2Other = ln2.endPointId;
    } else if (ln1.startPointId == ln2.endPointId) {
        sharedPtId = ln1.startPointId; ln1Other = ln1.endPointId; ln2Other = ln2.startPointId;
    } else if (ln1.endPointId == ln2.startPointId) {
        sharedPtId = ln1.endPointId; ln1Other = ln1.startPointId; ln2Other = ln2.endPointId;
    } else if (ln1.endPointId == ln2.endPointId) {
        sharedPtId = ln1.endPointId; ln1Other = ln1.startPointId; ln2Other = ln2.startPointId;
    } else {
        return {};  // lines don't share an endpoint
    }

    const auto& vp = m_points.at(sharedPtId);
    const auto& p1 = m_points.at(ln1Other);
    const auto& p2 = m_points.at(ln2Other);

    // Direction vectors from vertex
    double d1x = p1.x - vp.x, d1y = p1.y - vp.y;
    double d2x = p2.x - vp.x, d2y = p2.y - vp.y;
    double len1 = std::sqrt(d1x*d1x + d1y*d1y);
    double len2 = std::sqrt(d2x*d2x + d2y*d2y);
    if (len1 < kEps || len2 < kEps) return {};
    d1x /= len1; d1y /= len1;
    d2x /= len2; d2y /= len2;

    // Angle between lines
    double dot = d1x*d2x + d1y*d2y;
    double cross = d1x*d2y - d1y*d2x;
    double halfAngle = std::acos(std::clamp(dot, -1.0, 1.0)) / 2.0;
    if (halfAngle < kEps) return {};

    double dist = radius / std::tan(halfAngle);
    if (dist > len1 || dist > len2) return {};

    // Tangent points on the two lines
    double t1x = vp.x + dist * d1x, t1y = vp.y + dist * d1y;
    double t2x = vp.x + dist * d2x, t2y = vp.y + dist * d2y;

    // Arc center: offset from vertex along bisector
    double bx = d1x + d2x, by = d1y + d2y;
    double blen = std::sqrt(bx*bx + by*by);
    if (blen < kEps) return {};
    bx /= blen; by /= blen;
    double centerDist = radius / std::sin(halfAngle);
    double cx = vp.x + centerDist * bx, cy = vp.y + centerDist * by;

    double startAngle = std::atan2(t1y - cy, t1x - cx);
    double endAngle   = std::atan2(t2y - cy, t2x - cx);

    // Make sure arc goes the right way (shorter arc)
    if (cross < 0) std::swap(startAngle, endAngle);

    // Shorten the original lines: move shared vertex to tangent points
    // We need to split: move the shared endpoint of line1 to t1, line2 to t2
    // Since they share the same point, we need to unshare: create new points
    std::string newPt1Id = addPoint(t1x, t1y);
    std::string newPt2Id = addPoint(t2x, t2y);

    auto& mln1 = m_lines.at(line1Id);
    auto& mln2 = m_lines.at(line2Id);
    if (mln1.startPointId == sharedPtId) mln1.startPointId = newPt1Id;
    else mln1.endPointId = newPt1Id;
    if (mln2.startPointId == sharedPtId) mln2.startPointId = newPt2Id;
    else mln2.endPointId = newPt2Id;

    // Remove the shared point if no longer used
    if (!isPointShared(sharedPtId, ""))
        m_points.erase(sharedPtId);

    // Create the fillet arc
    return addArc(cx, cy, radius, startAngle, endAngle);
}

std::string Sketch::sketchChamfer(const std::string& line1Id, const std::string& line2Id, double distance)
{
    if (!m_lines.count(line1Id) || !m_lines.count(line2Id))
        return {};

    const auto& ln1 = m_lines.at(line1Id);
    const auto& ln2 = m_lines.at(line2Id);

    // Find shared endpoint
    std::string sharedPtId;
    std::string ln1Other, ln2Other;
    if (ln1.startPointId == ln2.startPointId) {
        sharedPtId = ln1.startPointId; ln1Other = ln1.endPointId; ln2Other = ln2.endPointId;
    } else if (ln1.startPointId == ln2.endPointId) {
        sharedPtId = ln1.startPointId; ln1Other = ln1.endPointId; ln2Other = ln2.startPointId;
    } else if (ln1.endPointId == ln2.startPointId) {
        sharedPtId = ln1.endPointId; ln1Other = ln1.startPointId; ln2Other = ln2.endPointId;
    } else if (ln1.endPointId == ln2.endPointId) {
        sharedPtId = ln1.endPointId; ln1Other = ln1.startPointId; ln2Other = ln2.startPointId;
    } else {
        return {};
    }

    const auto& vp = m_points.at(sharedPtId);
    const auto& p1 = m_points.at(ln1Other);
    const auto& p2 = m_points.at(ln2Other);

    double d1x = p1.x - vp.x, d1y = p1.y - vp.y;
    double d2x = p2.x - vp.x, d2y = p2.y - vp.y;
    double len1 = std::sqrt(d1x*d1x + d1y*d1y);
    double len2 = std::sqrt(d2x*d2x + d2y*d2y);
    if (len1 < kEps || len2 < kEps || distance > len1 || distance > len2)
        return {};
    d1x /= len1; d1y /= len1;
    d2x /= len2; d2y /= len2;

    // Chamfer points on each line at `distance` from vertex
    double c1x = vp.x + distance * d1x, c1y = vp.y + distance * d1y;
    double c2x = vp.x + distance * d2x, c2y = vp.y + distance * d2y;

    // Create new endpoint points and unshare
    std::string newPt1Id = addPoint(c1x, c1y);
    std::string newPt2Id = addPoint(c2x, c2y);

    auto& mln1 = m_lines.at(line1Id);
    auto& mln2 = m_lines.at(line2Id);
    if (mln1.startPointId == sharedPtId) mln1.startPointId = newPt1Id;
    else mln1.endPointId = newPt1Id;
    if (mln2.startPointId == sharedPtId) mln2.startPointId = newPt2Id;
    else mln2.endPointId = newPt2Id;

    if (!isPointShared(sharedPtId, ""))
        m_points.erase(sharedPtId);

    // Create the chamfer line
    return addLine(newPt1Id, newPt2Id);
}

} // namespace sketch
