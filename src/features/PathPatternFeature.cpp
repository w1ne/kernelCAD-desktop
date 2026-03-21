#include "PathPatternFeature.h"
#include "../kernel/OCCTKernel.h"
#include <TopoDS_Shape.hxx>

namespace features {

PathPatternFeature::PathPatternFeature(std::string id, PathPatternParams params)
    : m_id(std::move(id)), m_params(std::move(params))
{}

TopoDS_Shape PathPatternFeature::execute(kernel::OCCTKernel& kernel,
                                          const TopoDS_Shape& targetShape,
                                          const TopoDS_Shape& pathWire) const
{
    return kernel.pathPattern(targetShape, pathWire,
                               m_params.count,
                               m_params.startOffset,
                               m_params.endOffset);
}

} // namespace features
