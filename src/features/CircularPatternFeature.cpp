#include "CircularPatternFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>

namespace features {

CircularPatternFeature::CircularPatternFeature(std::string id,
                                                 CircularPatternParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape CircularPatternFeature::execute(kernel::OCCTKernel& kernel,
                                              const TopoDS_Shape& targetShape) const
{
    return kernel.circularPattern(targetShape,
                                   m_params.axisOx, m_params.axisOy, m_params.axisOz,
                                   m_params.axisDx, m_params.axisDy, m_params.axisDz,
                                   m_params.count, m_params.totalAngleDeg);
}

} // namespace features
