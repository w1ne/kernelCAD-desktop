#include "MirrorFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>

namespace features {

MirrorFeature::MirrorFeature(std::string id, MirrorParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape MirrorFeature::execute(kernel::OCCTKernel& kernel,
                                     const TopoDS_Shape& targetShape) const
{
    return kernel.mirror(targetShape,
                         m_params.planeOx, m_params.planeOy, m_params.planeOz,
                         m_params.planeNx, m_params.planeNy, m_params.planeNz);
}

} // namespace features
