#include "MeshBoolean.h"
#include "OCCTKernel.h"

#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <Poly_Triangulation.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <stdexcept>

namespace kernel {

TopoDS_Shape MeshBoolean::meshToShape(const TriMesh& mesh)
{
    if (mesh.vertices.empty() || mesh.indices.empty())
        throw std::runtime_error("MeshBoolean::meshToShape: empty mesh");

    BRepBuilderAPI_Sewing sewing(1e-3);

    // Create a face for each triangle and add to the sewing algorithm
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        uint32_t i0 = mesh.indices[i];
        uint32_t i1 = mesh.indices[i + 1];
        uint32_t i2 = mesh.indices[i + 2];

        gp_Pnt p0(mesh.vertices[i0 * 3],     mesh.vertices[i0 * 3 + 1], mesh.vertices[i0 * 3 + 2]);
        gp_Pnt p1(mesh.vertices[i1 * 3],     mesh.vertices[i1 * 3 + 1], mesh.vertices[i1 * 3 + 2]);
        gp_Pnt p2(mesh.vertices[i2 * 3],     mesh.vertices[i2 * 3 + 1], mesh.vertices[i2 * 3 + 2]);

        // Skip degenerate triangles
        if (p0.Distance(p1) < 1e-7 || p1.Distance(p2) < 1e-7 || p0.Distance(p2) < 1e-7)
            continue;

        // Build a triangular face from 3 edges
        TopoDS_Edge e1 = BRepBuilderAPI_MakeEdge(p0, p1);
        TopoDS_Edge e2 = BRepBuilderAPI_MakeEdge(p1, p2);
        TopoDS_Edge e3 = BRepBuilderAPI_MakeEdge(p2, p0);

        BRepBuilderAPI_MakeWire wireMaker;
        wireMaker.Add(e1);
        wireMaker.Add(e2);
        wireMaker.Add(e3);
        if (!wireMaker.IsDone())
            continue;

        BRepBuilderAPI_MakeFace faceMaker(wireMaker.Wire());
        if (faceMaker.IsDone())
            sewing.Add(faceMaker.Face());
    }

    sewing.Perform();
    TopoDS_Shape sewn = sewing.SewedShape();

    // Try to convert the sewn shell(s) into a solid
    try {
        BRepBuilderAPI_MakeSolid solidMaker;
        TopExp_Explorer shellEx(sewn, TopAbs_SHELL);
        bool hasShell = false;
        for (; shellEx.More(); shellEx.Next()) {
            solidMaker.Add(TopoDS::Shell(shellEx.Current()));
            hasShell = true;
        }
        if (hasShell && solidMaker.IsDone())
            return TopoDS_Shape(solidMaker.Solid());
    } catch (...) {
        // Fall through to return the shell
    }

    return sewn;  // return as shell if solid conversion fails
}

MeshBoolean::TriMesh MeshBoolean::shapeToMesh(const TopoDS_Shape& shape, double deflection)
{
    BRepMesh_IncrementalMesh mesher(shape, deflection);
    mesher.Perform();

    TriMesh result;

    TopExp_Explorer faceEx(shape, TopAbs_FACE);
    for (; faceEx.More(); faceEx.Next()) {
        const TopoDS_Face& face = TopoDS::Face(faceEx.Current());
        TopLoc_Location loc;
        auto tri = BRep_Tool::Triangulation(face, loc);
        if (tri.IsNull())
            continue;

        const bool reversed = (face.Orientation() == TopAbs_REVERSED);
        const uint32_t offset = static_cast<uint32_t>(result.vertices.size() / 3);
        const gp_Trsf trsf = loc.Transformation();

        for (int i = 1; i <= tri->NbNodes(); ++i) {
            gp_Pnt p = tri->Node(i).Transformed(trsf);
            result.vertices.push_back(static_cast<float>(p.X()));
            result.vertices.push_back(static_cast<float>(p.Y()));
            result.vertices.push_back(static_cast<float>(p.Z()));
        }

        for (int i = 1; i <= tri->NbTriangles(); ++i) {
            int n1, n2, n3;
            tri->Triangle(i).Get(n1, n2, n3);
            if (reversed)
                std::swap(n2, n3);
            result.indices.push_back(offset + n1 - 1);
            result.indices.push_back(offset + n2 - 1);
            result.indices.push_back(offset + n3 - 1);
        }
    }

    return result;
}

MeshBoolean::TriMesh MeshBoolean::meshUnion(const TriMesh& a, const TriMesh& b)
{
    TopoDS_Shape shapeA = meshToShape(a);
    TopoDS_Shape shapeB = meshToShape(b);

    OCCTKernel kernel;
    TopoDS_Shape result = kernel.booleanUnion(shapeA, shapeB);

    return shapeToMesh(result);
}

MeshBoolean::TriMesh MeshBoolean::meshDifference(const TriMesh& a, const TriMesh& b)
{
    TopoDS_Shape shapeA = meshToShape(a);
    TopoDS_Shape shapeB = meshToShape(b);

    OCCTKernel kernel;
    TopoDS_Shape result = kernel.booleanCut(shapeA, shapeB);

    return shapeToMesh(result);
}

MeshBoolean::TriMesh MeshBoolean::meshIntersection(const TriMesh& a, const TriMesh& b)
{
    TopoDS_Shape shapeA = meshToShape(a);
    TopoDS_Shape shapeB = meshToShape(b);

    OCCTKernel kernel;
    TopoDS_Shape result = kernel.booleanIntersect(shapeA, shapeB);

    return shapeToMesh(result);
}

} // namespace kernel
