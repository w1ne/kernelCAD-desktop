#pragma once
#include <string>
#include <vector>

// Forward declare OCCT types to keep include cost out of headers
class TopoDS_Shape;

namespace kernel {

/// Thin C++ wrapper around OCCT operations.
/// All geometry lives here — nothing else in the app touches OCCT directly.
class OCCTKernel
{
public:
    OCCTKernel();
    ~OCCTKernel();

    // ── Primitives ────────────────────────────────────────────────────────
    TopoDS_Shape makeBox(double dx, double dy, double dz);
    TopoDS_Shape makeCylinder(double radius, double height);
    TopoDS_Shape makeSphere(double radius);

    // ── Boolean ops ───────────────────────────────────────────────────────
    TopoDS_Shape booleanUnion(const TopoDS_Shape& a, const TopoDS_Shape& b);
    TopoDS_Shape booleanCut(const TopoDS_Shape& target, const TopoDS_Shape& tool);
    TopoDS_Shape booleanIntersect(const TopoDS_Shape& a, const TopoDS_Shape& b);

    // ── Feature ops ───────────────────────────────────────────────────────
    TopoDS_Shape extrude(const TopoDS_Shape& profile, double distance);
    TopoDS_Shape revolve(const TopoDS_Shape& profile, double angleDeg);
    TopoDS_Shape fillet(const TopoDS_Shape& shape,
                        const std::vector<int>& edgeIds,
                        double radius);

    // ── Export ────────────────────────────────────────────────────────────
    bool exportSTEP(const TopoDS_Shape& shape, const std::string& path);
    bool exportSTL(const TopoDS_Shape& shape, const std::string& path,
                   double linDeflection = 0.1);

    // ── Tessellation ─────────────────────────────────────────────────────
    struct Mesh {
        std::vector<float>    vertices;   // x,y,z triples
        std::vector<float>    normals;    // x,y,z triples
        std::vector<uint32_t> indices;
    };
    Mesh tessellate(const TopoDS_Shape& shape, double linDeflection = 0.1);

private:
    struct Impl;
    Impl* m_impl = nullptr;
};

} // namespace kernel
