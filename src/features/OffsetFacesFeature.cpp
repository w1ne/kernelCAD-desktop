#include "OffsetFacesFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>

namespace features {

OffsetFacesFeature::OffsetFacesFeature(std::string id, OffsetFacesParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape OffsetFacesFeature::execute(kernel::OCCTKernel& kernel,
                                          const TopoDS_Shape& targetShape) const
{
    return kernel.offsetFaces(targetShape, m_params.faceIndices, m_params.distance);
}

} // namespace features
