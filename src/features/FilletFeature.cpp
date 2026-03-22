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

    // Expand selection to tangent-connected edges if enabled
    std::vector<int> edgeIds = m_params.edgeIds;
    if (m_params.isTangentChain && !edgeIds.empty()) {
        try {
            edgeIds = kernel.expandTangentChain(targetShape, edgeIds);
        } catch (...) {
            // If expansion fails, proceed with original selection
        }
    }

    return kernel.fillet(targetShape, edgeIds, radius);
}

} // namespace features
