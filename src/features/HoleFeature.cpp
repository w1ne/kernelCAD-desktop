#include "HoleFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>
#include <stdexcept>

namespace features {

HoleFeature::HoleFeature(std::string id, HoleParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

double HoleFeature::parseExpr(const std::string& expr, double fallback)
{
    try {
        std::string num = expr;
        auto spacePos = num.find(' ');
        if (spacePos != std::string::npos)
            num = num.substr(0, spacePos);
        if (!num.empty())
            return std::stod(num);
    } catch (...) {}
    return fallback;
}

TopoDS_Shape HoleFeature::execute(kernel::OCCTKernel& kernel,
                                   const TopoDS_Shape& targetShape) const
{
    double diameter = parseExpr(m_params.diameterExpr, 10.0);
    double depth    = parseExpr(m_params.depthExpr, 0.0);

    switch (m_params.holeType) {
    case HoleType::Simple:
        return kernel.hole(targetShape,
                           m_params.posX, m_params.posY, m_params.posZ,
                           m_params.dirX, m_params.dirY, m_params.dirZ,
                           diameter, depth);

    case HoleType::Counterbore: {
        double cboreDia   = parseExpr(m_params.cboreDiameterExpr, 16.0);
        double cboreDepth = parseExpr(m_params.cboreDepthExpr, 5.0);
        return kernel.counterboreHole(targetShape,
                                       m_params.posX, m_params.posY, m_params.posZ,
                                       m_params.dirX, m_params.dirY, m_params.dirZ,
                                       diameter, depth,
                                       cboreDia, cboreDepth);
    }

    case HoleType::Countersink: {
        double csinkDia = parseExpr(m_params.csinkDiameterExpr, 20.0);
        return kernel.countersinkHole(targetShape,
                                       m_params.posX, m_params.posY, m_params.posZ,
                                       m_params.dirX, m_params.dirY, m_params.dirZ,
                                       diameter, depth,
                                       csinkDia, m_params.csinkAngleDeg);
    }

    default:
        return targetShape;
    }
}

} // namespace features
