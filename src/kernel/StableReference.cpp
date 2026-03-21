#include "StableReference.h"

#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
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

namespace kernel {

// ── Serialisation helpers ────────────────────────────────────────────────────

std::string FaceSignature::toString() const
{
    std::ostringstream os;
    os << std::setprecision(12)
       << cx << ',' << cy << ',' << cz << ';'
       << nx << ',' << ny << ',' << nz << ';'
       << area;
    return os.str();
}

FaceSignature FaceSignature::fromString(const std::string& s)
{
    FaceSignature sig;
    // Format: "cx,cy,cz;nx,ny,nz;area"
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
    return sig;
}

std::string EdgeSignature::toString() const
{
    std::ostringstream os;
    os << std::setprecision(12)
       << mx << ',' << my << ',' << mz << ';'
       << tx << ',' << ty << ',' << tz << ';'
       << length;
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

int StableReference::matchFace(const FaceSignature& target,
                                const TopoDS_Shape& newShape,
                                double tolerance)
{
    int nFaces = countFaces(newShape);
    if (nFaces == 0)
        return -1;

    int bestIdx = -1;
    double bestScore = std::numeric_limits<double>::max();
    const double tolSq = tolerance * tolerance;

    for (int i = 0; i < nFaces; ++i) {
        FaceSignature cand = computeFaceSignature(newShape, i);

        // Primary: centroid distance
        double cdSq = distSq(target.cx, target.cy, target.cz,
                             cand.cx, cand.cy, cand.cz);
        if (cdSq > tolSq * 100.0)
            continue;   // way too far, skip

        // Normal agreement: 1 - |dot(n_target, n_cand)| in [0, 1]
        // (0 = perfect alignment, 1 = perpendicular)
        double nDot = std::abs(dot3(target.nx, target.ny, target.nz,
                                    cand.nx, cand.ny, cand.nz));
        double normalPenalty = 1.0 - nDot;   // 0..1

        // Area ratio penalty: |log(area_new / area_old)| — scale-invariant
        double areaRatio = 1.0;
        if (target.area > 1e-12 && cand.area > 1e-12)
            areaRatio = std::abs(std::log(cand.area / target.area));
        // Clamp area ratio penalty
        areaRatio = std::min(areaRatio, 5.0);

        // Combined score: centroid distance dominates, then normal, then area
        double score = std::sqrt(cdSq) + normalPenalty * 2.0 + areaRatio * 0.5;

        if (score < bestScore) {
            bestScore = score;
            bestIdx = i;
        }
    }

    // Accept only if the best match is within a reasonable combined threshold.
    // The tolerance parameter primarily controls centroid distance acceptance.
    if (bestScore > tolerance * 20.0)
        return -1;

    return bestIdx;
}

// ── Edge matching ────────────────────────────────────────────────────────────

int StableReference::matchEdge(const EdgeSignature& target,
                                const TopoDS_Shape& newShape,
                                double tolerance)
{
    int nEdges = countEdges(newShape);
    if (nEdges == 0)
        return -1;

    int bestIdx = -1;
    double bestScore = std::numeric_limits<double>::max();
    const double tolSq = tolerance * tolerance;

    for (int i = 0; i < nEdges; ++i) {
        EdgeSignature cand = computeEdgeSignature(newShape, i);

        // Midpoint distance
        double mdSq = distSq(target.mx, target.my, target.mz,
                             cand.mx, cand.my, cand.mz);
        if (mdSq > tolSq * 100.0)
            continue;

        // Tangent agreement: the tangent can be in either direction along the
        // curve, so use the absolute dot product.
        double tDot = std::abs(dot3(target.tx, target.ty, target.tz,
                                    cand.tx, cand.ty, cand.tz));
        double tangentPenalty = 1.0 - tDot;  // 0..1

        // Length ratio penalty
        double lengthRatio = 1.0;
        if (target.length > 1e-12 && cand.length > 1e-12)
            lengthRatio = std::abs(std::log(cand.length / target.length));
        lengthRatio = std::min(lengthRatio, 5.0);

        double score = std::sqrt(mdSq) + tangentPenalty * 2.0 + lengthRatio * 0.5;

        if (score < bestScore) {
            bestScore = score;
            bestIdx = i;
        }
    }

    if (bestScore > tolerance * 20.0)
        return -1;

    return bestIdx;
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
    const TopoDS_Shape& newShape)
{
    std::vector<int> result;
    result.reserve(signatures.size());
    for (const auto& sig : signatures)
        result.push_back(matchFace(sig, newShape));
    return result;
}

std::vector<int> StableReference::remapEdgesFromSignatures(
    const std::vector<EdgeSignature>& signatures,
    const TopoDS_Shape& newShape)
{
    std::vector<int> result;
    result.reserve(signatures.size());
    for (const auto& sig : signatures)
        result.push_back(matchEdge(sig, newShape));
    return result;
}

} // namespace kernel
