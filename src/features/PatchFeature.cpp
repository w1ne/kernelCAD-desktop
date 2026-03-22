#include "PatchFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>

namespace features {

PatchFeature::PatchFeature(std::string id, PatchParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape PatchFeature::execute(kernel::OCCTKernel& kernel,
                                    const TopoDS_Shape& boundaryWire) const
{
    return kernel.patch(boundaryWire);
}

} // namespace features
