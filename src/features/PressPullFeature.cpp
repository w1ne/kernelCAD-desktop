#include "PressPullFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>
#include <stdexcept>

namespace features {

PressPullFeature::PressPullFeature(std::string id, PressPullParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape PressPullFeature::execute(kernel::OCCTKernel& kernel,
                                        const TopoDS_Shape& targetShape) const
{
    return kernel.offsetFaces(targetShape, {m_params.faceIndex}, m_params.distance);
}

} // namespace features
