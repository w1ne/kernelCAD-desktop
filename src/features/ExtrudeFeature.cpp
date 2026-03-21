#include "ExtrudeFeature.h"
#include "../kernel/OCCTKernel.h"
#include "../sketch/Sketch.h"
#include "../sketch/SketchToShape.h"
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <stdexcept>

namespace features {

ExtrudeFeature::ExtrudeFeature(std::string id, ExtrudeParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape ExtrudeFeature::execute(kernel::OCCTKernel& kernel,
                                     const sketch::Sketch* sketch) const
{
    // Parse distance from the expression string (simple numeric parse for now)
    double distance = 0.0;
    try {
        // Strip unit suffix if present (e.g. "50 mm" -> "50")
        std::string expr = m_params.distanceExpr;
        auto spacePos = expr.find(' ');
        if (spacePos != std::string::npos)
            expr = expr.substr(0, spacePos);
        distance = std::stod(expr);
    } catch (...) {
        distance = 50.0; // fallback default
    }

    if (m_params.profileId.empty()) {
        // No sketch profile yet — create a box primitive as a base feature
        // Use distance as the height dimension; default 50x50 base
        return kernel.makeBox(distance, distance, distance);
    }

    // Resolve profileId via the sketch system
    if (!sketch)
        throw std::runtime_error("ExtrudeFeature::execute: profileId is set but no sketch provided");

    // profileId is treated as a comma-separated list of curve IDs forming a profile,
    // or we detect profiles and use the first one if profileId == "auto"
    std::vector<std::string> curveIds;
    if (m_params.profileId == "auto") {
        auto profiles = sketch->detectProfiles();
        if (profiles.empty())
            throw std::runtime_error("ExtrudeFeature::execute: no profiles detected in sketch");
        curveIds = profiles[0];
    } else {
        // Parse comma-separated curve IDs
        std::string remaining = m_params.profileId;
        while (!remaining.empty()) {
            auto commaPos = remaining.find(',');
            if (commaPos == std::string::npos) {
                curveIds.push_back(remaining);
                break;
            }
            curveIds.push_back(remaining.substr(0, commaPos));
            remaining = remaining.substr(commaPos + 1);
        }
    }

    TopoDS_Face face = sketch::profileToFace(*sketch, curveIds);

    // Parse distance2 for two-sided extrude
    double distance2 = 0.0;
    if (!m_params.distance2Expr.empty()) {
        try {
            std::string expr2 = m_params.distance2Expr;
            auto sp2 = expr2.find(' ');
            if (sp2 != std::string::npos)
                expr2 = expr2.substr(0, sp2);
            distance2 = std::stod(expr2);
        } catch (...) {
            distance2 = 0.0;
        }
    }

    // Two-sided extrude: when distance2 is explicitly set
    if (distance2 > 0.0) {
        return kernel.extrudeTwoSides(face, distance, distance2);
    }

    // Dispatch based on extent type
    switch (m_params.extentType) {
    case ExtentType::Symmetric:
        return kernel.extrudeSymmetric(face, distance);
    case ExtentType::ThroughAll:
        return kernel.extrudeThroughAll(face);
    case ExtentType::Distance:
    default:
        // Also handle symmetric via the isSymmetric flag or direction enum
        if (m_params.isSymmetric || m_params.direction == ExtentDirection::Symmetric) {
            return kernel.extrudeSymmetric(face, distance);
        }
        if (m_params.direction == ExtentDirection::Negative) {
            return kernel.extrude(face, -distance);
        }
        return kernel.extrude(face, distance);
    }
}

} // namespace features
