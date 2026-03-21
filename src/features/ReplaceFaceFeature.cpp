#include "ReplaceFaceFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>

namespace features {

ReplaceFaceFeature::ReplaceFaceFeature(std::string id, ReplaceFaceParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape ReplaceFaceFeature::execute(kernel::OCCTKernel& kernel,
                                          const TopoDS_Shape& targetShape,
                                          const TopoDS_Shape& newFace) const
{
    return kernel.replaceFace(targetShape, m_params.faceIndex, newFace);
}

} // namespace features
