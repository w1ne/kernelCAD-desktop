#include "LoftFeature.h"
#include "../kernel/OCCTKernel.h"
#include "../sketch/Sketch.h"
#include "../sketch/SketchToShape.h"
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Face.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <stdexcept>
#include <cmath>

namespace features {

LoftFeature::LoftFeature(std::string id, LoftParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape LoftFeature::execute(kernel::OCCTKernel& kernel,
                                   const std::vector<const sketch::Sketch*>& sketches) const
{
    if (m_params.sectionIds.empty()) {
        // Test shape: loft between a circle at Z=0 and a square at Z=40
        // Section 1: circle of radius 15 at Z=0
        gp_Ax2 circleAx(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
        gp_Circ circle(circleAx, 15.0);
        TopoDS_Edge circleEdge = BRepBuilderAPI_MakeEdge(circle);
        TopoDS_Wire circleWire = BRepBuilderAPI_MakeWire(circleEdge);

        // Section 2: square 20x20 centered at Z=40
        gp_Pnt s0(-10, -10, 40);
        gp_Pnt s1( 10, -10, 40);
        gp_Pnt s2( 10,  10, 40);
        gp_Pnt s3(-10,  10, 40);

        BRepBuilderAPI_MakeWire squareBuilder;
        squareBuilder.Add(BRepBuilderAPI_MakeEdge(s0, s1));
        squareBuilder.Add(BRepBuilderAPI_MakeEdge(s1, s2));
        squareBuilder.Add(BRepBuilderAPI_MakeEdge(s2, s3));
        squareBuilder.Add(BRepBuilderAPI_MakeEdge(s3, s0));
        TopoDS_Wire squareWire = squareBuilder.Wire();

        std::vector<TopoDS_Shape> sections;
        sections.push_back(circleWire);
        sections.push_back(squareWire);

        return kernel.loft(sections, m_params.isClosed);
    }

    // Resolve sections from sketches
    if (sketches.size() != m_params.sectionIds.size())
        throw std::runtime_error("LoftFeature::execute: mismatch between sectionIds and provided sketches");

    std::vector<TopoDS_Shape> sections;
    for (size_t i = 0; i < m_params.sectionIds.size(); ++i) {
        const auto* sk = sketches[i];
        if (!sk)
            throw std::runtime_error("LoftFeature::execute: null sketch for section " + std::to_string(i));

        // Parse curve IDs for this section
        std::vector<std::string> curveIds;
        std::string remaining = m_params.sectionIds[i];
        while (!remaining.empty()) {
            auto commaPos = remaining.find(',');
            if (commaPos == std::string::npos) {
                curveIds.push_back(remaining);
                break;
            }
            curveIds.push_back(remaining.substr(0, commaPos));
            remaining = remaining.substr(commaPos + 1);
        }

        TopoDS_Wire wire = sketch::profileToWire(*sk, curveIds);
        sections.push_back(wire);
    }

    return kernel.loft(sections, m_params.isClosed);
}

} // namespace features
