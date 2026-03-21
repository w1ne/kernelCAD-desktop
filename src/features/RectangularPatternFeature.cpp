#include "RectangularPatternFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>

namespace features {

RectangularPatternFeature::RectangularPatternFeature(std::string id,
                                                       RectangularPatternParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

static double parseSpacing(const std::string& expr, double fallback)
{
    try {
        std::string s = expr;
        auto spacePos = s.find(' ');
        if (spacePos != std::string::npos)
            s = s.substr(0, spacePos);
        if (!s.empty())
            return std::stod(s);
    } catch (...) {}
    return fallback;
}

TopoDS_Shape RectangularPatternFeature::execute(kernel::OCCTKernel& kernel,
                                                 const TopoDS_Shape& targetShape) const
{
    double spacing1 = parseSpacing(m_params.spacing1Expr, 20.0);
    double spacing2 = parseSpacing(m_params.spacing2Expr, 20.0);

    return kernel.rectangularPattern(targetShape,
                                      m_params.dir1X, m_params.dir1Y, m_params.dir1Z,
                                      spacing1, m_params.count1,
                                      m_params.dir2X, m_params.dir2Y, m_params.dir2Z,
                                      spacing2, m_params.count2);
}

} // namespace features
