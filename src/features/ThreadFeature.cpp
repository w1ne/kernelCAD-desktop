#include "ThreadFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>

namespace features {

ThreadFeature::ThreadFeature(std::string id, ThreadParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape ThreadFeature::execute(kernel::OCCTKernel& kernel,
                                     const TopoDS_Shape& targetShape) const
{
    return kernel.thread(targetShape,
                         m_params.cylindricalFaceIndex,
                         m_params.pitch,
                         m_params.depth,
                         m_params.isInternal,
                         m_params.isRightHanded,
                         m_params.isModeled);
}

} // namespace features
