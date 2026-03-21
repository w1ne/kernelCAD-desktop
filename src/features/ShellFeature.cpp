#include "ShellFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>
#include <stdexcept>

namespace features {

ShellFeature::ShellFeature(std::string id, ShellParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape ShellFeature::execute(kernel::OCCTKernel& kernel,
                                    const TopoDS_Shape& targetShape) const
{
    return kernel.shell(targetShape, m_params.thicknessExpr, m_params.removedFaceIds);
}

} // namespace features
