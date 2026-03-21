#include "FilletFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>
#include <stdexcept>

namespace features {

FilletFeature::FilletFeature(std::string id, FilletParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape FilletFeature::execute(kernel::OCCTKernel& kernel,
                                     const TopoDS_Shape& targetShape) const
{
    // Parse radius from the expression string
    double radius = 2.0;
    try {
        std::string expr = m_params.radiusExpr;
        auto spacePos = expr.find(' ');
        if (spacePos != std::string::npos)
            expr = expr.substr(0, spacePos);
        if (!expr.empty())
            radius = std::stod(expr);
    } catch (...) {
        radius = 2.0; // fallback default
    }

    return kernel.fillet(targetShape, m_params.edgeIds, radius);
}

} // namespace features
