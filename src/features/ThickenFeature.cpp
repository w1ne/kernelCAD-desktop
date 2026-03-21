#include "ThickenFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>
#include <stdexcept>

namespace features {

ThickenFeature::ThickenFeature(std::string id, ThickenParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape ThickenFeature::execute(kernel::OCCTKernel& kernel,
                                      const TopoDS_Shape& targetShape) const
{
    // Parse thickness from the expression string (e.g. "2 mm", "3.5", "1 mm")
    double thickness = 2.0; // default
    try {
        std::string expr = m_params.thicknessExpr;
        auto spacePos = expr.find(' ');
        if (spacePos != std::string::npos)
            expr = expr.substr(0, spacePos);
        if (!expr.empty())
            thickness = std::stod(expr);
    } catch (...) {
        thickness = 2.0; // fallback default
    }

    if (m_params.isSymmetric) {
        // Symmetric: offset the surface by -halfThickness first, then thicken
        // that intermediate result by the full thickness. This produces a solid
        // centered on the original surface.
        double halfThickness = thickness / 2.0;
        TopoDS_Shape offsetSurface = kernel.thicken(targetShape, -halfThickness);
        return kernel.thicken(offsetSurface, thickness);
    }

    return kernel.thicken(targetShape, thickness);
}

} // namespace features
