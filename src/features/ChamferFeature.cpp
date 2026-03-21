#include "ChamferFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>
#include <stdexcept>

namespace features {

ChamferFeature::ChamferFeature(std::string id, ChamferParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape ChamferFeature::execute(kernel::OCCTKernel& kernel,
                                      const TopoDS_Shape& targetShape) const
{
    // Parse distance from the expression string
    double distance = 1.0;
    try {
        std::string expr = m_params.distanceExpr;
        auto spacePos = expr.find(' ');
        if (spacePos != std::string::npos)
            expr = expr.substr(0, spacePos);
        if (!expr.empty())
            distance = std::stod(expr);
    } catch (...) {
        distance = 1.0; // fallback default
    }

    return kernel.chamfer(targetShape, m_params.edgeIds, distance);
}

} // namespace features
