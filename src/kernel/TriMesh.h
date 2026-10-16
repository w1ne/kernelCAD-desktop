#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Triangle-mesh helpers shared by the kernel's exporters: vertex welding,
// topology checks and the STL / OBJ writers. No OCCT types live here, so the
// logic can be unit-tested on hand-built meshes.
namespace kernel::trimesh {

/// Topology and volume summary of a triangle mesh (units: mm, mm^3).
struct MeshReport {
    std::size_t vertexCount = 0;
    std::size_t triangleCount = 0;
    std::size_t degenerateTriangles = 0;  ///< zero-area or malformed triangles
    std::size_t boundaryEdges = 0;        ///< used by one triangle only: a hole
    std::size_t nonManifoldEdges = 0;     ///< used by three or more triangles
    std::size_t orientationErrors = 0;    ///< shared edge walked the same way twice
    double volume = 0.0;                  ///< signed; positive when normals point outward

    /// Closed, manifold, consistently wound and enclosing positive volume:
    /// the mesh can be sent to a slicer without repair.
    bool watertight() const
    {
        return boundaryEdges == 0 && nonManifoldEdges == 0
            && orientationErrors == 0 && volume > 0.0;
    }
};

/// Merge vertices closer than `tolerance` (mm) and drop triangles that collapse.
/// Works on positions and indices only; per-vertex normals are not carried over.
void weld(std::vector<float>& vertices, std::vector<uint32_t>& indices, double tolerance);

/// Count boundary, non-manifold and mis-oriented edges, and the signed volume.
/// Expects an indexed mesh: triangles that share an edge must share its vertices (see weld).
MeshReport analyze(const std::vector<float>& vertices, const std::vector<uint32_t>& indices);

/// Binary STL: 80-byte header, triangle count, then 50 bytes per facet.
/// Facet normals are computed from the triangles. Returns false on I/O failure
/// or if an index is out of range.
bool writeBinarySTL(const std::string& path, const std::vector<float>& vertices,
                    const std::vector<uint32_t>& indices);

/// Wavefront OBJ with positions and faces only (1-based indices, mm).
bool writeOBJ(const std::string& path, const std::vector<float>& vertices,
              const std::vector<uint32_t>& indices);

} // namespace kernel::trimesh
