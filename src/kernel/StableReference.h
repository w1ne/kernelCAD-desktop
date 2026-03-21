#pragma once
#include <vector>
#include <string>

class TopoDS_Shape;

namespace kernel {

/// Geometric signature for a face — centroid + normal + area.
/// Used to identify a face across recomputation without relying on
/// volatile integer indices.
struct FaceSignature {
    double cx = 0, cy = 0, cz = 0;   // centroid
    double nx = 0, ny = 0, nz = 0;   // average outward normal
    double area = 0;

    /// Serialise to a compact string "cx,cy,cz;nx,ny,nz;area".
    std::string toString() const;
    /// Parse from a string produced by toString().
    static FaceSignature fromString(const std::string& s);
};

/// Geometric signature for an edge — midpoint + tangent + length.
struct EdgeSignature {
    double mx = 0, my = 0, mz = 0;   // midpoint on the curve
    double tx = 0, ty = 0, tz = 0;   // tangent at midpoint (unit vector)
    double length = 0;

    std::string toString() const;
    static EdgeSignature fromString(const std::string& s);
};

/// Geometry-based stable reference system.
///
/// Instead of storing volatile face/edge indices, features store geometric
/// signatures.  When an upstream shape changes (e.g. a box resizes), the
/// signatures are matched against the new topology to find the correct new
/// indices.
class StableReference {
public:
    // ── Signature computation ───────────────────────────────────────────

    /// Compute a FaceSignature for face at the given index (TopExp_Explorer order).
    static FaceSignature computeFaceSignature(const TopoDS_Shape& shape, int faceIndex);

    /// Compute signatures for every face in a shape.
    static std::vector<FaceSignature> computeFaceSignatures(const TopoDS_Shape& shape);

    /// Compute an EdgeSignature for edge at the given index (TopExp order).
    static EdgeSignature computeEdgeSignature(const TopoDS_Shape& shape, int edgeIndex);

    /// Compute signatures for every edge in a shape.
    static std::vector<EdgeSignature> computeEdgeSignatures(const TopoDS_Shape& shape);

    // ── Matching ────────────────────────────────────────────────────────

    /// Find the face index in @p newShape whose signature best matches
    /// @p target.  Returns -1 if no face scores within @p tolerance
    /// (Euclidean distance on the centroid, weighted by normal agreement
    /// and area ratio).
    static int matchFace(const FaceSignature& target,
                         const TopoDS_Shape& newShape,
                         double tolerance = 1e-1);

    /// Find the edge index in @p newShape whose signature best matches
    /// @p target.  Returns -1 if no good match.
    static int matchEdge(const EdgeSignature& target,
                         const TopoDS_Shape& newShape,
                         double tolerance = 1e-1);

    // ── Batch remapping ─────────────────────────────────────────────────

    /// Remap face indices: for each index in @p oldIndices, compute its
    /// signature from @p oldShape, then find the matching face in
    /// @p newShape.  Returns a vector of new indices (same length as
    /// @p oldIndices).  Un-matchable entries get -1.
    static std::vector<int> remapFaces(const std::vector<int>& oldIndices,
                                       const TopoDS_Shape& oldShape,
                                       const TopoDS_Shape& newShape);

    /// Remap edge indices analogously.
    static std::vector<int> remapEdges(const std::vector<int>& oldIndices,
                                       const TopoDS_Shape& oldShape,
                                       const TopoDS_Shape& newShape);

    /// Remap face indices using pre-computed signatures (avoids redundant
    /// recomputation when signatures are already stored in the feature).
    static std::vector<int> remapFacesFromSignatures(
        const std::vector<FaceSignature>& signatures,
        const TopoDS_Shape& newShape);

    /// Remap edge indices using pre-computed signatures.
    static std::vector<int> remapEdgesFromSignatures(
        const std::vector<EdgeSignature>& signatures,
        const TopoDS_Shape& newShape);
};

} // namespace kernel
