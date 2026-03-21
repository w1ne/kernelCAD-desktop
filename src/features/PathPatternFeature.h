#pragma once
#include "Feature.h"
#include "ExtrudeFeature.h"  // for FeatureOperation
#include <string>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

struct PathPatternParams {
    std::string targetBodyId;
    std::string pathBodyId;        // body containing the path wire/edge
    int count = 5;
    double startOffset = 0.0;      // 0.0 = start of path
    double endOffset = 1.0;        // 1.0 = end of path
    FeatureOperation operation = FeatureOperation::Join;
};

class PathPatternFeature : public Feature
{
public:
    explicit PathPatternFeature(std::string id, PathPatternParams params);

    FeatureType type() const override { return FeatureType::PathPattern; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Path Pattern"; }

    PathPatternParams&       params()       { return m_params; }
    const PathPatternParams& params() const { return m_params; }

    /// Execute path pattern on the target shape along the path wire.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape,
                         const TopoDS_Shape& pathWire) const;

private:
    std::string       m_id;
    PathPatternParams m_params;
};

} // namespace features
