#pragma once
#include <string>
#include <vector>
#include <utility>
#include <TopoDS_Shape.hxx>

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

    // ── Combine / Split / Offset ops ────────────────────────────────────
    /// Combine two bodies with a boolean operation.
    /// @param operation 0 = join (fuse), 1 = cut, 2 = intersect
    TopoDS_Shape combine(const TopoDS_Shape& target, const TopoDS_Shape& tool,
                         int operation);

    /// Split a body with a splitting tool (plane face, or another body).
    /// Returns the result as a compound of the split pieces.
    TopoDS_Shape splitBody(const TopoDS_Shape& body, const TopoDS_Shape& splittingTool);

    /// Offset specified faces of a body by a distance.
    /// Positive distance moves faces outward (along normal), negative inward.
    TopoDS_Shape offsetFaces(const TopoDS_Shape& shape,
                              const std::vector<int>& faceIndices,
                              double distance);

    // ── Feature ops ───────────────────────────────────────────────────────
    TopoDS_Shape extrude(const TopoDS_Shape& profile, double distance);
    TopoDS_Shape extrudeSymmetric(const TopoDS_Shape& profile, double totalDistance);
    TopoDS_Shape extrudeTwoSides(const TopoDS_Shape& profile, double dist1, double dist2);
    TopoDS_Shape extrudeThroughAll(const TopoDS_Shape& profile);
    TopoDS_Shape revolve(const TopoDS_Shape& profile, double angleDeg);
    TopoDS_Shape fillet(const TopoDS_Shape& shape,
                        const std::vector<int>& edgeIds,
                        double radius);
    TopoDS_Shape chamfer(const TopoDS_Shape& shape,
                         const std::vector<int>& edgeIds,
                         double distance);

    // ── Advanced feature ops ───────────────────────────────────────────
    /// Sweep a profile along a path (wire).
    TopoDS_Shape sweep(const TopoDS_Shape& profile, const TopoDS_Shape& path);

    /// Loft through multiple profile sections.
    TopoDS_Shape loft(const std::vector<TopoDS_Shape>& sections, bool isClosed = false);

    /// Shell — hollow out a solid by removing specified faces and offsetting the rest.
    TopoDS_Shape shell(const TopoDS_Shape& shape, double thickness,
                       const std::vector<int>& removedFaceIds = {});

    // ── Pattern / Mirror ops ──────────────────────────────────────────────
    /// Mirror a shape about a plane defined by origin + normal.
    /// Returns the fused result of original + mirrored copy.
    TopoDS_Shape mirror(const TopoDS_Shape& shape,
                        double planeOx, double planeOy, double planeOz,
                        double planeNx, double planeNy, double planeNz);

    /// Rectangular pattern: repeat shape in two directions.
    /// count1/count2 include the original (count >= 1).
    TopoDS_Shape rectangularPattern(const TopoDS_Shape& shape,
                                     double dirX1, double dirY1, double dirZ1,
                                     double spacing1, int count1,
                                     double dirX2, double dirY2, double dirZ2,
                                     double spacing2, int count2);

    /// Circular pattern: repeat shape around an axis.
    /// count includes the original. totalAngleDeg defaults to 360.
    TopoDS_Shape circularPattern(const TopoDS_Shape& shape,
                                  double axisOx, double axisOy, double axisOz,
                                  double axisDx, double axisDy, double axisDz,
                                  int count, double totalAngleDeg = 360.0);

    // ── Move / Transform ops ─────────────────────────────────────────────
    /// Transform a shape by a 4x4 matrix (16 doubles, column-major).
    TopoDS_Shape transform(const TopoDS_Shape& shape, const double matrix[16]);

    /// Translate a shape by (dx, dy, dz).
    TopoDS_Shape translate(const TopoDS_Shape& shape, double dx, double dy, double dz);

    /// Rotate a shape around an axis (origin + direction) by angleDeg degrees.
    TopoDS_Shape rotate(const TopoDS_Shape& shape,
                        double axisOx, double axisOy, double axisOz,
                        double axisDx, double axisDy, double axisDz,
                        double angleDeg);

    // ── Draft / Thicken ops ──────────────────────────────────────────────
    /// Draft (taper) faces of a shape — apply a draft angle to selected faces
    /// relative to a pull direction.
    TopoDS_Shape draft(const TopoDS_Shape& shape,
                       const std::vector<int>& faceIndices,
                       double angleDeg,
                       double pullDirX, double pullDirY, double pullDirZ);

    /// Thicken a surface/shell into a solid by offsetting.
    TopoDS_Shape thicken(const TopoDS_Shape& shape, double thickness);

    // ── Hole ops ────────────────────────────────────────────────────────
    /// Create a cylindrical hole at a position on a solid.
    /// @param shape Target solid
    /// @param posX, posY, posZ Position of hole center in world coords
    /// @param dirX, dirY, dirZ Direction of hole (into material)
    /// @param diameter Hole diameter
    /// @param depth Hole depth (0 = through-all, uses 1000 mm cut)
    /// @return Modified shape with hole
    TopoDS_Shape hole(const TopoDS_Shape& shape,
                      double posX, double posY, double posZ,
                      double dirX, double dirY, double dirZ,
                      double diameter, double depth);

    /// Create a counterbore hole
    TopoDS_Shape counterboreHole(const TopoDS_Shape& shape,
                                  double posX, double posY, double posZ,
                                  double dirX, double dirY, double dirZ,
                                  double diameter, double depth,
                                  double cboreDiameter, double cboreDepth);

    /// Create a countersink hole
    TopoDS_Shape countersinkHole(const TopoDS_Shape& shape,
                                  double posX, double posY, double posZ,
                                  double dirX, double dirY, double dirZ,
                                  double diameter, double depth,
                                  double csinkDiameter, double csinkAngleDeg);

    // ── Thread ops ────────────────────────────────────────────────────────
    /// Create a helical thread on a cylindrical face.
    /// For modeled threads: creates a helical groove cut into (internal) or
    /// added onto (external) the cylinder surface.
    /// For cosmetic threads: returns the shape unchanged (cosmetic is display-only).
    /// @param cylindricalFaceIndex Index of the cylindrical face (-1 = auto-detect first).
    TopoDS_Shape thread(const TopoDS_Shape& shape,
                        int cylindricalFaceIndex,
                        double pitch,
                        double depth,
                        bool isInternal,
                        bool isRightHanded,
                        bool isModeled);

    // ── Scale ops ────────────────────────────────────────────────────────
    /// Scale a shape uniformly around a center point.
    TopoDS_Shape scaleUniform(const TopoDS_Shape& shape, double factor,
                               double centerX = 0, double centerY = 0, double centerZ = 0);

    // ── Path Pattern ─────────────────────────────────────────────────────
    /// Pattern a shape along a path curve.
    /// Creates copies at even intervals along the path, oriented perpendicular.
    TopoDS_Shape pathPattern(const TopoDS_Shape& shape,
                              const TopoDS_Shape& pathWire,
                              int count, double startOffset = 0.0,
                              double endOffset = 1.0);

    // ── Coil (Helical Sweep) ─────────────────────────────────────────────
    /// Create a helical coil (spring shape).
    /// @param profileShape The cross-section to sweep (e.g., circle wire/face)
    /// @param axisOx,axisOy,axisOz Origin of helix axis
    /// @param axisDx,axisDy,axisDz Direction of helix axis
    /// @param radius Helix radius
    /// @param pitch Distance per revolution
    /// @param turns Number of turns
    /// @param taperAngleDeg Taper angle (0 = cylindrical, >0 = conical)
    TopoDS_Shape coil(const TopoDS_Shape& profileShape,
                      double axisOx, double axisOy, double axisOz,
                      double axisDx, double axisDy, double axisDz,
                      double radius, double pitch, double turns,
                      double taperAngleDeg = 0.0);

    // ── Surface Editing ──────────────────────────────────────────────────
    /// Remove faces from a solid, healing the gap.
    TopoDS_Shape deleteFaces(const TopoDS_Shape& shape,
                              const std::vector<int>& faceIndices);

    /// Replace a face with a new surface (face from another body).
    TopoDS_Shape replaceFace(const TopoDS_Shape& shape, int faceIndex,
                              const TopoDS_Shape& newFace);

    /// Reverse normals of specified faces. If faceIndices is empty,
    /// reverses the entire shape orientation.
    TopoDS_Shape reverseNormals(const TopoDS_Shape& shape,
                                 const std::vector<int>& faceIndices = {});

    /// Scale a shape non-uniformly (different factor per axis) around a center point.
    TopoDS_Shape scaleNonUniform(const TopoDS_Shape& shape,
                                  double factorX, double factorY, double factorZ,
                                  double centerX = 0, double centerY = 0, double centerZ = 0);

    // ── Interference Detection ──────────────────────────────────────────
    struct InterferenceResult {
        std::string body1Id, body2Id;
        double volume;                  // volume of interference region
        TopoDS_Shape interferenceShape; // the overlapping solid
    };

    /// Check interference (overlap) between all pairs of bodies.
    /// Uses BRepAlgoAPI_Common on each pair; non-zero volume = interference.
    std::vector<InterferenceResult> checkInterference(
        const std::vector<std::pair<std::string, TopoDS_Shape>>& bodies);

    // ── Physical Properties ─────────────────────────────────────────────
    struct PhysicalProperties {
        double volume = 0;           // mm^3
        double surfaceArea = 0;      // mm^2
        double mass = 0;             // grams (volume * density)
        double cogX = 0, cogY = 0, cogZ = 0;  // center of gravity
        // Moments of inertia
        double ixx = 0, iyy = 0, izz = 0;
        double ixy = 0, ixz = 0, iyz = 0;
        // Bounding box
        double bboxMinX = 0, bboxMinY = 0, bboxMinZ = 0;
        double bboxMaxX = 0, bboxMaxY = 0, bboxMaxZ = 0;
    };

    /// Compute physical properties (volume, surface area, mass, CoG, inertia, bbox).
    /// @param density Material density in g/mm^3 (default: steel = 0.00785).
    PhysicalProperties computeProperties(const TopoDS_Shape& shape,
                                          double density = 0.00785);

    // ── Import ────────────────────────────────────────────────────────────
    /// Import a STEP file. Returns a vector of shapes (one per root solid/shell).
    /// Throws std::runtime_error on failure.
    std::vector<TopoDS_Shape> importSTEP(const std::string& path);

    /// Import an IGES file. Returns a vector of shapes.
    std::vector<TopoDS_Shape> importIGES(const std::string& path);

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

    // ── Topology queries ─────────────────────────────────────────────────
    /// Count the number of faces in a shape (TopExp_Explorer order).
    int faceCount(const TopoDS_Shape& shape);

    /// Compute centroids of all faces in a shape, returned as packed floats
    /// [x0,y0,z0, x1,y1,z1, ...].  Uses vertex averaging on each face.
    std::vector<float> faceCentroids(const TopoDS_Shape& shape);

    /// Count the number of edges in a shape.
    int edgeCount(const TopoDS_Shape& shape);

    // ── Edge extraction ──────────────────────────────────────────────────
    struct EdgeMesh {
        std::vector<float>    vertices;   // x,y,z polyline points
        std::vector<uint32_t> indices;    // line segment pairs (GL_LINES)
    };
    /// Extract edge polylines from a B-Rep shape for wireframe rendering.
    /// Uses BRepAdaptor_Curve + GCPnts_TangentialDeflection for smooth curves.
    EdgeMesh extractEdges(const TopoDS_Shape& shape, double deflection = 0.1);

private:
    struct Impl;
    Impl* m_impl = nullptr;
};

} // namespace kernel
