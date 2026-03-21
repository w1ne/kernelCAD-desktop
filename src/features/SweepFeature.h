#pragma once
#include "Feature.h"
#include "ExtrudeFeature.h" // for FeatureOperation
#include <string>

// Forward declare
namespace kernel { class OCCTKernel; }
namespace sketch { class Sketch; }

namespace features {

struct SweepParams {
    std::string      profileId;        // sketch profile
    std::string      sketchId;         // parent sketch
    std::string      pathId;           // path sketch or edge reference
    std::string      pathSketchId;     // sketch containing the path
    FeatureOperation operation = FeatureOperation::NewBody;
    // Orientation: perpendicular (profile stays normal to path) or parallel
    bool isPerpendicularOrientation = true;
};

class SweepFeature : public Feature
{
public:
    explicit SweepFeature(std::string id, SweepParams params);

    FeatureType type() const override { return FeatureType::Sweep; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Sweep"; }

    SweepParams&       params()       { return m_params; }
    const SweepParams& params() const { return m_params; }

    /// Execute this feature against the kernel and return the resulting shape.
    /// When profileId/pathId are empty, creates a test shape: sweep a small
    /// circle along a helix-like path.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const sketch::Sketch* profileSketch = nullptr,
                         const sketch::Sketch* pathSketch = nullptr) const;

private:
    std::string m_id;
    SweepParams m_params;
};

} // namespace features
