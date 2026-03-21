#include "SketchToShape.h"
#include "Sketch.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Circ.hxx>
#include <gp_Elips.hxx>
#include <gp_Ax2.hxx>
#include <gp_Pln.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <Geom_BSplineCurve.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <stdexcept>
#include <cmath>

namespace sketch {

/// Helper: convert 2D sketch coordinates to a 3D gp_Pnt using the sketch plane.
static gp_Pnt toWorld(const Sketch& sk, double sx, double sy)
{
    double wx, wy, wz;
    sk.sketchToWorld(sx, sy, wx, wy, wz);
    return gp_Pnt(wx, wy, wz);
}

TopoDS_Wire profileToWire(const Sketch& sketch, const std::vector<std::string>& profileCurveIds)
{
    if (profileCurveIds.empty())
        throw std::runtime_error("profileToWire: empty profile");

    BRepBuilderAPI_MakeWire wireBuilder;

    for (const auto& curveId : profileCurveIds) {
        // Try to find as a line
        const auto& lines = sketch.lines();
        auto lineIt = lines.find(curveId);
        if (lineIt != lines.end()) {
            const SketchLine& ln = lineIt->second;
            const SketchPoint& startPt = sketch.point(ln.startPointId);
            const SketchPoint& endPt   = sketch.point(ln.endPointId);

            gp_Pnt p1 = toWorld(sketch, startPt.x, startPt.y);
            gp_Pnt p2 = toWorld(sketch, endPt.x, endPt.y);

            BRepBuilderAPI_MakeEdge edgeBuilder(p1, p2);
            if (!edgeBuilder.IsDone())
                throw std::runtime_error("profileToWire: failed to create edge for line '" + curveId + "'");
            wireBuilder.Add(edgeBuilder.Edge());
            continue;
        }

        // Try to find as a circle (full circle = closed profile by itself)
        const auto& circles = sketch.circles();
        auto circIt = circles.find(curveId);
        if (circIt != circles.end()) {
            const SketchCircle& circ = circIt->second;
            const SketchPoint& center = sketch.point(circ.centerPointId);

            gp_Pnt centerPt = toWorld(sketch, center.x, center.y);

            // Get the plane normal for the circle axis
            double nx, ny, nz;
            sketch.planeNormal(nx, ny, nz);
            gp_Dir axis(nx, ny, nz);

            gp_Ax2 ax2(centerPt, axis);
            gp_Circ gpCirc(ax2, circ.radius);

            BRepBuilderAPI_MakeEdge edgeBuilder(gpCirc);
            if (!edgeBuilder.IsDone())
                throw std::runtime_error("profileToWire: failed to create edge for circle '" + curveId + "'");
            wireBuilder.Add(edgeBuilder.Edge());
            continue;
        }

        // Try to find as an arc
        const auto& arcs = sketch.arcs();
        auto arcIt = arcs.find(curveId);
        if (arcIt != arcs.end()) {
            const SketchArc& a = arcIt->second;
            const SketchPoint& startPt = sketch.point(a.startPointId);
            const SketchPoint& endPt   = sketch.point(a.endPointId);
            const SketchPoint& centerPt = sketch.point(a.centerPointId);

            gp_Pnt p1 = toWorld(sketch, startPt.x, startPt.y);
            gp_Pnt p2 = toWorld(sketch, endPt.x, endPt.y);
            gp_Pnt pc = toWorld(sketch, centerPt.x, centerPt.y);

            // Get the plane normal for the arc axis
            double nx, ny, nz;
            sketch.planeNormal(nx, ny, nz);
            gp_Dir axis(nx, ny, nz);

            gp_Ax2 ax2(pc, axis);
            gp_Circ gpCirc(ax2, a.radius);

            // Create arc using the start, a mid-point approximation, and end
            // Use GC_MakeArcOfCircle with three points:
            // start, end, and the circle to constrain the arc direction.
            GC_MakeArcOfCircle arcMaker(gpCirc, p1, p2, Standard_True);
            if (!arcMaker.IsDone())
                throw std::runtime_error("profileToWire: failed to create arc '" + curveId + "'");

            BRepBuilderAPI_MakeEdge edgeBuilder(arcMaker.Value());
            if (!edgeBuilder.IsDone())
                throw std::runtime_error("profileToWire: failed to create edge for arc '" + curveId + "'");
            wireBuilder.Add(edgeBuilder.Edge());
            continue;
        }

        // Try to find as a spline
        const auto& splines = sketch.splines();
        auto splIt = splines.find(curveId);
        if (splIt != splines.end()) {
            const SketchSpline& spl = splIt->second;
            int nPoles = static_cast<int>(spl.controlPointIds.size());
            int degree = spl.degree;

            // Build poles array
            TColgp_Array1OfPnt poles(1, nPoles);
            for (int i = 0; i < nPoles; ++i) {
                const SketchPoint& cp = sketch.point(spl.controlPointIds[i]);
                poles.SetValue(i + 1, toWorld(sketch, cp.x, cp.y));
            }

            // Build uniform clamped knot vector
            int nKnots = nPoles - degree + 1;
            if (nKnots < 2) nKnots = 2;
            TColStd_Array1OfReal knots(1, nKnots);
            TColStd_Array1OfInteger mults(1, nKnots);

            for (int i = 1; i <= nKnots; ++i) {
                knots.SetValue(i, static_cast<double>(i - 1) / (nKnots - 1));
                mults.SetValue(i, 1);
            }
            // Clamped: first and last knot multiplicities = degree + 1
            mults.SetValue(1, degree + 1);
            mults.SetValue(nKnots, degree + 1);

            Handle(Geom_BSplineCurve) bspl = new Geom_BSplineCurve(
                poles, knots, mults, degree, spl.isClosed);

            BRepBuilderAPI_MakeEdge edgeBuilder(bspl);
            if (!edgeBuilder.IsDone())
                throw std::runtime_error("profileToWire: failed to create edge for spline '" + curveId + "'");
            wireBuilder.Add(edgeBuilder.Edge());
            continue;
        }

        // Try to find as an ellipse
        const auto& ellipses = sketch.ellipses();
        auto ellIt = ellipses.find(curveId);
        if (ellIt != ellipses.end()) {
            const SketchEllipse& ell = ellIt->second;
            const SketchPoint& center = sketch.point(ell.centerPointId);

            gp_Pnt centerPt = toWorld(sketch, center.x, center.y);

            double nx, ny, nz;
            sketch.planeNormal(nx, ny, nz);
            gp_Dir axis(nx, ny, nz);

            // Build direction for major axis (rotated from sketch X direction)
            double xdx, xdy, xdz;
            sketch.planeXDir(xdx, xdy, xdz);
            double ydx, ydy, ydz;
            sketch.planeYDir(ydx, ydy, ydz);
            double cosA = std::cos(ell.rotationAngle);
            double sinA = std::sin(ell.rotationAngle);
            gp_Dir majorDir(
                cosA * xdx + sinA * ydx,
                cosA * xdy + sinA * ydy,
                cosA * xdz + sinA * ydz);

            gp_Ax2 ax2(centerPt, axis, majorDir);
            gp_Elips gpEllipse(ax2, ell.majorRadius, ell.minorRadius);

            BRepBuilderAPI_MakeEdge edgeBuilder(gpEllipse);
            if (!edgeBuilder.IsDone())
                throw std::runtime_error("profileToWire: failed to create edge for ellipse '" + curveId + "'");
            wireBuilder.Add(edgeBuilder.Edge());
            continue;
        }

        throw std::runtime_error("profileToWire: unknown curve entity '" + curveId + "'");
    }

    if (!wireBuilder.IsDone())
        throw std::runtime_error("profileToWire: failed to build wire from profile edges");

    return wireBuilder.Wire();
}

TopoDS_Face profileToFace(const Sketch& sketch, const std::vector<std::string>& profileCurveIds)
{
    TopoDS_Wire wire = profileToWire(sketch, profileCurveIds);

    // Build the plane from the sketch coordinate system
    double ox, oy, oz;
    sketch.planeOrigin(ox, oy, oz);

    double nx, ny, nz;
    sketch.planeNormal(nx, ny, nz);

    gp_Pln plane(gp_Pnt(ox, oy, oz), gp_Dir(nx, ny, nz));

    BRepBuilderAPI_MakeFace faceBuilder(plane, wire, Standard_True);
    if (!faceBuilder.IsDone())
        throw std::runtime_error("profileToFace: failed to create face from wire");

    return faceBuilder.Face();
}

} // namespace sketch
