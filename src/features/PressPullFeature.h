#pragma once
#include "Feature.h"
#include <string>
#include <vector>

namespace kernel { class OCCTKernel; }

namespace features {

struct PressPullParams {
    std::string targetBodyId;
    int         faceIndex = 0;
    double      distance  = 5.0;
    std::string distanceExpr;  // e.g. "5 mm" or parameter name
    // Stable face signature for recompute resilience
    std::vector<std::string> faceSignatures;
};

class PressPullFeature : public Feature
{
public:
    explicit PressPullFeature(std::string id, PressPullParams params);

    FeatureType type() const override { return FeatureType::PressPull; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Press/Pull"; }

    PressPullParams&       params()       { return m_params; }
    const PressPullParams& params() const { return m_params; }

    /// Execute: push/pull the face on the target shape.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape) const;

private:
    std::string     m_id;
    PressPullParams m_params;
};

} // namespace features
