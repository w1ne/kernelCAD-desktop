#include "CoilFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>

namespace features {

CoilFeature::CoilFeature(std::string id, CoilParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape CoilFeature::execute(kernel::OCCTKernel& kernel,
                                   const TopoDS_Shape& profileShape) const
{
    return kernel.coil(profileShape,
                       m_params.axisOx, m_params.axisOy, m_params.axisOz,
                       m_params.axisDx, m_params.axisDy, m_params.axisDz,
                       m_params.radius, m_params.pitch, m_params.turns,
                       m_params.taperAngleDeg);
}

} // namespace features
