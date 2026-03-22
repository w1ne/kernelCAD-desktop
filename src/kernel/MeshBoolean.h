#pragma once
#include <vector>
#include <cstdint>
#include <TopoDS_Shape.hxx>

namespace kernel {

/// Mesh boolean operations via OCCT sewing + B-Rep booleans.
/// Converts triangle meshes to B-Rep solids, runs the boolean, and
/// re-tessellates the result back to a triangle mesh.
class MeshBoolean {
public:
    struct TriMesh {
        std::vector<float> vertices;      // x,y,z triples
        std::vector<uint32_t> indices;    // triangle indices
    };

    /// Boolean union of two triangle meshes.
    static TriMesh meshUnion(const TriMesh& a, const TriMesh& b);

    /// Boolean subtraction (a minus b).
    static TriMesh meshDifference(const TriMesh& a, const TriMesh& b);

    /// Boolean intersection.
    static TriMesh meshIntersection(const TriMesh& a, const TriMesh& b);

    /// Convert a triangle mesh to an OCCT solid shape via sewing.
    /// The resulting shape can participate in B-Rep boolean operations.
    static TopoDS_Shape meshToShape(const TriMesh& mesh);

    /// Convert an OCCT shape back to a triangle mesh.
    static TriMesh shapeToMesh(const TopoDS_Shape& shape, double deflection = 0.1);
};

} // namespace kernel
