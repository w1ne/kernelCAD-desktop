#include "DeleteFaceFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>

namespace features {

DeleteFaceFeature::DeleteFaceFeature(std::string id, DeleteFaceParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape DeleteFaceFeature::execute(kernel::OCCTKernel& kernel,
                                         const TopoDS_Shape& targetShape) const
{
    return kernel.deleteFaces(targetShape, m_params.faceIndices);
}

} // namespace features
