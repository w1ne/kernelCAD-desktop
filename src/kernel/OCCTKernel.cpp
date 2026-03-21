#include "OCCTKernel.h"

// OCCT includes
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <Poly_Triangulation.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <STEPControl_Writer.hxx>
#include <StlAPI_Writer.hxx>

namespace kernel {

struct OCCTKernel::Impl {};

OCCTKernel::OCCTKernel() : m_impl(new Impl) {}
OCCTKernel::~OCCTKernel() { delete m_impl; }

TopoDS_Shape OCCTKernel::makeBox(double dx, double dy, double dz)
{
    return BRepPrimAPI_MakeBox(dx, dy, dz).Shape();
}

TopoDS_Shape OCCTKernel::makeCylinder(double radius, double height)
{
    return BRepPrimAPI_MakeCylinder(radius, height).Shape();
}

TopoDS_Shape OCCTKernel::makeSphere(double radius)
{
    return BRepPrimAPI_MakeSphere(radius).Shape();
}

TopoDS_Shape OCCTKernel::booleanUnion(const TopoDS_Shape& a, const TopoDS_Shape& b)
{
    BRepAlgoAPI_Fuse fuse(a, b);
    fuse.Build();
    if (!fuse.IsDone()) return a; // TODO: propagate error
    return fuse.Shape();
}

TopoDS_Shape OCCTKernel::booleanCut(const TopoDS_Shape& target, const TopoDS_Shape& tool)
{
    BRepAlgoAPI_Cut cut(target, tool);
    cut.Build();
    if (!cut.IsDone()) return target;
    return cut.Shape();
}

TopoDS_Shape OCCTKernel::booleanIntersect(const TopoDS_Shape& a, const TopoDS_Shape& b)
{
    BRepAlgoAPI_Common common(a, b);
    common.Build();
    if (!common.IsDone()) return a;
    return common.Shape();
}

TopoDS_Shape OCCTKernel::extrude(const TopoDS_Shape& profile, double distance)
{
    gp_Vec direction(0, 0, distance);
    return BRepPrimAPI_MakePrism(profile, direction).Shape();
}

TopoDS_Shape OCCTKernel::revolve(const TopoDS_Shape& profile, double angleDeg)
{
    gp_Ax1 axis(gp_Pnt(0,0,0), gp_Dir(0,0,1));
    double angleRad = angleDeg * M_PI / 180.0;
    return BRepPrimAPI_MakeRevol(profile, axis, angleRad).Shape();
}

TopoDS_Shape OCCTKernel::fillet(const TopoDS_Shape& shape,
                                 const std::vector<int>& /*edgeIds*/,
                                 double radius)
{
    BRepFilletAPI_MakeFillet mk(shape);
    // TODO: select edges by edgeIds using tempId lookup
    // For now fillet all edges
    TopExp_Explorer ex(shape, TopAbs_EDGE);
    for (; ex.More(); ex.Next())
        mk.Add(radius, TopoDS::Edge(ex.Current()));
    mk.Build();
    if (!mk.IsDone()) return shape;
    return mk.Shape();
}

bool OCCTKernel::exportSTEP(const TopoDS_Shape& shape, const std::string& path)
{
    STEPControl_Writer writer;
    writer.Transfer(shape, STEPControl_AsIs);
    return writer.Write(path.c_str()) == IFSelect_RetDone;
}

bool OCCTKernel::exportSTL(const TopoDS_Shape& shape, const std::string& path,
                             double linDeflection)
{
    BRepMesh_IncrementalMesh mesh(shape, linDeflection);
    mesh.Perform();
    StlAPI_Writer writer;
    return writer.Write(shape, path.c_str());
}

OCCTKernel::Mesh OCCTKernel::tessellate(const TopoDS_Shape& shape, double linDeflection)
{
    BRepMesh_IncrementalMesh mesher(shape, linDeflection);
    mesher.Perform();

    Mesh result;
    TopExp_Explorer faceEx(shape, TopAbs_FACE);
    for (; faceEx.More(); faceEx.Next()) {
        const TopoDS_Face& face = TopoDS::Face(faceEx.Current());
        TopLoc_Location loc;
        auto tri = BRep_Tool::Triangulation(face, loc);
        if (tri.IsNull()) continue;

        const bool reversed = (face.Orientation() == TopAbs_REVERSED);
        const uint32_t offset = static_cast<uint32_t>(result.vertices.size() / 3);

        for (int i = 1; i <= tri->NbNodes(); ++i) {
            gp_Pnt p = tri->Node(i).Transformed(loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.Transformation());
            result.vertices.push_back(static_cast<float>(p.X()));
            result.vertices.push_back(static_cast<float>(p.Y()));
            result.vertices.push_back(static_cast<float>(p.Z()));
            // normals computed per-triangle below
            result.normals.push_back(0); result.normals.push_back(0); result.normals.push_back(1);
        }

        for (int i = 1; i <= tri->NbTriangles(); ++i) {
            int n1, n2, n3;
            tri->Triangle(i).Get(n1, n2, n3);
            if (reversed) std::swap(n2, n3);
            result.indices.push_back(offset + n1 - 1);
            result.indices.push_back(offset + n2 - 1);
            result.indices.push_back(offset + n3 - 1);
        }
    }
    return result;
}

} // namespace kernel
