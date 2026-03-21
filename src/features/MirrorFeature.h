#pragma once
#include "Feature.h"
#include "ExtrudeFeature.h"  // for FeatureOperation
#include <string>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

struct MirrorParams {
    std::string targetBodyId;
    // Mirror plane (origin + normal)
    double planeOx = 0, planeOy = 0, planeOz = 0;
    double planeNx = 1, planeNy = 0, planeNz = 0;  // default: YZ plane
    bool isCombine = true;  // fuse with original
};

class MirrorFeature : public Feature
{
public:
    explicit MirrorFeature(std::string id, MirrorParams params);

    FeatureType type() const override { return FeatureType::Mirror; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Mirror"; }

    MirrorParams&       params()       { return m_params; }
    const MirrorParams& params() const { return m_params; }

    /// Execute mirror on the target shape and return the resulting shape.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape) const;

private:
    std::string  m_id;
    MirrorParams m_params;
};

} // namespace features
