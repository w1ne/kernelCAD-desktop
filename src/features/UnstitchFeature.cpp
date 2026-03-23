#include "UnstitchFeature.h"
#include "../kernel/OCCTKernel.h"

namespace features {

UnstitchFeature::UnstitchFeature(std::string id, UnstitchParams params)
    : m_id(std::move(id))
    , m_params(std::move(params))
{}

TopoDS_Shape UnstitchFeature::execute(kernel::OCCTKernel& kernel,
                                       const TopoDS_Shape& shape) const
{
    return kernel.unstitch(shape);
}

} // namespace features
