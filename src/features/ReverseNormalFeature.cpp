#include "ReverseNormalFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>

namespace features {

ReverseNormalFeature::ReverseNormalFeature(std::string id, ReverseNormalParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape ReverseNormalFeature::execute(kernel::OCCTKernel& kernel,
                                            const TopoDS_Shape& targetShape) const
{
    return kernel.reverseNormals(targetShape, m_params.faceIndices);
}

} // namespace features
