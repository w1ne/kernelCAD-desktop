#pragma once
#include "Feature.h"
#include "ExtrudeFeature.h" // for FeatureOperation
#include <string>
#include <vector>

// Forward declare
namespace kernel { class OCCTKernel; }
namespace sketch { class Sketch; }

namespace features {

struct LoftParams {
    std::vector<std::string> sectionIds;       // sketch profile IDs
    std::vector<std::string> sectionSketchIds;  // sketch IDs containing the sections
    bool isClosed = false;                      // periodic loft
    FeatureOperation operation = FeatureOperation::NewBody;
};

class LoftFeature : public Feature
{
public:
    explicit LoftFeature(std::string id, LoftParams params);

    FeatureType type() const override { return FeatureType::Loft; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Loft"; }

    LoftParams&       params()       { return m_params; }
    const LoftParams& params() const { return m_params; }

    /// Execute this feature against the kernel and return the resulting shape.
    /// When sectionIds is empty, creates a test shape: loft between a circle
    /// and a rectangle at different heights.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const std::vector<const sketch::Sketch*>& sketches = {}) const;

private:
    std::string m_id;
    LoftParams  m_params;
};

} // namespace features
