#pragma once
#include <string>
#include <vector>

class TopoDS_Shape;

namespace kernel {

enum class SurfaceType { Plane, Cylinder, Cone, Sphere, Torus, BSpline, Other };
enum class CurveType { Line, Circle, Ellipse, BSpline, Other };
enum class EdgeConvexity { Convex, Concave, Tangent, Unknown };

struct FaceInfo {
    int index;
    SurfaceType surfaceType;
    double area;
    double centroidX, centroidY, centroidZ;
    double normalX, normalY, normalZ;  // at centroid
    int edgeCount;
    int loopCount;
    bool isReversed;
};

struct EdgeInfo {
    int index;
    CurveType curveType;
    double length;
    double startX, startY, startZ;
    double endX, endY, endZ;
    EdgeConvexity convexity;
    int adjacentFace1;  // face index, -1 if boundary
    int adjacentFace2;
    bool isDegenerate;
    bool isSeam;
};

struct VertexInfo {
    int index;
    double x, y, z;
    int edgeCount;  // number of edges meeting at this vertex
};

/// Query B-Rep topology of an OCCT shape.
class BRepQuery {
public:
    explicit BRepQuery(const TopoDS_Shape& shape);

    // -- Face queries --
    int faceCount() const;
    FaceInfo faceInfo(int index) const;
    std::vector<FaceInfo> allFaces() const;
    std::vector<int> facesOfType(SurfaceType type) const;
    std::vector<int> adjacentFaces(int faceIndex) const;
    std::vector<int> tangentiallyConnectedFaces(int faceIndex) const;

    // -- Edge queries --
    int edgeCount() const;
    EdgeInfo edgeInfo(int index) const;
    std::vector<EdgeInfo> allEdges() const;
    std::vector<int> edgesOfType(CurveType type) const;
    std::vector<int> edgesOfFace(int faceIndex) const;
    std::vector<int> concaveEdges() const;
    std::vector<int> convexEdges() const;

    // -- Vertex queries --
    int vertexCount() const;
    VertexInfo vertexInfo(int index) const;

    // -- Body queries --
    bool pointContainment(double x, double y, double z) const;  // inside/outside

private:
    const TopoDS_Shape& m_shape;
};

/// Convert enum values to human-readable strings.
const char* surfaceTypeName(SurfaceType t);
const char* curveTypeName(CurveType t);
const char* edgeConvexityName(EdgeConvexity c);

} // namespace kernel
