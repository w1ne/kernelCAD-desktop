#include "CombineFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>

namespace features {

CombineFeature::CombineFeature(std::string id, CombineParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape CombineFeature::execute(kernel::OCCTKernel& kernel,
                                      const TopoDS_Shape& target,
                                      const TopoDS_Shape& tool) const
{
    return kernel.combine(target, tool, static_cast<int>(m_params.operation));
}

} // namespace features
