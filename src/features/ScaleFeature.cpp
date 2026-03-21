#include "ScaleFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>

namespace features {

ScaleFeature::ScaleFeature(std::string id, ScaleParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape ScaleFeature::execute(kernel::OCCTKernel& kernel,
                                    const TopoDS_Shape& targetShape) const
{
    if (m_params.scaleType == ScaleType::Uniform) {
        return kernel.scaleUniform(targetShape, m_params.factor,
                                    m_params.centerX, m_params.centerY, m_params.centerZ);
    } else {
        return kernel.scaleNonUniform(targetShape,
                                       m_params.factorX, m_params.factorY, m_params.factorZ,
                                       m_params.centerX, m_params.centerY, m_params.centerZ);
    }
}

} // namespace features
