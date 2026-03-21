#include "SweepFeature.h"
#include "../kernel/OCCTKernel.h"
#include "../sketch/Sketch.h"
#include "../sketch/SketchToShape.h"
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Face.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <GC_MakeCircle.hxx>
#include <Geom_Circle.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <stdexcept>
#include <cmath>

namespace features {

SweepFeature::SweepFeature(std::string id, SweepParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape SweepFeature::execute(kernel::OCCTKernel& kernel,
                                    const sketch::Sketch* profileSketch,
                                    const sketch::Sketch* pathSketch) const
{
    if (m_params.profileId.empty() || m_params.pathId.empty()) {
        // Test shape: sweep a small circle along a curved path (quarter helix approximation)
        // Profile: circle of radius 3 at origin, in XY plane
        gp_Ax2 circleAx(gp_Pnt(20, 0, 0), gp_Dir(0, 0, 1));
        gp_Circ circle(circleAx, 3.0);
        TopoDS_Edge circleEdge = BRepBuilderAPI_MakeEdge(circle);
        TopoDS_Wire profileWire = BRepBuilderAPI_MakeWire(circleEdge);
        TopoDS_Face profileFace = BRepBuilderAPI_MakeFace(profileWire);

        // Path: a series of edges forming a gentle S-curve in 3D
        // We approximate with line segments to form a path wire
        BRepBuilderAPI_MakeWire pathBuilder;
        const int nSegments = 20;
        const double totalHeight = 60.0;
        const double radius = 20.0;
        for (int i = 0; i < nSegments; ++i) {
            double t0 = static_cast<double>(i) / nSegments;
            double t1 = static_cast<double>(i + 1) / nSegments;
            double angle0 = t0 * 2.0 * M_PI;
            double angle1 = t1 * 2.0 * M_PI;
            gp_Pnt p0(radius * std::cos(angle0), radius * std::sin(angle0), t0 * totalHeight);
            gp_Pnt p1(radius * std::cos(angle1), radius * std::sin(angle1), t1 * totalHeight);
            pathBuilder.Add(BRepBuilderAPI_MakeEdge(p0, p1));
        }
        TopoDS_Wire pathWire = pathBuilder.Wire();

        return kernel.sweep(profileFace, pathWire);
    }

    // Resolve profile from sketch
    if (!profileSketch)
        throw std::runtime_error("SweepFeature::execute: profileId is set but no profile sketch provided");
    if (!pathSketch)
        throw std::runtime_error("SweepFeature::execute: pathId is set but no path sketch provided");

    // Parse profile curve IDs
    std::vector<std::string> profileCurveIds;
    {
        std::string remaining = m_params.profileId;
        while (!remaining.empty()) {
            auto commaPos = remaining.find(',');
            if (commaPos == std::string::npos) {
                profileCurveIds.push_back(remaining);
                break;
            }
            profileCurveIds.push_back(remaining.substr(0, commaPos));
            remaining = remaining.substr(commaPos + 1);
        }
    }

    // Parse path curve IDs
    std::vector<std::string> pathCurveIds;
    {
        std::string remaining = m_params.pathId;
        while (!remaining.empty()) {
            auto commaPos = remaining.find(',');
            if (commaPos == std::string::npos) {
                pathCurveIds.push_back(remaining);
                break;
            }
            pathCurveIds.push_back(remaining.substr(0, commaPos));
            remaining = remaining.substr(commaPos + 1);
        }
    }

    TopoDS_Face profileFace = sketch::profileToFace(*profileSketch, profileCurveIds);
    TopoDS_Wire pathWire = sketch::profileToWire(*pathSketch, pathCurveIds);

    return kernel.sweep(profileFace, pathWire);
}

} // namespace features
