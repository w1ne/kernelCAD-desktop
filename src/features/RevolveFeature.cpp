#include "RevolveFeature.h"
#include "../kernel/OCCTKernel.h"
#include "../sketch/Sketch.h"
#include "../sketch/SketchToShape.h"
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <stdexcept>

namespace features {

RevolveFeature::RevolveFeature(std::string id, RevolveParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape RevolveFeature::execute(kernel::OCCTKernel& kernel,
                                     const sketch::Sketch* sketch) const
{
    // Parse angle from the expression string (simple numeric parse for now)
    double angle = 360.0;
    try {
        std::string expr = m_params.angleExpr;
        auto spacePos = expr.find(' ');
        if (spacePos != std::string::npos)
            expr = expr.substr(0, spacePos);
        if (!expr.empty())
            angle = std::stod(expr);
    } catch (...) {
        angle = 360.0; // fallback default
    }

    if (m_params.isFullRevolution)
        angle = 360.0;

    if (m_params.profileId.empty()) {
        // No sketch profile yet — create a cylinder primitive as a base feature.
        // Use angle as a proxy for the radius dimension; default height 50.
        double radius = angle > 0 ? angle / 10.0 : 25.0;
        return kernel.makeCylinder(radius, 50.0);
    }

    // Resolve profileId via the sketch system
    if (!sketch)
        throw std::runtime_error("RevolveFeature::execute: profileId is set but no sketch provided");

    // profileId is treated as a comma-separated list of curve IDs forming a profile,
    // or we detect profiles and use the first one if profileId == "auto"
    std::vector<std::string> curveIds;
    if (m_params.profileId == "auto") {
        auto profiles = sketch->detectProfiles();
        if (profiles.empty())
            throw std::runtime_error("RevolveFeature::execute: no profiles detected in sketch");
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
    return kernel.revolve(face, angle);
}

} // namespace features
