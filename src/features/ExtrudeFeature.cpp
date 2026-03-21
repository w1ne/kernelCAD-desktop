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
    return kernel.extrude(face, distance);
}

} // namespace features
