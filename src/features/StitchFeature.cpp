#include "StitchFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>

namespace features {

StitchFeature::StitchFeature(std::string id, StitchParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape StitchFeature::execute(kernel::OCCTKernel& kernel,
                                     const std::vector<TopoDS_Shape>& shapes) const
{
    return kernel.stitch(shapes, m_params.tolerance);
}

} // namespace features
