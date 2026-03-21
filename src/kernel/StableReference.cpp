#include "StableReference.h"

#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <sstream>
#include <iomanip>
#include <limits>
#include <algorithm>
#include <unordered_map>

namespace kernel {

// ── Serialisation helpers ────────────────────────────────────────────────────

std::string FaceSignature::toString() const
{
    std::ostringstream os;
    os << std::setprecision(12)
       << cx << ',' << cy << ',' << cz << ';'
       << nx << ',' << ny << ',' << nz << ';'
       << area << ';'
       << static_cast<int>(surfaceType);
    return os.str();
}

FaceSignature FaceSignature::fromString(const std::string& s)
{
    FaceSignature sig;
    // Format: "cx,cy,cz;nx,ny,nz;area[;surfType]"
    std::string tok;
    std::vector<double> vals;
    for (char c : s) {
        if (c == ',' || c == ';') {
            if (!tok.empty()) { vals.push_back(std::stod(tok)); tok.clear(); }
        } else {
            tok += c;
        }
    }
    if (!tok.empty()) vals.push_back(std::stod(tok));
    if (vals.size() >= 7) {
        sig.cx = vals[0]; sig.cy = vals[1]; sig.cz = vals[2];
        sig.nx = vals[3]; sig.ny = vals[4]; sig.nz = vals[5];
        sig.area = vals[6];
    }
    if (vals.size() >= 8) {
        int st = static_cast<int>(vals[7]);
        if (st >= 0 && st <= 5)
            sig.surfaceType = static_cast<SurfaceKind>(st);
    }
    return sig;
}

std::string EdgeSignature::toString() const
{
    std::ostringstream os;
    os << std::setprecision(12)
       << mx << ',' << my << ',' << mz << ';'
       << tx << ',' << ty << ',' << tz << ';'
       << length << ';'
       << static_cast<int>(edgeType) << ';'
       << adjNormal1X << ',' << adjNormal1Y << ',' << adjNormal1Z << ';'
       << adjNormal2X << ',' << adjNormal2Y << ',' << adjNormal2Z;
    return os.str();
}

EdgeSignature EdgeSignature::fromString(const std::string& s)
{
    EdgeSignature sig;
    std::string tok;
    std::vector<double> vals;
    for (char c : s) {
        if (c == ',' || c == ';') {
            if (!tok.empty()) { vals.push_back(std::stod(tok)); tok.clear(); }
        } else {
            tok += c;
        }
    }
    if (!tok.empty()) vals.push_back(std::stod(tok));
    if (vals.size() >= 7) {
        sig.mx = vals[0]; sig.my = vals[1]; sig.mz = vals[2];
        sig.tx = vals[3]; sig.ty = vals[4]; sig.tz = vals[5];
        sig.length = vals[6];
    }
    if (vals.size() >= 8) {
        int et = static_cast<int>(vals[7]);
        if (et >= 0 && et <= 2)
            sig.edgeType = static_cast<CurveKind>(et);
    }
    if (vals.size() >= 14) {
        sig.adjNormal1X = vals[8];  sig.adjNormal1Y = vals[9];  sig.adjNormal1Z = vals[10];
        sig.adjNormal2X = vals[11]; sig.adjNormal2Y = vals[12]; sig.adjNormal2Z = vals[13];
    }
    return sig;
}

// ── Internal helpers ─────────────────────────────────────────────────────────

namespace {

/// Retrieve the i-th face from a shape (TopExp_Explorer order).
TopoDS_Face nthFace(const TopoDS_Shape& shape, int index)
{
    int i = 0;
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next(), ++i) {
        if (i == index)
            return TopoDS::Face(ex.Current());
    }
    return {};
}

/// Retrieve the i-th unique edge from a shape (TopExp indexed map order).
TopoDS_Edge nthEdge(const TopoDS_Shape& shape, int index)
{
    TopTools_IndexedMapOfShape edgeMap;
    TopExp::MapShapes(shape, TopAbs_EDGE, edgeMap);
    if (index >= 0 && index < edgeMap.Extent())
        return TopoDS::Edge(edgeMap(index + 1));   // 1-based
    return {};
}

int countEdges(const TopoDS_Shape& shape)
{
    TopTools_IndexedMapOfShape edgeMap;
    TopExp::MapShapes(shape, TopAbs_EDGE, edgeMap);
    return edgeMap.Extent();
}

int countFaces(const TopoDS_Shape& shape)
{
    int n = 0;
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next())
        ++n;
    return n;
}

/// Euclidean distance squared between two 3D points.
double distSq(double ax, double ay, double az,
              double bx, double by, double bz)
{
    double dx = ax - bx, dy = ay - by, dz = az - bz;
    return dx * dx + dy * dy + dz * dz;
}

/// Dot product of two 3-vectors.
double dot3(double ax, double ay, double az,
            double bx, double by, double bz)
{
    return ax * bx + ay * by + az * bz;
}

} // anonymous namespace

// ── Face signature computation ───────────────────────────────────────────────

FaceSignature StableReference::computeFaceSignature(const TopoDS_Shape& shape, int faceIndex)
{
    FaceSignature sig;
    TopoDS_Face face = nthFace(shape, faceIndex);
    if (face.IsNull())
        return sig;

    // Compute area and centroid via GProp
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props);
    sig.area = props.Mass();    // "mass" of the surface = area

    gp_Pnt cog = props.CentreOfMass();
    sig.cx = cog.X();
    sig.cy = cog.Y();
    sig.cz = cog.Z();

    // Compute normal at the parametric midpoint of the surface
    BRepAdaptor_Surface surf(face);
    double uMid = (surf.FirstUParameter() + surf.LastUParameter()) * 0.5;
    double vMid = (surf.FirstVParameter() + surf.LastVParameter()) * 0.5;
    gp_Pnt pnt;
    gp_Vec du, dv;
    surf.D1(uMid, vMid, pnt, du, dv);
    gp_Vec normal = du.Crossed(dv);
    if (normal.Magnitude() > 1e-12) {
        normal.Normalize();
        // Respect face orientation
        if (face.Orientation() == TopAbs_REVERSED)
            normal.Reverse();
        sig.nx = normal.X();
        sig.ny = normal.Y();
        sig.nz = normal.Z();
    }

    // Classify surface type
    switch (surf.GetType()) {
        case GeomAbs_Plane:     sig.surfaceType = SurfaceKind::Planar;      break;
        case GeomAbs_Cylinder:  sig.surfaceType = SurfaceKind::Cylindrical;  break;
        case GeomAbs_Cone:      sig.surfaceType = SurfaceKind::Conical;      break;
        case GeomAbs_Sphere:    sig.surfaceType = SurfaceKind::Spherical;    break;
        case GeomAbs_Torus:     sig.surfaceType = SurfaceKind::Toroidal;     break;
        default:                sig.surfaceType = SurfaceKind::Other;        break;
    }

    return sig;
}

std::vector<FaceSignature> StableReference::computeFaceSignatures(const TopoDS_Shape& shape)
{
    int n = countFaces(shape);
    std::vector<FaceSignature> sigs;
    sigs.reserve(n);
    for (int i = 0; i < n; ++i)
        sigs.push_back(computeFaceSignature(shape, i));
    return sigs;
}

// ── Edge signature computation ───────────────────────────────────────────────

EdgeSignature StableReference::computeEdgeSignature(const TopoDS_Shape& shape, int edgeIndex)
{
    EdgeSignature sig;
    TopoDS_Edge edge = nthEdge(shape, edgeIndex);
    if (edge.IsNull())
        return sig;

    BRepAdaptor_Curve curve(edge);

    // Length via GProp
    GProp_GProps props;
    BRepGProp::LinearProperties(edge, props);
    sig.length = props.Mass();

    // Midpoint: evaluate curve at midpoint parameter
    double uFirst = curve.FirstParameter();
    double uLast  = curve.LastParameter();
    double uMid   = (uFirst + uLast) * 0.5;

    gp_Pnt midPnt;
    gp_Vec tangent;
    curve.D1(uMid, midPnt, tangent);

    sig.mx = midPnt.X();
    sig.my = midPnt.Y();
    sig.mz = midPnt.Z();

    if (tangent.Magnitude() > 1e-12) {
        tangent.Normalize();
        sig.tx = tangent.X();
        sig.ty = tangent.Y();
        sig.tz = tangent.Z();
    }

    // Classify curve type
    switch (curve.GetType()) {
        case GeomAbs_Line:    sig.edgeType = CurveKind::Line;   break;
        case GeomAbs_Circle:  sig.edgeType = CurveKind::Circle; break;
        default:              sig.edgeType = CurveKind::Other;  break;
    }

    // Compute adjacent face normals for topological context.
    // Build an edge-to-face adjacency map and look up which faces border
    // this edge.
    TopTools_IndexedDataMapOfShapeListOfShape edgeFaceMap;
    TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, edgeFaceMap);

    int adjIdx = 0;
    if (edgeFaceMap.Contains(edge)) {
        const TopTools_ListOfShape& adjFaces = edgeFaceMap.FindFromKey(edge);
        for (TopTools_ListIteratorOfListOfShape it(adjFaces); it.More() && adjIdx < 2; it.Next(), ++adjIdx) {
            const TopoDS_Face& adjFace = TopoDS::Face(it.Value());
            BRepAdaptor_Surface adjSurf(adjFace);
            double uM = (adjSurf.FirstUParameter() + adjSurf.LastUParameter()) * 0.5;
            double vM = (adjSurf.FirstVParameter() + adjSurf.LastVParameter()) * 0.5;
            gp_Pnt p;
            gp_Vec dU, dV;
            adjSurf.D1(uM, vM, p, dU, dV);
            gp_Vec n = dU.Crossed(dV);
            if (n.Magnitude() > 1e-12) {
                n.Normalize();
                if (adjFace.Orientation() == TopAbs_REVERSED)
                    n.Reverse();
            }
            if (adjIdx == 0) {
                sig.adjNormal1X = n.X(); sig.adjNormal1Y = n.Y(); sig.adjNormal1Z = n.Z();
            } else {
                sig.adjNormal2X = n.X(); sig.adjNormal2Y = n.Y(); sig.adjNormal2Z = n.Z();
            }
        }
    }

    return sig;
}

std::vector<EdgeSignature> StableReference::computeEdgeSignatures(const TopoDS_Shape& shape)
{
    int n = countEdges(shape);
    std::vector<EdgeSignature> sigs;
    sigs.reserve(n);
    for (int i = 0; i < n; ++i)
        sigs.push_back(computeEdgeSignature(shape, i));
    return sigs;
}

// ── Face matching ────────────────────────────────────────────────────────────

int StableReference::matchFaceScored(const FaceSignature& target,
                                      const std::vector<FaceSignature>& candidates,
                                      double tolerance, double& outScore)
{
    int bestIdx = -1;
    double bestScore = std::numeric_limits<double>::max();

    for (size_t i = 0; i < candidates.size(); ++i) {
        const auto& cand = candidates[i];

        // 1. Centroid distance (primary metric)
        double dx = cand.cx - target.cx;
        double dy = cand.cy - target.cy;
        double dz = cand.cz - target.cz;
        double centroidDist = std::sqrt(dx*dx + dy*dy + dz*dz);

        // Early rejection: skip candidates that are way too far
        if (centroidDist > tolerance * 10.0)
            continue;

        // 2. Normal alignment: dot product in [-1, 1].
        //    Use signed dot so we distinguish same-direction from opposite.
        double normalDot = dot3(target.nx, target.ny, target.nz,
                                cand.nx, cand.ny, cand.nz);
        double normalDiff = 1.0 - normalDot;  // 0 = same, 2 = opposite

        // 3. Area ratio on log scale (handles face splits gracefully)
        double areaDiff = 0.0;
        if (target.area > 1e-12 && cand.area > 1e-12) {
            double ratio = cand.area / target.area;
            areaDiff = std::abs(std::log(std::max(ratio, 0.01)));
        }
        areaDiff = std::min(areaDiff, 5.0);

        // 4. Surface type penalty: large penalty for type mismatch
        double typePenalty = (cand.surfaceType != target.surfaceType) ? 1.0 : 0.0;

        // Composite score (lower is better).
        // Normal is heavily weighted to prevent matching the wrong face when
        // centroids are close (e.g. opposite sides of a thin box).
        double score = centroidDist * 1.0
                     + normalDiff * 50.0
                     + areaDiff * 10.0
                     + typePenalty * 30.0;

        if (score < bestScore) {
            bestScore = score;
            bestIdx = static_cast<int>(i);
        }
    }

    // Reject if best score exceeds a reasonable threshold
    if (bestScore > tolerance * 100.0) {
        outScore = std::numeric_limits<double>::max();
        return -1;
    }

    outScore = bestScore;
    return bestIdx;
}

int StableReference::matchFace(const FaceSignature& target,
                                const TopoDS_Shape& newShape,
                                double tolerance)
{
    auto candidates = computeFaceSignatures(newShape);
    double score;
    return matchFaceScored(target, candidates, tolerance, score);
}

// ── Edge matching ────────────────────────────────────────────────────────────

int StableReference::matchEdgeScored(const EdgeSignature& target,
                                      const std::vector<EdgeSignature>& candidates,
                                      double tolerance, double& outScore)
{
    int bestIdx = -1;
    double bestScore = std::numeric_limits<double>::max();

    for (size_t i = 0; i < candidates.size(); ++i) {
        const auto& cand = candidates[i];

        // 1. Midpoint distance
        double dx = cand.mx - target.mx;
        double dy = cand.my - target.my;
        double dz = cand.mz - target.mz;
        double midDist = std::sqrt(dx*dx + dy*dy + dz*dz);

        if (midDist > tolerance * 10.0)
            continue;

        // 2. Tangent agreement (absolute dot — direction can flip)
        double tDot = std::abs(dot3(target.tx, target.ty, target.tz,
                                    cand.tx, cand.ty, cand.tz));
        double tangentDiff = 1.0 - tDot;  // 0..1

        // 3. Length ratio on log scale
        double lengthDiff = 0.0;
        if (target.length > 1e-12 && cand.length > 1e-12) {
            double ratio = cand.length / target.length;
            lengthDiff = std::abs(std::log(std::max(ratio, 0.01)));
        }
        lengthDiff = std::min(lengthDiff, 5.0);

        // 4. Edge type penalty
        double typePenalty = (cand.edgeType != target.edgeType) ? 1.0 : 0.0;

        // 5. Adjacent face normal agreement (topological context).
        //    Compare the best pairing of the two adjacent normals (order may differ).
        double adjPenalty = 0.0;
        bool hasAdj = (std::abs(target.adjNormal1X) + std::abs(target.adjNormal1Y) + std::abs(target.adjNormal1Z)) > 1e-12;
        bool candHasAdj = (std::abs(cand.adjNormal1X) + std::abs(cand.adjNormal1Y) + std::abs(cand.adjNormal1Z)) > 1e-12;
        if (hasAdj && candHasAdj) {
            // Try both pairings: (1-1, 2-2) and (1-2, 2-1), pick best
            double d11 = 1.0 - dot3(target.adjNormal1X, target.adjNormal1Y, target.adjNormal1Z,
                                     cand.adjNormal1X, cand.adjNormal1Y, cand.adjNormal1Z);
            double d22 = 1.0 - dot3(target.adjNormal2X, target.adjNormal2Y, target.adjNormal2Z,
                                     cand.adjNormal2X, cand.adjNormal2Y, cand.adjNormal2Z);
            double d12 = 1.0 - dot3(target.adjNormal1X, target.adjNormal1Y, target.adjNormal1Z,
                                     cand.adjNormal2X, cand.adjNormal2Y, cand.adjNormal2Z);
            double d21 = 1.0 - dot3(target.adjNormal2X, target.adjNormal2Y, target.adjNormal2Z,
                                     cand.adjNormal1X, cand.adjNormal1Y, cand.adjNormal1Z);
            double pairing1 = d11 + d22;
            double pairing2 = d12 + d21;
            adjPenalty = std::min(pairing1, pairing2);
        }

        double score = midDist * 1.0
                     + tangentDiff * 50.0
                     + lengthDiff * 10.0
                     + typePenalty * 30.0
                     + adjPenalty * 20.0;

        if (score < bestScore) {
            bestScore = score;
            bestIdx = static_cast<int>(i);
        }
    }

    if (bestScore > tolerance * 100.0) {
        outScore = std::numeric_limits<double>::max();
        return -1;
    }

    outScore = bestScore;
    return bestIdx;
}

int StableReference::matchEdge(const EdgeSignature& target,
                                const TopoDS_Shape& newShape,
                                double tolerance)
{
    auto candidates = computeEdgeSignatures(newShape);
    double score;
    return matchEdgeScored(target, candidates, tolerance, score);
}

// ── Batch remapping (from shape) ─────────────────────────────────────────────

std::vector<int> StableReference::remapFaces(const std::vector<int>& oldIndices,
                                              const TopoDS_Shape& oldShape,
                                              const TopoDS_Shape& newShape)
{
    std::vector<int> result;
    result.reserve(oldIndices.size());
    for (int idx : oldIndices) {
        FaceSignature sig = computeFaceSignature(oldShape, idx);
        result.push_back(matchFace(sig, newShape));
    }
    return result;
}

std::vector<int> StableReference::remapEdges(const std::vector<int>& oldIndices,
                                              const TopoDS_Shape& oldShape,
                                              const TopoDS_Shape& newShape)
{
    std::vector<int> result;
    result.reserve(oldIndices.size());
    for (int idx : oldIndices) {
        EdgeSignature sig = computeEdgeSignature(oldShape, idx);
        result.push_back(matchEdge(sig, newShape));
    }
    return result;
}

// ── Batch remapping (from pre-computed signatures) ───────────────────────────

std::vector<int> StableReference::remapFacesFromSignatures(
    const std::vector<FaceSignature>& signatures,
    const TopoDS_Shape& newShape,
    std::vector<bool>* matchedFlags)
{
    // Pre-compute all candidate signatures once (avoid per-query recomputation)
    auto candidates = computeFaceSignatures(newShape);

    std::vector<int> result;
    std::vector<double> scores;
    result.reserve(signatures.size());
    scores.reserve(signatures.size());

    if (matchedFlags) {
        matchedFlags->assign(signatures.size(), false);
    }

    for (const auto& sig : signatures) {
        double score;
        int idx = matchFaceScored(sig, candidates, 1e-1, score);
        result.push_back(idx);
        scores.push_back(score);
    }

    // Resolve ambiguities: no two old signatures should map to the same new index
    resolveAmbiguities(result, scores, matchedFlags);

    // Fill matchedFlags for entries that survived ambiguity resolution
    if (matchedFlags) {
        for (size_t i = 0; i < result.size(); ++i)
            (*matchedFlags)[i] = (result[i] >= 0);
    }

    return result;
}

std::vector<int> StableReference::remapEdgesFromSignatures(
    const std::vector<EdgeSignature>& signatures,
    const TopoDS_Shape& newShape,
    std::vector<bool>* matchedFlags)
{
    auto candidates = computeEdgeSignatures(newShape);

    std::vector<int> result;
    std::vector<double> scores;
    result.reserve(signatures.size());
    scores.reserve(signatures.size());

    if (matchedFlags) {
        matchedFlags->assign(signatures.size(), false);
    }

    for (const auto& sig : signatures) {
        double score;
        int idx = matchEdgeScored(sig, candidates, 1e-1, score);
        result.push_back(idx);
        scores.push_back(score);
    }

    resolveAmbiguities(result, scores, matchedFlags);

    if (matchedFlags) {
        for (size_t i = 0; i < result.size(); ++i)
            (*matchedFlags)[i] = (result[i] >= 0);
    }

    return result;
}

// ── Ambiguity resolution ─────────────────────────────────────────────────────

void StableReference::resolveAmbiguities(std::vector<int>& result,
                                          const std::vector<double>& scores,
                                          std::vector<bool>* matchedFlags)
{
    // Build a map: new_index -> list of (old_index, score)
    std::unordered_map<int, std::vector<std::pair<size_t, double>>> indexMap;
    for (size_t i = 0; i < result.size(); ++i) {
        if (result[i] >= 0)
            indexMap[result[i]].emplace_back(i, scores[i]);
    }

    for (auto& [newIdx, entries] : indexMap) {
        if (entries.size() <= 1)
            continue;

        // Multiple old signatures mapped to the same new index.
        // Keep the one with the best (lowest) score; mark others as unmatched.
        size_t bestEntry = 0;
        double bestScore = entries[0].second;
        for (size_t j = 1; j < entries.size(); ++j) {
            if (entries[j].second < bestScore) {
                bestScore = entries[j].second;
                bestEntry = j;
            }
        }

        for (size_t j = 0; j < entries.size(); ++j) {
            if (j != bestEntry) {
                result[entries[j].first] = -1;
                if (matchedFlags)
                    (*matchedFlags)[entries[j].first] = false;
            }
        }
    }
}

} // namespace kernel
