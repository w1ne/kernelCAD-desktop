#include "DraftFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>
#include <stdexcept>

namespace features {

DraftFeature::DraftFeature(std::string id, DraftParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape DraftFeature::execute(kernel::OCCTKernel& kernel,
                                    const TopoDS_Shape& targetShape) const
{
    // Parse angle from the expression string (e.g. "3 deg", "5", "1.5 deg")
    double angleDeg = 3.0; // default
    try {
        std::string expr = m_params.angleExpr;
        auto spacePos = expr.find(' ');
        if (spacePos != std::string::npos)
            expr = expr.substr(0, spacePos);
        if (!expr.empty())
            angleDeg = std::stod(expr);
    } catch (...) {
        angleDeg = 3.0; // fallback default
    }

    return kernel.draft(targetShape, m_params.faceIndices, angleDeg,
                        m_params.pullDirX, m_params.pullDirY, m_params.pullDirZ);
}

} // namespace features
