#include "BRepQuery.h"

#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <BRep_Tool.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepGProp.hxx>
#include <BRepGProp_Face.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <GProp_GProps.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GeomAbs_CurveType.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Dir.hxx>

#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace kernel {

// ---------------------------------------------------------------------------
// Helpers — enum conversion
// ---------------------------------------------------------------------------

static SurfaceType mapSurfaceType(GeomAbs_SurfaceType t)
{
    switch (t) {
    case GeomAbs_Plane:      return SurfaceType::Plane;
    case GeomAbs_Cylinder:   return SurfaceType::Cylinder;
    case GeomAbs_Cone:       return SurfaceType::Cone;
    case GeomAbs_Sphere:     return SurfaceType::Sphere;
    case GeomAbs_Torus:      return SurfaceType::Torus;
    case GeomAbs_BSplineSurface: return SurfaceType::BSpline;
    default:                 return SurfaceType::Other;
    }
}

static CurveType mapCurveType(GeomAbs_CurveType t)
{
    switch (t) {
    case GeomAbs_Line:       return CurveType::Line;
    case GeomAbs_Circle:     return CurveType::Circle;
    case GeomAbs_Ellipse:    return CurveType::Ellipse;
    case GeomAbs_BSplineCurve: return CurveType::BSpline;
    default:                 return CurveType::Other;
    }
}

const char* surfaceTypeName(SurfaceType t)
{
    switch (t) {
    case SurfaceType::Plane:    return "Plane";
    case SurfaceType::Cylinder: return "Cylinder";
    case SurfaceType::Cone:     return "Cone";
    case SurfaceType::Sphere:   return "Sphere";
    case SurfaceType::Torus:    return "Torus";
    case SurfaceType::BSpline:  return "B-Spline";
    case SurfaceType::Other:    return "Other";
    }
    return "Unknown";
}

const char* curveTypeName(CurveType t)
{
    switch (t) {
    case CurveType::Line:    return "Line";
    case CurveType::Circle:  return "Circle";
    case CurveType::Ellipse: return "Ellipse";
    case CurveType::BSpline: return "B-Spline";
    case CurveType::Other:   return "Other";
    }
    return "Unknown";
}

const char* edgeConvexityName(EdgeConvexity c)
{
    switch (c) {
    case EdgeConvexity::Convex:  return "Convex";
    case EdgeConvexity::Concave: return "Concave";
    case EdgeConvexity::Tangent: return "Tangent";
    case EdgeConvexity::Unknown: return "Unknown";
    }
    return "Unknown";
}

// ---------------------------------------------------------------------------
// BRepQuery implementation
// ---------------------------------------------------------------------------

BRepQuery::BRepQuery(const TopoDS_Shape& shape)
    : m_shape(shape)
{
}

// --- Face queries ---

int BRepQuery::faceCount() const
{
    TopTools_IndexedMapOfShape map;
    TopExp::MapShapes(m_shape, TopAbs_FACE, map);
    return map.Extent();
}

FaceInfo BRepQuery::faceInfo(int index) const
{
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(m_shape, TopAbs_FACE, faceMap);
    if (index < 0 || index >= faceMap.Extent())
        throw std::out_of_range("BRepQuery::faceInfo: index " + std::to_string(index) +
                                " out of range [0, " + std::to_string(faceMap.Extent()) + ")");

    const TopoDS_Face& face = TopoDS::Face(faceMap(index + 1)); // 1-based in OCCT

    FaceInfo fi{};
    fi.index = index;
    fi.isReversed = (face.Orientation() == TopAbs_REVERSED);

    // Surface type
    BRepAdaptor_Surface adaptor(face, /*BasisSurface=*/false);
    fi.surfaceType = mapSurfaceType(adaptor.GetType());

    // Area
    GProp_GProps gprops;
    BRepGProp::SurfaceProperties(face, gprops);
    fi.area = gprops.Mass();

    // Centroid (center of mass of the surface)
    gp_Pnt cog = gprops.CentreOfMass();
    fi.centroidX = cog.X();
    fi.centroidY = cog.Y();
    fi.centroidZ = cog.Z();

    // Normal at centroid — project centroid onto surface parametric domain
    // Use BRepGProp_Face to evaluate at mid-parameter as a robust fallback
    {
        double uMin = adaptor.FirstUParameter();
        double uMax = adaptor.LastUParameter();
        double vMin = adaptor.FirstVParameter();
        double vMax = adaptor.LastVParameter();
        double uMid = 0.5 * (uMin + uMax);
        double vMid = 0.5 * (vMin + vMax);

        gp_Pnt pnt;
        gp_Vec dU, dV;
        adaptor.D1(uMid, vMid, pnt, dU, dV);
        gp_Vec normal = dU.Crossed(dV);
        double mag = normal.Magnitude();
        if (mag > 1e-12) {
            normal.Divide(mag);
            if (fi.isReversed) normal.Reverse();
            fi.normalX = normal.X();
            fi.normalY = normal.Y();
            fi.normalZ = normal.Z();
        } else {
            fi.normalX = 0;
            fi.normalY = 0;
            fi.normalZ = 1;
        }
    }

    // Edge count on this face
    {
        TopTools_IndexedMapOfShape edgeMap;
        TopExp::MapShapes(face, TopAbs_EDGE, edgeMap);
        fi.edgeCount = edgeMap.Extent();
    }

    // Loop (wire) count
    {
        int wires = 0;
        for (TopExp_Explorer ex(face, TopAbs_WIRE); ex.More(); ex.Next())
            ++wires;
        fi.loopCount = wires;
    }

    return fi;
}

std::vector<FaceInfo> BRepQuery::allFaces() const
{
    int n = faceCount();
    std::vector<FaceInfo> result;
    result.reserve(n);
    for (int i = 0; i < n; ++i)
        result.push_back(faceInfo(i));
    return result;
}

std::vector<int> BRepQuery::facesOfType(SurfaceType type) const
{
    std::vector<int> result;
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(m_shape, TopAbs_FACE, faceMap);
    for (int i = 1; i <= faceMap.Extent(); ++i) {
        const TopoDS_Face& face = TopoDS::Face(faceMap(i));
        BRepAdaptor_Surface adaptor(face, false);
        if (mapSurfaceType(adaptor.GetType()) == type)
            result.push_back(i - 1);
    }
    return result;
}

std::vector<int> BRepQuery::adjacentFaces(int faceIndex) const
{
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(m_shape, TopAbs_FACE, faceMap);
    if (faceIndex < 0 || faceIndex >= faceMap.Extent())
        return {};

    // Build edge -> ancestor faces map
    TopTools_IndexedDataMapOfShapeListOfShape edgeFaceMap;
    TopExp::MapShapesAndAncestors(m_shape, TopAbs_EDGE, TopAbs_FACE, edgeFaceMap);

    const TopoDS_Face& face = TopoDS::Face(faceMap(faceIndex + 1));

    // Collect all faces sharing an edge with this face
    TopTools_IndexedMapOfShape edgesOfFace;
    TopExp::MapShapes(face, TopAbs_EDGE, edgesOfFace);

    std::vector<int> result;
    for (int e = 1; e <= edgesOfFace.Extent(); ++e) {
        const TopoDS_Shape& edge = edgesOfFace(e);
        int edgeIdx = edgeFaceMap.FindIndex(edge);
        if (edgeIdx == 0) continue;
        const TopTools_ListOfShape& ancestors = edgeFaceMap(edgeIdx);
        for (auto it = ancestors.begin(); it != ancestors.end(); ++it) {
            int fi = faceMap.FindIndex(*it) - 1;
            if (fi >= 0 && fi != faceIndex) {
                // Avoid duplicates
                if (std::find(result.begin(), result.end(), fi) == result.end())
                    result.push_back(fi);
            }
        }
    }
    return result;
}

std::vector<int> BRepQuery::tangentiallyConnectedFaces(int faceIndex) const
{
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(m_shape, TopAbs_FACE, faceMap);
    if (faceIndex < 0 || faceIndex >= faceMap.Extent())
        return {};

    TopTools_IndexedDataMapOfShapeListOfShape edgeFaceMap;
    TopExp::MapShapesAndAncestors(m_shape, TopAbs_EDGE, TopAbs_FACE, edgeFaceMap);

    TopTools_IndexedMapOfShape edgeMap;
    TopExp::MapShapes(m_shape, TopAbs_EDGE, edgeMap);

    // BFS from faceIndex, only crossing tangent edges
    std::vector<bool> visited(faceMap.Extent(), false);
    visited[faceIndex] = true;
    std::vector<int> queue = { faceIndex };
    std::vector<int> result;

    for (size_t qi = 0; qi < queue.size(); ++qi) {
        int curFace = queue[qi];
        const TopoDS_Face& f = TopoDS::Face(faceMap(curFace + 1));

        TopTools_IndexedMapOfShape edgesOfFace;
        TopExp::MapShapes(f, TopAbs_EDGE, edgesOfFace);

        for (int e = 1; e <= edgesOfFace.Extent(); ++e) {
            const TopoDS_Shape& edge = edgesOfFace(e);
            int eIdx = edgeFaceMap.FindIndex(edge);
            if (eIdx == 0) continue;

            const TopTools_ListOfShape& ancestors = edgeFaceMap(eIdx);
            if (ancestors.Size() != 2) continue;

            // Get both faces
            const TopoDS_Face& f1 = TopoDS::Face(ancestors.First());
            const TopoDS_Face& f2 = TopoDS::Face(ancestors.Last());
            int fi1 = faceMap.FindIndex(f1) - 1;
            int fi2 = faceMap.FindIndex(f2) - 1;
            int neighbor = (fi1 == curFace) ? fi2 : fi1;

            if (neighbor < 0 || visited[neighbor]) continue;

            // Compute dihedral angle at edge midpoint
            const TopoDS_Edge& topoEdge = TopoDS::Edge(edge);
            if (BRep_Tool::Degenerated(topoEdge)) continue;

            BRepAdaptor_Curve curve(topoEdge);
            double midParam = 0.5 * (curve.FirstParameter() + curve.LastParameter());
            gp_Pnt midPt = curve.Value(midParam);

            // Evaluate surface normals at the edge midpoint
            auto evalNormal = [&](const TopoDS_Face& face) -> gp_Vec {
                BRepAdaptor_Surface surf(face, false);
                double uMin = surf.FirstUParameter(), uMax = surf.LastUParameter();
                double vMin = surf.FirstVParameter(), vMax = surf.LastVParameter();

                // Project midPt onto the surface parametric space (use midpoint as approx)
                double u = 0.5 * (uMin + uMax);
                double v = 0.5 * (vMin + vMax);

                gp_Pnt pnt;
                gp_Vec dU, dV;
                surf.D1(u, v, pnt, dU, dV);
                gp_Vec n = dU.Crossed(dV);
                if (face.Orientation() == TopAbs_REVERSED) n.Reverse();
                double mag = n.Magnitude();
                if (mag > 1e-12) n.Divide(mag);
                return n;
            };

            gp_Vec n1 = evalNormal(f1);
            gp_Vec n2 = evalNormal(f2);

            double dot = n1.Dot(n2);
            // Tangent if normals are nearly parallel (dot ~ 1.0)
            static constexpr double tangentThreshold = 0.9998; // ~1 degree
            if (dot > tangentThreshold) {
                visited[neighbor] = true;
                queue.push_back(neighbor);
                result.push_back(neighbor);
            }
        }
    }
    return result;
}

// --- Edge queries ---

int BRepQuery::edgeCount() const
{
    TopTools_IndexedMapOfShape map;
    TopExp::MapShapes(m_shape, TopAbs_EDGE, map);
    return map.Extent();
}

EdgeInfo BRepQuery::edgeInfo(int index) const
{
    TopTools_IndexedMapOfShape edgeMap;
    TopExp::MapShapes(m_shape, TopAbs_EDGE, edgeMap);
    if (index < 0 || index >= edgeMap.Extent())
        throw std::out_of_range("BRepQuery::edgeInfo: index " + std::to_string(index) +
                                " out of range [0, " + std::to_string(edgeMap.Extent()) + ")");

    const TopoDS_Edge& edge = TopoDS::Edge(edgeMap(index + 1));

    EdgeInfo ei{};
    ei.index = index;
    ei.isDegenerate = BRep_Tool::Degenerated(edge);
    ei.isSeam = false;
    ei.adjacentFace1 = -1;
    ei.adjacentFace2 = -1;
    ei.convexity = EdgeConvexity::Unknown;

    if (ei.isDegenerate) {
        ei.curveType = CurveType::Other;
        ei.length = 0;
        // Still get vertex position
        TopoDS_Vertex v1, v2;
        TopExp::Vertices(edge, v1, v2);
        if (!v1.IsNull()) {
            gp_Pnt p = BRep_Tool::Pnt(v1);
            ei.startX = p.X(); ei.startY = p.Y(); ei.startZ = p.Z();
            ei.endX = p.X(); ei.endY = p.Y(); ei.endZ = p.Z();
        }
        return ei;
    }

    // Curve type
    BRepAdaptor_Curve adaptor(edge);
    ei.curveType = mapCurveType(adaptor.GetType());

    // Length
    ei.length = GCPnts_AbscissaPoint::Length(adaptor);

    // Endpoints
    {
        TopoDS_Vertex v1, v2;
        TopExp::Vertices(edge, v1, v2);
        if (!v1.IsNull()) {
            gp_Pnt p = BRep_Tool::Pnt(v1);
            ei.startX = p.X(); ei.startY = p.Y(); ei.startZ = p.Z();
        }
        if (!v2.IsNull()) {
            gp_Pnt p = BRep_Tool::Pnt(v2);
            ei.endX = p.X(); ei.endY = p.Y(); ei.endZ = p.Z();
        }
    }

    // Adjacent faces and convexity
    {
        TopTools_IndexedDataMapOfShapeListOfShape edgeFaceMap;
        TopExp::MapShapesAndAncestors(m_shape, TopAbs_EDGE, TopAbs_FACE, edgeFaceMap);

        TopTools_IndexedMapOfShape faceMap;
        TopExp::MapShapes(m_shape, TopAbs_FACE, faceMap);

        int eIdx = edgeFaceMap.FindIndex(edge);
        if (eIdx > 0) {
            const TopTools_ListOfShape& ancestors = edgeFaceMap(eIdx);
            auto it = ancestors.begin();
            if (it != ancestors.end()) {
                ei.adjacentFace1 = faceMap.FindIndex(*it) - 1;
                const TopoDS_Face& f1 = TopoDS::Face(*it);
                ++it;
                if (it != ancestors.end()) {
                    ei.adjacentFace2 = faceMap.FindIndex(*it) - 1;
                    const TopoDS_Face& f2 = TopoDS::Face(*it);

                    // Check if seam edge (same face on both sides)
                    if (f1.IsSame(f2))
                        ei.isSeam = true;

                    // Compute convexity from dihedral angle at edge midpoint
                    double midParam = 0.5 * (adaptor.FirstParameter() + adaptor.LastParameter());
                    gp_Pnt midPt = adaptor.Value(midParam);
                    gp_Vec tangent;
                    gp_Pnt dummy;
                    adaptor.D1(midParam, dummy, tangent);
                    if (tangent.Magnitude() < 1e-12) {
                        ei.convexity = EdgeConvexity::Unknown;
                    } else {
                        tangent.Normalize();

                        auto evalNormal = [&](const TopoDS_Face& face) -> gp_Vec {
                            BRepAdaptor_Surface surf(face, false);
                            double uMid = 0.5 * (surf.FirstUParameter() + surf.LastUParameter());
                            double vMid = 0.5 * (surf.FirstVParameter() + surf.LastVParameter());
                            gp_Pnt p;
                            gp_Vec dU, dV;
                            surf.D1(uMid, vMid, p, dU, dV);
                            gp_Vec n = dU.Crossed(dV);
                            if (face.Orientation() == TopAbs_REVERSED) n.Reverse();
                            double mag = n.Magnitude();
                            if (mag > 1e-12) n.Divide(mag);
                            return n;
                        };

                        gp_Vec n1 = evalNormal(f1);
                        gp_Vec n2 = evalNormal(f2);

                        double dot = n1.Dot(n2);
                        if (dot > 0.9998) {
                            ei.convexity = EdgeConvexity::Tangent;
                        } else {
                            // Cross product of normals dotted with tangent gives sign
                            gp_Vec cross = n1.Crossed(n2);
                            double sign = cross.Dot(tangent);
                            // Positive sign -> concave, negative -> convex
                            // (this depends on orientation convention; use the
                            //  sum-of-normals direction vs midpoint offset heuristic)
                            gp_Vec nSum = n1 + n2;
                            // If the sum points "outward" relative to the edge,
                            // the edge is convex
                            // Alternative: use the dihedral angle directly
                            double angle = n1.Angle(n2);
                            if (std::abs(angle) < 0.01) {
                                ei.convexity = EdgeConvexity::Tangent;
                            } else {
                                // The cross product sign determines concavity
                                // When cross aligns with tangent, material is on the
                                // "inside" of the angle -> concave
                                if (sign > 0)
                                    ei.convexity = EdgeConvexity::Concave;
                                else
                                    ei.convexity = EdgeConvexity::Convex;
                            }
                        }
                    }
                }
            }
        }
    }

    return ei;
}

std::vector<EdgeInfo> BRepQuery::allEdges() const
{
    int n = edgeCount();
    std::vector<EdgeInfo> result;
    result.reserve(n);
    for (int i = 0; i < n; ++i)
        result.push_back(edgeInfo(i));
    return result;
}

std::vector<int> BRepQuery::edgesOfType(CurveType type) const
{
    std::vector<int> result;
    TopTools_IndexedMapOfShape edgeMap;
    TopExp::MapShapes(m_shape, TopAbs_EDGE, edgeMap);
    for (int i = 1; i <= edgeMap.Extent(); ++i) {
        const TopoDS_Edge& edge = TopoDS::Edge(edgeMap(i));
        if (BRep_Tool::Degenerated(edge)) {
            if (type == CurveType::Other) result.push_back(i - 1);
            continue;
        }
        BRepAdaptor_Curve adaptor(edge);
        if (mapCurveType(adaptor.GetType()) == type)
            result.push_back(i - 1);
    }
    return result;
}

std::vector<int> BRepQuery::edgesOfFace(int faceIndex) const
{
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(m_shape, TopAbs_FACE, faceMap);
    if (faceIndex < 0 || faceIndex >= faceMap.Extent())
        return {};

    TopTools_IndexedMapOfShape globalEdgeMap;
    TopExp::MapShapes(m_shape, TopAbs_EDGE, globalEdgeMap);

    const TopoDS_Face& face = TopoDS::Face(faceMap(faceIndex + 1));
    TopTools_IndexedMapOfShape faceEdges;
    TopExp::MapShapes(face, TopAbs_EDGE, faceEdges);

    std::vector<int> result;
    for (int e = 1; e <= faceEdges.Extent(); ++e) {
        int globalIdx = globalEdgeMap.FindIndex(faceEdges(e));
        if (globalIdx > 0)
            result.push_back(globalIdx - 1);
    }
    return result;
}

std::vector<int> BRepQuery::concaveEdges() const
{
    std::vector<int> result;
    int n = edgeCount();
    for (int i = 0; i < n; ++i) {
        EdgeInfo ei = edgeInfo(i);
        if (ei.convexity == EdgeConvexity::Concave)
            result.push_back(i);
    }
    return result;
}

std::vector<int> BRepQuery::convexEdges() const
{
    std::vector<int> result;
    int n = edgeCount();
    for (int i = 0; i < n; ++i) {
        EdgeInfo ei = edgeInfo(i);
        if (ei.convexity == EdgeConvexity::Convex)
            result.push_back(i);
    }
    return result;
}

// --- Vertex queries ---

int BRepQuery::vertexCount() const
{
    TopTools_IndexedMapOfShape map;
    TopExp::MapShapes(m_shape, TopAbs_VERTEX, map);
    return map.Extent();
}

VertexInfo BRepQuery::vertexInfo(int index) const
{
    TopTools_IndexedMapOfShape vertMap;
    TopExp::MapShapes(m_shape, TopAbs_VERTEX, vertMap);
    if (index < 0 || index >= vertMap.Extent())
        throw std::out_of_range("BRepQuery::vertexInfo: index " + std::to_string(index) +
                                " out of range [0, " + std::to_string(vertMap.Extent()) + ")");

    const TopoDS_Vertex& vertex = TopoDS::Vertex(vertMap(index + 1));

    VertexInfo vi{};
    vi.index = index;

    gp_Pnt p = BRep_Tool::Pnt(vertex);
    vi.x = p.X();
    vi.y = p.Y();
    vi.z = p.Z();

    // Count edges meeting at this vertex
    TopTools_IndexedDataMapOfShapeListOfShape vertEdgeMap;
    TopExp::MapShapesAndAncestors(m_shape, TopAbs_VERTEX, TopAbs_EDGE, vertEdgeMap);
    int vIdx = vertEdgeMap.FindIndex(vertex);
    if (vIdx > 0)
        vi.edgeCount = vertEdgeMap(vIdx).Size();
    else
        vi.edgeCount = 0;

    return vi;
}

// --- Body queries ---

bool BRepQuery::pointContainment(double x, double y, double z) const
{
    BRepClass3d_SolidClassifier classifier(m_shape);
    classifier.Perform(gp_Pnt(x, y, z), 1e-6);
    return classifier.State() == TopAbs_IN;
}

} // namespace kernel
