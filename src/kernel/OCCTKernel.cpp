#include "OCCTKernel.h"

#include <cmath>
#include <unordered_set>

// OCCT includes
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <TopTools_ListOfShape.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <Poly_Triangulation.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <STEPControl_Writer.hxx>
#include <STEPControl_Reader.hxx>
#include <IGESControl_Reader.hxx>
#include <StlAPI_Writer.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <GCPnts_TangentialDeflection.hxx>
#include <TopoDS_Edge.hxx>
#include <BRepAlgoAPI_Splitter.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepOffset_MakeOffset.hxx>
#include <BRepOffset_Mode.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <gp_Pln.hxx>
#include <TopoDS_Compound.hxx>
#include <BRep_Builder.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <gp_GTrsf.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <Geom_BSplineCurve.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <gp_Circ.hxx>

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
    if (!fuse.IsDone())
        throw std::runtime_error("Boolean union failed");
    return fuse.Shape();
}

TopoDS_Shape OCCTKernel::booleanCut(const TopoDS_Shape& target, const TopoDS_Shape& tool)
{
    BRepAlgoAPI_Cut cut(target, tool);
    cut.Build();
    if (!cut.IsDone())
        throw std::runtime_error("Boolean cut failed");
    return cut.Shape();
}

TopoDS_Shape OCCTKernel::booleanIntersect(const TopoDS_Shape& a, const TopoDS_Shape& b)
{
    BRepAlgoAPI_Common common(a, b);
    common.Build();
    if (!common.IsDone()) return a;
    return common.Shape();
}

TopoDS_Shape OCCTKernel::combine(const TopoDS_Shape& target, const TopoDS_Shape& tool,
                                  int operation)
{
    switch (operation) {
    case 0:  return booleanUnion(target, tool);
    case 1:  return booleanCut(target, tool);
    case 2:  return booleanIntersect(target, tool);
    default: return booleanUnion(target, tool);
    }
}

TopoDS_Shape OCCTKernel::splitBody(const TopoDS_Shape& body, const TopoDS_Shape& splittingTool)
{
    BRepAlgoAPI_Splitter splitter;

    TopTools_ListOfShape arguments;
    arguments.Append(body);
    splitter.SetArguments(arguments);

    TopTools_ListOfShape tools;
    tools.Append(splittingTool);
    splitter.SetTools(tools);

    splitter.Build();
    if (!splitter.IsDone())
        return body;

    return splitter.Shape();
}

TopoDS_Shape OCCTKernel::offsetFaces(const TopoDS_Shape& shape,
                                      const std::vector<int>& faceIndices,
                                      double distance)
{
    // Strategy: use BRepOffsetAPI_MakeThickSolid with the selected faces.
    // MakeThickSolid removes the listed faces and offsets the remaining shell,
    // but we want the opposite: offset only the listed faces outward.
    //
    // Correct approach: collect all faces NOT in faceIndices as the "faces to remove"
    // for MakeThickSolid. This hollows the solid by removing those faces and
    // offsetting the remaining faces (= our selected faces) by 'distance'.
    //
    // Actually, the simplest robust approach for offsetting specific faces is to use
    // BRepOffset_MakeOffset directly. But the most straightforward OCCT API is
    // BRepOffsetAPI_MakeOffsetShape which offsets the entire shell uniformly, then
    // we'd need per-face control.
    //
    // Best approach: use BRepOffset_MakeOffset with per-face offsets.
    // However that API is complex. A practical approach:
    // For each selected face, use MakeThickSolid treating those faces as the
    // "open" faces offset outward by the distance.

    // Collect the selected faces by index
    TopTools_ListOfShape facesToOffset;
    int faceIdx = 0;
    TopExp_Explorer ex(shape, TopAbs_FACE);
    for (; ex.More(); ex.Next()) {
        for (int id : faceIndices) {
            if (id == faceIdx) {
                facesToOffset.Append(ex.Current());
                break;
            }
        }
        faceIdx++;
    }

    if (facesToOffset.IsEmpty())
        return shape;

    // BRepOffsetAPI_MakeThickSolid: removes the listed faces and offsets the
    // remaining faces by 'distance'. A positive distance offsets outward.
    BRepOffsetAPI_MakeThickSolid offsetter;
    offsetter.MakeThickSolidByJoin(shape, facesToOffset, distance, 1e-3);
    offsetter.Build();
    if (!offsetter.IsDone())
        return shape;

    return offsetter.Shape();
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
                                 const std::vector<int>& edgeIds,
                                 double radius)
{
    BRepFilletAPI_MakeFillet mk(shape);

    if (edgeIds.empty()) {
        // Fillet all edges
        TopExp_Explorer ex(shape, TopAbs_EDGE);
        for (; ex.More(); ex.Next())
            mk.Add(radius, TopoDS::Edge(ex.Current()));
    } else {
        // Fillet only the edges whose indices appear in edgeIds
        std::unordered_set<int> edgeSet(edgeIds.begin(), edgeIds.end());
        int edgeIndex = 0;
        TopExp_Explorer ex(shape, TopAbs_EDGE);
        for (; ex.More(); ex.Next()) {
            if (edgeSet.count(edgeIndex))
                mk.Add(radius, TopoDS::Edge(ex.Current()));
            edgeIndex++;
        }
    }

    mk.Build();
    if (!mk.IsDone()) return shape;
    return mk.Shape();
}

TopoDS_Shape OCCTKernel::chamfer(const TopoDS_Shape& shape,
                                  const std::vector<int>& edgeIds,
                                  double distance)
{
    BRepFilletAPI_MakeChamfer mk(shape);

    if (edgeIds.empty()) {
        // Chamfer all edges
        TopExp_Explorer ex(shape, TopAbs_EDGE);
        for (; ex.More(); ex.Next())
            mk.Add(distance, TopoDS::Edge(ex.Current()));
    } else {
        // Chamfer only the edges whose indices appear in edgeIds
        std::unordered_set<int> edgeSet(edgeIds.begin(), edgeIds.end());
        int edgeIndex = 0;
        TopExp_Explorer ex(shape, TopAbs_EDGE);
        for (; ex.More(); ex.Next()) {
            if (edgeSet.count(edgeIndex))
                mk.Add(distance, TopoDS::Edge(ex.Current()));
            edgeIndex++;
        }
    }

    mk.Build();
    if (!mk.IsDone()) return shape;
    return mk.Shape();
}

TopoDS_Shape OCCTKernel::sweep(const TopoDS_Shape& profile, const TopoDS_Shape& path)
{
    // Extract the wire from the path shape
    TopoDS_Wire pathWire;
    if (path.ShapeType() == TopAbs_WIRE) {
        pathWire = TopoDS::Wire(path);
    } else {
        // Try to find a wire inside the path shape
        TopExp_Explorer ex(path, TopAbs_WIRE);
        if (ex.More())
            pathWire = TopoDS::Wire(ex.Current());
        else
            return profile; // fallback: return profile unchanged
    }

    BRepOffsetAPI_MakePipe pipe(pathWire, profile);
    pipe.Build();
    if (!pipe.IsDone()) return profile;
    return pipe.Shape();
}

TopoDS_Shape OCCTKernel::loft(const std::vector<TopoDS_Shape>& sections, bool isClosed)
{
    if (sections.empty())
        return TopoDS_Shape();

    BRepOffsetAPI_ThruSections lofter(/*isSolid=*/Standard_True, /*ruled=*/Standard_False);
    lofter.SetSmoothing(Standard_True);

    for (const auto& section : sections) {
        if (section.ShapeType() == TopAbs_WIRE) {
            lofter.AddWire(TopoDS::Wire(section));
        } else {
            // Try to extract a wire from the section shape
            TopExp_Explorer ex(section, TopAbs_WIRE);
            if (ex.More())
                lofter.AddWire(TopoDS::Wire(ex.Current()));
        }
    }

    if (isClosed)
        lofter.SetSmoothing(Standard_True);

    lofter.Build();
    if (!lofter.IsDone()) return sections.front();
    return lofter.Shape();
}

TopoDS_Shape OCCTKernel::shell(const TopoDS_Shape& shape, double thickness,
                                const std::vector<int>& removedFaceIds)
{
    TopTools_ListOfShape facesToRemove;

    if (removedFaceIds.empty()) {
        // If no faces specified, remove the topmost face (highest Z centroid)
        double maxZ = -1e30;
        TopoDS_Face topFace;
        bool found = false;
        TopExp_Explorer ex(shape, TopAbs_FACE);
        for (; ex.More(); ex.Next()) {
            const TopoDS_Face& face = TopoDS::Face(ex.Current());
            // Compute approximate centroid via bounding box midpoint
            TopExp_Explorer vertEx(face, TopAbs_VERTEX);
            double sumZ = 0; int count = 0;
            for (; vertEx.More(); vertEx.Next()) {
                gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(vertEx.Current()));
                sumZ += p.Z();
                count++;
            }
            if (count > 0) {
                double avgZ = sumZ / count;
                if (avgZ > maxZ) {
                    maxZ = avgZ;
                    topFace = face;
                    found = true;
                }
            }
        }
        if (found)
            facesToRemove.Append(topFace);
    } else {
        // Select faces by index
        int faceIndex = 0;
        TopExp_Explorer ex(shape, TopAbs_FACE);
        for (; ex.More(); ex.Next()) {
            for (int id : removedFaceIds) {
                if (id == faceIndex) {
                    facesToRemove.Append(ex.Current());
                    break;
                }
            }
            faceIndex++;
        }
    }

    BRepOffsetAPI_MakeThickSolid hollower;
    hollower.MakeThickSolidByJoin(shape, facesToRemove, -thickness,
                                   1e-3 /*tolerance*/);
    hollower.Build();
    if (!hollower.IsDone()) return shape;
    return hollower.Shape();
}

TopoDS_Shape OCCTKernel::mirror(const TopoDS_Shape& shape,
                                double planeOx, double planeOy, double planeOz,
                                double planeNx, double planeNy, double planeNz)
{
    gp_Pnt origin(planeOx, planeOy, planeOz);
    gp_Dir normal(planeNx, planeNy, planeNz);
    gp_Ax2 mirrorPlane(origin, normal);

    gp_Trsf trsf;
    trsf.SetMirror(mirrorPlane);

    BRepBuilderAPI_Transform transformer(shape, trsf, /*copy=*/Standard_True);
    if (!transformer.IsDone()) return shape;

    TopoDS_Shape mirrored = transformer.Shape();

    // Fuse original + mirrored
    BRepAlgoAPI_Fuse fuse(shape, mirrored);
    fuse.Build();
    if (!fuse.IsDone()) return shape;
    return fuse.Shape();
}

TopoDS_Shape OCCTKernel::rectangularPattern(const TopoDS_Shape& shape,
                                             double dirX1, double dirY1, double dirZ1,
                                             double spacing1, int count1,
                                             double dirX2, double dirY2, double dirZ2,
                                             double spacing2, int count2)
{
    if (count1 < 1) count1 = 1;
    if (count2 < 1) count2 = 1;

    // Normalize direction vectors
    auto normalize = [](double& x, double& y, double& z) {
        double len = std::sqrt(x*x + y*y + z*z);
        if (len > 1e-12) { x /= len; y /= len; z /= len; }
    };
    normalize(dirX1, dirY1, dirZ1);
    normalize(dirX2, dirY2, dirZ2);

    TopoDS_Shape result = shape;

    for (int i = 0; i < count1; ++i) {
        for (int j = 0; j < count2; ++j) {
            if (i == 0 && j == 0) continue; // skip the original

            double tx = dirX1 * spacing1 * i + dirX2 * spacing2 * j;
            double ty = dirY1 * spacing1 * i + dirY2 * spacing2 * j;
            double tz = dirZ1 * spacing1 * i + dirZ2 * spacing2 * j;

            gp_Trsf trsf;
            trsf.SetTranslation(gp_Vec(tx, ty, tz));

            BRepBuilderAPI_Transform transformer(shape, trsf, /*copy=*/Standard_True);
            if (!transformer.IsDone()) continue;

            BRepAlgoAPI_Fuse fuse(result, transformer.Shape());
            fuse.Build();
            if (fuse.IsDone())
                result = fuse.Shape();
        }
    }

    return result;
}

TopoDS_Shape OCCTKernel::circularPattern(const TopoDS_Shape& shape,
                                          double axisOx, double axisOy, double axisOz,
                                          double axisDx, double axisDy, double axisDz,
                                          int count, double totalAngleDeg)
{
    if (count < 2) return shape;

    gp_Pnt axisOrigin(axisOx, axisOy, axisOz);
    gp_Dir axisDir(axisDx, axisDy, axisDz);
    gp_Ax1 axis(axisOrigin, axisDir);

    double stepAngleRad = (totalAngleDeg * M_PI / 180.0) / count;

    TopoDS_Shape result = shape;

    for (int i = 1; i < count; ++i) {
        gp_Trsf trsf;
        trsf.SetRotation(axis, stepAngleRad * i);

        BRepBuilderAPI_Transform transformer(shape, trsf, /*copy=*/Standard_True);
        if (!transformer.IsDone()) continue;

        BRepAlgoAPI_Fuse fuse(result, transformer.Shape());
        fuse.Build();
        if (fuse.IsDone())
            result = fuse.Shape();
    }

    return result;
}

// ── Move / Transform ops ─────────────────────────────────────────────────────

TopoDS_Shape OCCTKernel::transform(const TopoDS_Shape& shape, const double matrix[16])
{
    // Build a gp_Trsf from the 4x4 column-major matrix.
    // OCCT gp_Trsf supports affine transforms (rotation + translation + uniform scale).
    // We extract the 3x3 rotation/scale part and the translation vector.
    // Column-major layout: col0 = [m[0],m[1],m[2],m[3]], col1 = [m[4]..m[7]], etc.
    gp_Trsf trsf;
    trsf.SetValues(
        matrix[0], matrix[4], matrix[8],  matrix[12],
        matrix[1], matrix[5], matrix[9],  matrix[13],
        matrix[2], matrix[6], matrix[10], matrix[14]
    );

    BRepBuilderAPI_Transform transformer(shape, trsf, /*copy=*/Standard_True);
    if (!transformer.IsDone()) return shape;
    return transformer.Shape();
}

TopoDS_Shape OCCTKernel::translate(const TopoDS_Shape& shape, double dx, double dy, double dz)
{
    gp_Trsf trsf;
    trsf.SetTranslation(gp_Vec(dx, dy, dz));

    BRepBuilderAPI_Transform transformer(shape, trsf, /*copy=*/Standard_True);
    if (!transformer.IsDone()) return shape;
    return transformer.Shape();
}

TopoDS_Shape OCCTKernel::rotate(const TopoDS_Shape& shape,
                                double axisOx, double axisOy, double axisOz,
                                double axisDx, double axisDy, double axisDz,
                                double angleDeg)
{
    gp_Pnt axisOrigin(axisOx, axisOy, axisOz);
    gp_Dir axisDir(axisDx, axisDy, axisDz);
    gp_Ax1 axis(axisOrigin, axisDir);

    double angleRad = angleDeg * M_PI / 180.0;

    gp_Trsf trsf;
    trsf.SetRotation(axis, angleRad);

    BRepBuilderAPI_Transform transformer(shape, trsf, /*copy=*/Standard_True);
    if (!transformer.IsDone()) return shape;
    return transformer.Shape();
}

// ── Draft / Thicken ops ──────────────────────────────────────────────────────

TopoDS_Shape OCCTKernel::draft(const TopoDS_Shape& shape,
                               const std::vector<int>& faceIndices,
                               double angleDeg,
                               double pullDirX, double pullDirY, double pullDirZ)
{
    double angleRad = angleDeg * M_PI / 180.0;
    gp_Dir pullDir(pullDirX, pullDirY, pullDirZ);

    BRepOffsetAPI_DraftAngle drafter(shape);

    // Build a set of target face indices for O(1) lookup
    std::unordered_set<int> targetSet(faceIndices.begin(), faceIndices.end());

    int faceIndex = 0;
    TopExp_Explorer ex(shape, TopAbs_FACE);
    for (; ex.More(); ex.Next()) {
        if (targetSet.count(faceIndex)) {
            const TopoDS_Face& face = TopoDS::Face(ex.Current());
            drafter.Add(face, pullDir, angleRad, gp_Pln(gp_Pnt(0, 0, 0), pullDir));
        }
        faceIndex++;
    }

    drafter.Build();
    if (!drafter.IsDone()) return shape;
    return drafter.Shape();
}

TopoDS_Shape OCCTKernel::thicken(const TopoDS_Shape& shape, double thickness)
{
    // BRepOffset_MakeOffset offsets all faces of a shape (surface or shell)
    // to create a solid.
    BRepOffset_MakeOffset offsetter;
    offsetter.Initialize(shape, thickness, 1e-3 /*tolerance*/,
                         BRepOffset_Skin, /*intersection=*/Standard_False,
                         /*selfInter=*/Standard_False);
    offsetter.MakeOffsetShape();
    if (!offsetter.IsDone()) return shape;
    return offsetter.Shape();
}

TopoDS_Shape OCCTKernel::hole(const TopoDS_Shape& shape,
                              double posX, double posY, double posZ,
                              double dirX, double dirY, double dirZ,
                              double diameter, double depth)
{
    double radius = diameter / 2.0;
    // Through-all: use a large depth to guarantee full penetration
    double cutDepth = (depth <= 0.0) ? 1000.0 : depth;

    gp_Pnt origin(posX, posY, posZ);
    gp_Dir direction(dirX, dirY, dirZ);
    gp_Ax2 axis(origin, direction);

    BRepPrimAPI_MakeCylinder mkCyl(axis, radius, cutDepth);
    mkCyl.Build();
    if (!mkCyl.IsDone()) return shape;

    BRepAlgoAPI_Cut cut(shape, mkCyl.Shape());
    cut.Build();
    if (!cut.IsDone()) return shape;
    return cut.Shape();
}

TopoDS_Shape OCCTKernel::counterboreHole(const TopoDS_Shape& shape,
                                          double posX, double posY, double posZ,
                                          double dirX, double dirY, double dirZ,
                                          double diameter, double depth,
                                          double cboreDiameter, double cboreDepth)
{
    // First cut the main (smaller) hole
    TopoDS_Shape result = hole(shape, posX, posY, posZ,
                               dirX, dirY, dirZ,
                               diameter, depth);

    // Then cut the counterbore (larger, shallow cylinder) at the same position
    double cboreRadius = cboreDiameter / 2.0;
    gp_Pnt origin(posX, posY, posZ);
    gp_Dir direction(dirX, dirY, dirZ);
    gp_Ax2 axis(origin, direction);

    BRepPrimAPI_MakeCylinder mkCbore(axis, cboreRadius, cboreDepth);
    mkCbore.Build();
    if (!mkCbore.IsDone()) return result;

    BRepAlgoAPI_Cut cut(result, mkCbore.Shape());
    cut.Build();
    if (!cut.IsDone()) return result;
    return cut.Shape();
}

TopoDS_Shape OCCTKernel::countersinkHole(const TopoDS_Shape& shape,
                                          double posX, double posY, double posZ,
                                          double dirX, double dirY, double dirZ,
                                          double diameter, double depth,
                                          double csinkDiameter, double csinkAngleDeg)
{
    // First cut the main (smaller) hole
    TopoDS_Shape result = hole(shape, posX, posY, posZ,
                               dirX, dirY, dirZ,
                               diameter, depth);

    // Then cut the countersink cone at the same position.
    // The cone has its large end at the surface (csinkDiameter) and
    // tapers down to the hole diameter. The cone height is derived from
    // the half-angle and the radii difference.
    double halfAngleRad = (csinkAngleDeg / 2.0) * M_PI / 180.0;
    double csinkRadius = csinkDiameter / 2.0;
    double holeRadius  = diameter / 2.0;

    // Cone height from the geometry: h = (R - r) / tan(halfAngle)
    double tanHalf = std::tan(halfAngleRad);
    if (tanHalf < 1e-12) return result; // degenerate angle
    double coneHeight = (csinkRadius - holeRadius) / tanHalf;

    gp_Pnt origin(posX, posY, posZ);
    gp_Dir direction(dirX, dirY, dirZ);
    gp_Ax2 axis(origin, direction);

    // BRepPrimAPI_MakeCone(axis, R1_at_base, R2_at_top, height)
    // The base of the cone is at the surface (large radius), the top is
    // at coneHeight depth (smaller radius = holeRadius).
    BRepPrimAPI_MakeCone mkCone(axis, csinkRadius, holeRadius, coneHeight);
    mkCone.Build();
    if (!mkCone.IsDone()) return result;

    BRepAlgoAPI_Cut cut(result, mkCone.Shape());
    cut.Build();
    if (!cut.IsDone()) return result;
    return cut.Shape();
}

// ── Thread ops ────────────────────────────────────────────────────────────────

TopoDS_Shape OCCTKernel::thread(const TopoDS_Shape& shape,
                                int cylindricalFaceIndex,
                                double pitch,
                                double depth,
                                bool isInternal,
                                bool isRightHanded,
                                bool isModeled)
{
    // Cosmetic threads: just return the shape unchanged (metadata-only in the UI)
    if (!isModeled)
        return shape;

    // Find the cylindrical face
    TopoDS_Face cylFace;
    bool found = false;
    int faceIdx = 0;
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        const TopoDS_Face& face = TopoDS::Face(ex.Current());
        BRepAdaptor_Surface adaptor(face);

        if (adaptor.GetType() == GeomAbs_Cylinder) {
            if (cylindricalFaceIndex < 0 || cylindricalFaceIndex == faceIdx) {
                cylFace = face;
                found = true;
                break;
            }
        }
        faceIdx++;
    }

    if (!found)
        return shape; // No cylindrical face found; return unchanged

    // Extract cylinder geometry
    BRepAdaptor_Surface adaptor(cylFace);
    gp_Cylinder cylinder = adaptor.Cylinder();
    gp_Ax3 cylAx3 = cylinder.Position();
    gp_Pnt cylOrigin = cylAx3.Location();
    gp_Dir cylDir = cylAx3.Direction();
    double cylRadius = cylinder.Radius();

    // Determine thread radius: for internal threads, cut inward; for external, add outward
    double threadRadius = isInternal ? (cylRadius - depth) : (cylRadius + depth);
    // The profile center radius: mid-point between cylRadius and threadRadius
    double profileRadius = (cylRadius + threadRadius) / 2.0;
    double profileHalfDepth = std::abs(depth) / 2.0;

    // Determine the height of the cylinder face to know how many turns we need
    double vMin = adaptor.FirstVParameter();
    double vMax = adaptor.LastVParameter();
    double cylHeight = vMax - vMin;
    if (cylHeight < 1e-6)
        cylHeight = 10.0; // fallback

    // Number of turns
    int numTurns = static_cast<int>(std::ceil(cylHeight / pitch));
    if (numTurns < 1) numTurns = 1;

    // Build a helix as a polyline approximation.
    // We sample the helix at multiple points per turn.
    int pointsPerTurn = 36;
    int totalPoints = numTurns * pointsPerTurn + 1;

    // Local coordinate system of the cylinder
    gp_Dir cylX = cylAx3.XDirection();
    gp_Dir cylY = cylAx3.YDirection();

    TColgp_Array1OfPnt helixPts(1, totalPoints);
    for (int i = 0; i < totalPoints; ++i) {
        double t = static_cast<double>(i) / pointsPerTurn; // turns
        double angle = t * 2.0 * M_PI;
        if (!isRightHanded)
            angle = -angle;
        double z = t * pitch + vMin; // height along cylinder axis

        // Point on the helix at the profile center radius
        gp_Pnt pt = cylOrigin.Translated(
            gp_Vec(cylX) * (profileRadius * std::cos(angle)) +
            gp_Vec(cylY) * (profileRadius * std::sin(angle)) +
            gp_Vec(cylDir) * z
        );
        helixPts.SetValue(i + 1, pt);
    }

    // Build a polyline wire along the helix points
    BRepBuilderAPI_MakeWire wireMaker;
    for (int i = 1; i < totalPoints; ++i) {
        BRepBuilderAPI_MakeEdge edgeMaker(helixPts.Value(i), helixPts.Value(i + 1));
        if (edgeMaker.IsDone())
            wireMaker.Add(edgeMaker.Edge());
    }
    if (!wireMaker.IsDone())
        return shape;

    TopoDS_Wire helixWire = wireMaker.Wire();

    // Build a small triangular thread profile perpendicular to the helix at the start.
    gp_Pnt startPt = helixPts.Value(1);
    gp_Vec tangent(helixPts.Value(1), helixPts.Value(2));
    if (tangent.Magnitude() < 1e-12)
        return shape;
    tangent.Normalize();

    // Radial direction at start point (from cylinder axis to the point on helix)
    gp_Vec radial = gp_Vec(cylOrigin, startPt);
    // Remove the axial component to get pure radial
    radial = radial - gp_Vec(cylDir) * radial.Dot(gp_Vec(cylDir));
    if (radial.Magnitude() < 1e-12)
        return shape;
    radial.Normalize();

    // Bitangent: perpendicular to tangent and radial
    gp_Vec bitangent = gp_Vec(tangent).Crossed(radial);
    if (bitangent.Magnitude() < 1e-12)
        return shape;
    bitangent.Normalize();

    // Triangle vertices: a V-shaped thread profile
    // - tip: at profileRadius + profileHalfDepth (outward) in radial direction
    // - base left/right: at profileRadius - profileHalfDepth, offset along bitangent
    double halfPitch = pitch * 0.4; // slightly less than half pitch for the base width

    gp_Pnt tipPt   = startPt.Translated(radial * profileHalfDepth);
    gp_Pnt basePt1 = startPt.Translated(radial * (-profileHalfDepth) + bitangent * halfPitch);
    gp_Pnt basePt2 = startPt.Translated(radial * (-profileHalfDepth) + bitangent * (-halfPitch));

    // Build triangular profile wire
    BRepBuilderAPI_MakeEdge e1(tipPt, basePt1);
    BRepBuilderAPI_MakeEdge e2(basePt1, basePt2);
    BRepBuilderAPI_MakeEdge e3(basePt2, tipPt);
    if (!e1.IsDone() || !e2.IsDone() || !e3.IsDone())
        return shape;

    BRepBuilderAPI_MakeWire profileWireMaker;
    profileWireMaker.Add(e1.Edge());
    profileWireMaker.Add(e2.Edge());
    profileWireMaker.Add(e3.Edge());
    if (!profileWireMaker.IsDone())
        return shape;

    TopoDS_Wire profileWire = profileWireMaker.Wire();

    // Sweep the profile along the helix
    BRepOffsetAPI_MakePipeShell pipeMaker(helixWire);
    pipeMaker.Add(profileWire);
    pipeMaker.Build();
    if (!pipeMaker.IsDone())
        return shape;

    // Make it a solid
    pipeMaker.MakeSolid();
    TopoDS_Shape threadSolid = pipeMaker.Shape();

    // Boolean operation: cut for internal threads, fuse for external threads
    if (isInternal) {
        BRepAlgoAPI_Cut cut(shape, threadSolid);
        cut.Build();
        if (!cut.IsDone()) return shape;
        return cut.Shape();
    } else {
        BRepAlgoAPI_Fuse fuse(shape, threadSolid);
        fuse.Build();
        if (!fuse.IsDone()) return shape;
        return fuse.Shape();
    }
}

// ── Scale ops ─────────────────────────────────────────────────────────────────

TopoDS_Shape OCCTKernel::scaleUniform(const TopoDS_Shape& shape, double factor,
                                       double centerX, double centerY, double centerZ)
{
    gp_Trsf trsf;
    trsf.SetScale(gp_Pnt(centerX, centerY, centerZ), factor);

    BRepBuilderAPI_Transform transformer(shape, trsf, /*copy=*/Standard_True);
    if (!transformer.IsDone()) return shape;
    return transformer.Shape();
}

TopoDS_Shape OCCTKernel::scaleNonUniform(const TopoDS_Shape& shape,
                                          double factorX, double factorY, double factorZ,
                                          double centerX, double centerY, double centerZ)
{
    // For non-uniform scale we need gp_GTrsf (general transformation).
    // Strategy: translate to origin, apply scale matrix, translate back.
    gp_GTrsf gtrsf;

    // Set the diagonal scale values (row, col) -- gp_GTrsf uses 1-based indexing.
    gtrsf.SetValue(1, 1, factorX);
    gtrsf.SetValue(2, 2, factorY);
    gtrsf.SetValue(3, 3, factorZ);

    // Incorporate the center offset: T = translate(center) * Scale * translate(-center)
    // The translation part: new_translation = center - Scale * center
    //   tx = centerX - factorX * centerX = centerX * (1 - factorX)
    gtrsf.SetValue(1, 4, centerX * (1.0 - factorX));
    gtrsf.SetValue(2, 4, centerY * (1.0 - factorY));
    gtrsf.SetValue(3, 4, centerZ * (1.0 - factorZ));

    BRepBuilderAPI_GTransform transformer(shape, gtrsf, /*copy=*/Standard_True);
    if (!transformer.IsDone()) return shape;
    return transformer.Shape();
}

std::vector<TopoDS_Shape> OCCTKernel::importSTEP(const std::string& path)
{
    STEPControl_Reader reader;
    IFSelect_ReturnStatus status = reader.ReadFile(path.c_str());
    if (status != IFSelect_RetDone)
        throw std::runtime_error("Failed to read STEP file: " + path);

    reader.TransferRoots();

    std::vector<TopoDS_Shape> results;
    for (int i = 1; i <= reader.NbShapes(); ++i)
        results.push_back(reader.Shape(i));

    if (results.empty())
        throw std::runtime_error("STEP file contains no geometry: " + path);

    return results;
}

std::vector<TopoDS_Shape> OCCTKernel::importIGES(const std::string& path)
{
    IGESControl_Reader reader;
    IFSelect_ReturnStatus status = reader.ReadFile(path.c_str());
    if (status != IFSelect_RetDone)
        throw std::runtime_error("Failed to read IGES file: " + path);

    reader.TransferRoots();

    std::vector<TopoDS_Shape> results;
    for (int i = 1; i <= reader.NbShapes(); ++i)
        results.push_back(reader.Shape(i));

    if (results.empty())
        throw std::runtime_error("IGES file contains no geometry: " + path);

    return results;
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

        const gp_Trsf trsf = loc.Transformation();
        const bool hasNormals = tri->HasNormals();

        for (int i = 1; i <= tri->NbNodes(); ++i) {
            gp_Pnt p = tri->Node(i).Transformed(trsf);
            result.vertices.push_back(static_cast<float>(p.X()));
            result.vertices.push_back(static_cast<float>(p.Y()));
            result.vertices.push_back(static_cast<float>(p.Z()));

            if (hasNormals) {
                gp_Dir n = tri->Normal(i);
                n = n.Transformed(trsf);
                if (reversed) n.Reverse();
                result.normals.push_back(static_cast<float>(n.X()));
                result.normals.push_back(static_cast<float>(n.Y()));
                result.normals.push_back(static_cast<float>(n.Z()));
            } else {
                // Will be filled by face-normal accumulation below
                result.normals.push_back(0);
                result.normals.push_back(0);
                result.normals.push_back(0);
            }
        }

        for (int i = 1; i <= tri->NbTriangles(); ++i) {
            int n1, n2, n3;
            tri->Triangle(i).Get(n1, n2, n3);
            if (reversed) std::swap(n2, n3);
            result.indices.push_back(offset + n1 - 1);
            result.indices.push_back(offset + n2 - 1);
            result.indices.push_back(offset + n3 - 1);

            if (!hasNormals) {
                // Accumulate area-weighted face normals for smooth shading
                uint32_t i0 = offset + n1 - 1;
                uint32_t i1 = offset + n2 - 1;
                uint32_t i2 = offset + n3 - 1;
                gp_Vec v0(result.vertices[i0*3], result.vertices[i0*3+1], result.vertices[i0*3+2]);
                gp_Vec v1(result.vertices[i1*3], result.vertices[i1*3+1], result.vertices[i1*3+2]);
                gp_Vec v2(result.vertices[i2*3], result.vertices[i2*3+1], result.vertices[i2*3+2]);
                gp_Vec faceN = (v1 - v0).Crossed(v2 - v0);
                // faceN magnitude is proportional to triangle area — good weighting
                for (uint32_t vi : {i0, i1, i2}) {
                    result.normals[vi*3]   += static_cast<float>(faceN.X());
                    result.normals[vi*3+1] += static_cast<float>(faceN.Y());
                    result.normals[vi*3+2] += static_cast<float>(faceN.Z());
                }
            }
        }
    }

    // Normalize any accumulated face normals (fallback path)
    for (size_t i = 0; i + 2 < result.normals.size(); i += 3) {
        float nx = result.normals[i], ny = result.normals[i+1], nz = result.normals[i+2];
        float len = std::sqrt(nx*nx + ny*ny + nz*nz);
        if (len > 1e-10f) {
            result.normals[i]   = nx / len;
            result.normals[i+1] = ny / len;
            result.normals[i+2] = nz / len;
        } else {
            result.normals[i] = 0; result.normals[i+1] = 0; result.normals[i+2] = 1;
        }
    }

    return result;
}

OCCTKernel::EdgeMesh OCCTKernel::extractEdges(const TopoDS_Shape& shape, double deflection)
{
    EdgeMesh result;

    TopExp_Explorer edgeEx(shape, TopAbs_EDGE);
    for (; edgeEx.More(); edgeEx.Next()) {
        const TopoDS_Edge& edge = TopoDS::Edge(edgeEx.Current());

        // Skip degenerated edges (seam edges on periodic surfaces)
        if (BRep_Tool::Degenerated(edge))
            continue;

        BRepAdaptor_Curve curve(edge);
        GCPnts_TangentialDeflection discretizer(curve, deflection, 0.1 /*angular deflection*/);

        int nbPoints = discretizer.NbPoints();
        if (nbPoints < 2)
            continue;

        uint32_t baseIndex = static_cast<uint32_t>(result.vertices.size() / 3);

        for (int i = 1; i <= nbPoints; ++i) {
            gp_Pnt p = discretizer.Value(i);
            result.vertices.push_back(static_cast<float>(p.X()));
            result.vertices.push_back(static_cast<float>(p.Y()));
            result.vertices.push_back(static_cast<float>(p.Z()));
        }

        // Emit line segments connecting consecutive polyline points
        for (int i = 0; i < nbPoints - 1; ++i) {
            result.indices.push_back(baseIndex + static_cast<uint32_t>(i));
            result.indices.push_back(baseIndex + static_cast<uint32_t>(i + 1));
        }
    }

    return result;
}

// ── Topology queries ─────────────────────────────────────────────────────────

int OCCTKernel::faceCount(const TopoDS_Shape& shape)
{
    int count = 0;
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next())
        ++count;
    return count;
}

std::vector<float> OCCTKernel::faceCentroids(const TopoDS_Shape& shape)
{
    std::vector<float> centroids;

    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        const TopoDS_Face& face = TopoDS::Face(ex.Current());

        // Compute centroid by averaging all vertices of the face
        double sumX = 0, sumY = 0, sumZ = 0;
        int vertCount = 0;
        for (TopExp_Explorer vertEx(face, TopAbs_VERTEX); vertEx.More(); vertEx.Next()) {
            gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(vertEx.Current()));
            sumX += p.X();
            sumY += p.Y();
            sumZ += p.Z();
            ++vertCount;
        }

        if (vertCount > 0) {
            centroids.push_back(static_cast<float>(sumX / vertCount));
            centroids.push_back(static_cast<float>(sumY / vertCount));
            centroids.push_back(static_cast<float>(sumZ / vertCount));
        } else {
            // Degenerate face — push origin as fallback
            centroids.push_back(0.0f);
            centroids.push_back(0.0f);
            centroids.push_back(0.0f);
        }
    }

    return centroids;
}

int OCCTKernel::edgeCount(const TopoDS_Shape& shape)
{
    int count = 0;
    for (TopExp_Explorer ex(shape, TopAbs_EDGE); ex.More(); ex.Next())
        ++count;
    return count;
}

// =============================================================================
// Physical Properties via BRepGProp
// =============================================================================
// Interference detection
// =============================================================================

std::vector<OCCTKernel::InterferenceResult> OCCTKernel::checkInterference(
    const std::vector<std::pair<std::string, TopoDS_Shape>>& bodies)
{
    std::vector<InterferenceResult> results;

    for (size_t i = 0; i < bodies.size(); ++i) {
        for (size_t j = i + 1; j < bodies.size(); ++j) {
            try {
                BRepAlgoAPI_Common common(bodies[i].second, bodies[j].second);
                common.Build();
                if (!common.IsDone())
                    continue;

                const TopoDS_Shape& commonShape = common.Shape();
                GProp_GProps gprops;
                BRepGProp::VolumeProperties(commonShape, gprops);
                double vol = gprops.Mass();

                if (vol > 1e-6) {
                    InterferenceResult ir;
                    ir.body1Id = bodies[i].first;
                    ir.body2Id = bodies[j].first;
                    ir.volume = vol;
                    ir.interferenceShape = commonShape;
                    results.push_back(std::move(ir));
                }
            } catch (...) {
                // Skip pairs that fail boolean common
            }
        }
    }

    return results;
}

// =============================================================================

OCCTKernel::PhysicalProperties OCCTKernel::computeProperties(
    const TopoDS_Shape& shape, double density)
{
    PhysicalProperties props;

    // Volume properties
    GProp_GProps volumeProps;
    BRepGProp::VolumeProperties(shape, volumeProps);
    props.volume = volumeProps.Mass();  // GProp "mass" is actually volume for VolumeProperties
    props.mass = props.volume * density;

    // Center of gravity from volume properties
    gp_Pnt cog = volumeProps.CentreOfMass();
    props.cogX = cog.X();
    props.cogY = cog.Y();
    props.cogZ = cog.Z();

    // Moments of inertia
    gp_Mat inertia = volumeProps.MatrixOfInertia();
    props.ixx = inertia(1, 1);
    props.iyy = inertia(2, 2);
    props.izz = inertia(3, 3);
    props.ixy = inertia(1, 2);
    props.ixz = inertia(1, 3);
    props.iyz = inertia(2, 3);

    // Surface area
    GProp_GProps surfaceProps;
    BRepGProp::SurfaceProperties(shape, surfaceProps);
    props.surfaceArea = surfaceProps.Mass();  // GProp "mass" is area for SurfaceProperties

    // Bounding box
    Bnd_Box bbox;
    BRepBndLib::Add(shape, bbox);
    if (!bbox.IsVoid()) {
        bbox.Get(props.bboxMinX, props.bboxMinY, props.bboxMinZ,
                 props.bboxMaxX, props.bboxMaxY, props.bboxMaxZ);
    }

    return props;
}

// ── Path Pattern ────────────────────────────────────────────────────────────

TopoDS_Shape OCCTKernel::pathPattern(const TopoDS_Shape& shape,
                                      const TopoDS_Shape& pathWire,
                                      int count, double startOffset,
                                      double endOffset)
{
    if (count < 1)
        throw std::runtime_error("pathPattern: count must be >= 1");

    // Extract the first edge/wire from the path shape to get a curve
    TopoDS_Wire wire;
    {
        TopExp_Explorer expW(pathWire, TopAbs_WIRE);
        if (expW.More()) {
            wire = TopoDS::Wire(expW.Current());
        } else {
            // Try to build a wire from edges
            TopExp_Explorer expE(pathWire, TopAbs_EDGE);
            if (expE.More()) {
                BRepBuilderAPI_MakeWire wm;
                for (; expE.More(); expE.Next())
                    wm.Add(TopoDS::Edge(expE.Current()));
                wire = wm.Wire();
            } else {
                throw std::runtime_error("pathPattern: path contains no edges or wires");
            }
        }
    }

    // Compute total length using BRepAdaptor_CompCurve (approximated via edges)
    // We'll iterate edges and use GCPnts_UniformAbscissa for uniform spacing
    // Collect all edges from the wire
    std::vector<TopoDS_Edge> edges;
    for (TopExp_Explorer exp(wire, TopAbs_EDGE); exp.More(); exp.Next())
        edges.push_back(TopoDS::Edge(exp.Current()));

    if (edges.empty())
        throw std::runtime_error("pathPattern: no edges in path wire");

    // Compute total length
    double totalLength = 0;
    for (const auto& edge : edges) {
        BRepAdaptor_Curve curve(edge);
        GCPnts_TangentialDeflection sampler(curve, 0.01, 0.1);
        // Use GProp for accurate length
        GProp_GProps props;
        BRepGProp::LinearProperties(edge, props);
        totalLength += props.Mass();
    }

    if (totalLength < 1e-10)
        throw std::runtime_error("pathPattern: path has zero length");

    // For uniform placement, compute distances along the path
    double startDist = startOffset * totalLength;
    double endDist = endOffset * totalLength;
    double range = endDist - startDist;

    if (count == 1) {
        // Single copy at startDist
    }

    // Build a compound of all copies fused together
    TopoDS_Shape result = shape;

    // Walk along edges to find points at given distances
    auto pointAndTangentAtDist = [&](double targetDist) -> std::pair<gp_Pnt, gp_Dir> {
        double accumulated = 0;
        for (const auto& edge : edges) {
            GProp_GProps eprops;
            BRepGProp::LinearProperties(edge, eprops);
            double edgeLen = eprops.Mass();

            if (accumulated + edgeLen >= targetDist || &edge == &edges.back()) {
                double localDist = targetDist - accumulated;
                if (localDist < 0) localDist = 0;
                if (localDist > edgeLen) localDist = edgeLen;

                BRepAdaptor_Curve curve(edge);
                double u0 = curve.FirstParameter();
                double u1 = curve.LastParameter();

                // Find parameter at localDist using uniform abscissa
                // Simple linear interpolation by arc length ratio
                double ratio = (edgeLen > 1e-10) ? localDist / edgeLen : 0.0;
                double u = u0 + ratio * (u1 - u0);

                gp_Pnt pt;
                gp_Vec tan;
                curve.D1(u, pt, tan);

                if (tan.Magnitude() < 1e-10)
                    tan = gp_Vec(0, 0, 1);

                return { pt, gp_Dir(tan) };
            }
            accumulated += edgeLen;
        }
        // Fallback: return end of last edge
        BRepAdaptor_Curve curve(edges.back());
        gp_Pnt pt;
        gp_Vec tan;
        curve.D1(curve.LastParameter(), pt, tan);
        if (tan.Magnitude() < 1e-10) tan = gp_Vec(0, 0, 1);
        return { pt, gp_Dir(tan) };
    };

    for (int i = 0; i < count; ++i) {
        double dist;
        if (count == 1)
            dist = startDist;
        else
            dist = startDist + range * static_cast<double>(i) / (count - 1);

        auto [pt, tangent] = pointAndTangentAtDist(dist);

        // Build a transform that places the shape at pt with Z aligned to tangent
        gp_Ax3 targetAx(pt, tangent);
        gp_Ax3 originAx(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));

        gp_Trsf trsf;
        trsf.SetTransformation(targetAx, originAx);

        BRepBuilderAPI_Transform xform(shape, trsf, true /*copy*/);
        if (!xform.IsDone())
            continue;

        TopoDS_Shape copy = xform.Shape();

        if (i == 0) {
            result = copy;
        } else {
            BRepAlgoAPI_Fuse fuser(result, copy);
            if (fuser.IsDone())
                result = fuser.Shape();
        }
    }

    return result;
}

// ── Coil (Helical Sweep) ────────────────────────────────────────────────────

TopoDS_Shape OCCTKernel::coil(const TopoDS_Shape& profileShape,
                               double axisOx, double axisOy, double axisOz,
                               double axisDx, double axisDy, double axisDz,
                               double radius, double pitch, double turns,
                               double taperAngleDeg)
{
    if (turns <= 0)
        throw std::runtime_error("coil: turns must be > 0");
    if (radius <= 0)
        throw std::runtime_error("coil: radius must be > 0");
    if (pitch <= 0)
        throw std::runtime_error("coil: pitch must be > 0");

    gp_Pnt origin(axisOx, axisOy, axisOz);
    gp_Dir axisDir(axisDx, axisDy, axisDz);

    // Build a helix as a polyline approximation
    // ~36 segments per turn for smooth appearance
    const int segmentsPerTurn = 36;
    int totalSegments = static_cast<int>(std::ceil(turns * segmentsPerTurn));
    if (totalSegments < 4) totalSegments = 4;

    double totalAngle = turns * 2.0 * M_PI;
    double totalHeight = turns * pitch;
    double taperRad = taperAngleDeg * M_PI / 180.0;

    // Build a local coordinate system: axis = Z, X and Y perpendicular
    gp_Ax2 localAx(origin, axisDir);
    gp_Dir xDir = localAx.XDirection();
    gp_Dir yDir = localAx.YDirection();

    BRepBuilderAPI_MakeWire helixWireBuilder;
    gp_Pnt prevPt;
    for (int i = 0; i <= totalSegments; ++i) {
        double t = static_cast<double>(i) / totalSegments;
        double angle = t * totalAngle;
        double h = t * totalHeight;
        double r = radius + h * std::tan(taperRad);

        gp_Pnt pt = origin.Translated(gp_Vec(axisDir) * h);
        pt.Translate(gp_Vec(xDir) * (r * std::cos(angle)));
        pt.Translate(gp_Vec(yDir) * (r * std::sin(angle)));

        if (i > 0) {
            if (prevPt.Distance(pt) > 1e-6) {
                TopoDS_Edge seg = BRepBuilderAPI_MakeEdge(prevPt, pt);
                helixWireBuilder.Add(seg);
            }
        }
        prevPt = pt;
    }

    if (!helixWireBuilder.IsDone())
        throw std::runtime_error("coil: failed to build helix wire");

    TopoDS_Wire helixWire = helixWireBuilder.Wire();

    // Extract profile as wire, or create a default circular cross-section
    TopoDS_Wire profileWire;
    if (profileShape.IsNull()) {
        // Default: circle of radius = pitch/4 at the start of the helix
        double profileRadius = pitch / 4.0;
        gp_Pnt startPt = origin.Translated(gp_Vec(xDir) * radius);
        // Profile in a plane perpendicular to the helix tangent at start
        // Tangent at start is roughly along yDir (perpendicular to xDir in the base plane)
        gp_Dir profileNormal = axisDir; // approximate: profile normal along helix axis
        gp_Ax2 profileAx(startPt, profileNormal);
        gp_Circ profileCirc(profileAx, profileRadius);
        TopoDS_Edge circEdge = BRepBuilderAPI_MakeEdge(profileCirc);
        profileWire = BRepBuilderAPI_MakeWire(circEdge);
    } else {
        TopExp_Explorer expW(profileShape, TopAbs_WIRE);
        if (expW.More()) {
            profileWire = TopoDS::Wire(expW.Current());
        } else {
            TopExp_Explorer expE(profileShape, TopAbs_EDGE);
            if (expE.More()) {
                BRepBuilderAPI_MakeWire wm;
                for (; expE.More(); expE.Next())
                    wm.Add(TopoDS::Edge(expE.Current()));
                profileWire = wm.Wire();
            } else {
                throw std::runtime_error("coil: profile contains no edges or wires");
            }
        }
    }

    // Use MakePipeShell for the sweep
    BRepOffsetAPI_MakePipeShell pipeShell(helixWire);
    pipeShell.Add(profileWire);
    pipeShell.Build();

    if (!pipeShell.IsDone())
        throw std::runtime_error("coil: MakePipeShell failed");

    pipeShell.MakeSolid();
    return pipeShell.Shape();
}

// ── Delete Faces ────────────────────────────────────────────────────────────

TopoDS_Shape OCCTKernel::deleteFaces(const TopoDS_Shape& shape,
                                      const std::vector<int>& faceIndices)
{
    if (faceIndices.empty())
        return shape;

    // Collect the faces to remove
    TopTools_ListOfShape facesToRemove;
    int idx = 0;
    std::unordered_set<int> removeSet(faceIndices.begin(), faceIndices.end());
    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next(), ++idx) {
        if (removeSet.count(idx))
            facesToRemove.Append(exp.Current());
    }

    if (facesToRemove.IsEmpty())
        throw std::runtime_error("deleteFaces: no matching faces found");

    // Use MakeThickSolid with zero offset on the selected faces to remove them.
    // This effectively removes the face and heals the gap.
    BRepOffsetAPI_MakeThickSolid thickSolid;
    thickSolid.MakeThickSolidByJoin(shape, facesToRemove, 0.0,
                                     1e-3 /*tolerance*/);
    thickSolid.Build();

    if (!thickSolid.IsDone())
        throw std::runtime_error("deleteFaces: MakeThickSolid failed");

    return thickSolid.Shape();
}

// ── Replace Face ────────────────────────────────────────────────────────────

TopoDS_Shape OCCTKernel::replaceFace(const TopoDS_Shape& shape, int faceIndex,
                                      const TopoDS_Shape& newFace)
{
    // Find the target face by index
    TopoDS_Face oldFace;
    int idx = 0;
    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next(), ++idx) {
        if (idx == faceIndex) {
            oldFace = TopoDS::Face(exp.Current());
            break;
        }
    }

    if (oldFace.IsNull())
        throw std::runtime_error("replaceFace: face index out of range");

    // Extract the first face from newFace shape
    TopoDS_Face replacementFace;
    for (TopExp_Explorer exp(newFace, TopAbs_FACE); exp.More(); exp.Next()) {
        replacementFace = TopoDS::Face(exp.Current());
        break;
    }

    if (replacementFace.IsNull())
        throw std::runtime_error("replaceFace: replacement shape contains no faces");

    // Rebuild the shape, substituting the old face with the new one
    BRep_Builder bbuilder;
    TopoDS_Compound compound;
    bbuilder.MakeCompound(compound);

    // Rebuild as a shell with the replacement face
    TopoDS_Shell newShell;
    bbuilder.MakeShell(newShell);
    idx = 0;
    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next(), ++idx) {
        if (idx == faceIndex)
            bbuilder.Add(newShell, replacementFace);
        else
            bbuilder.Add(newShell, exp.Current());
    }

    // Try to make a solid from the shell
    TopoDS_Solid solid;
    bbuilder.MakeSolid(solid);
    bbuilder.Add(solid, newShell);

    return solid;
}

// ── Reverse Normals ─────────────────────────────────────────────────────────

TopoDS_Shape OCCTKernel::reverseNormals(const TopoDS_Shape& shape,
                                          const std::vector<int>& faceIndices)
{
    if (faceIndices.empty()) {
        // Reverse the entire shape orientation
        TopoDS_Shape reversed = shape.Reversed();
        return reversed;
    }

    // Reverse only specified faces: rebuild the shape with those faces reversed
    std::unordered_set<int> reverseSet(faceIndices.begin(), faceIndices.end());

    BRep_Builder bbuilder;
    TopoDS_Shell newShell;
    bbuilder.MakeShell(newShell);

    int idx = 0;
    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next(), ++idx) {
        if (reverseSet.count(idx)) {
            TopoDS_Shape reversed = exp.Current().Reversed();
            bbuilder.Add(newShell, reversed);
        } else {
            bbuilder.Add(newShell, exp.Current());
        }
    }

    // Rebuild solid from shell
    TopoDS_Solid solid;
    bbuilder.MakeSolid(solid);
    bbuilder.Add(solid, newShell);

    return solid;
}

} // namespace kernel
