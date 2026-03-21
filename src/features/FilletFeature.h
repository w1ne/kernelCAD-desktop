#pragma once
#include "Feature.h"
#include "../kernel/StableReference.h"
#include <string>
#include <vector>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

struct FilletParams {
    std::string      targetBodyId;
    std::vector<int> edgeIds;
    std::string      radiusExpr;          // e.g. "2 mm"
    bool             isVariableRadius = false;
    bool             isG2 = false;        // G2 curvature-continuous fillet
    bool             isTangentChain = true;
    bool             isRollingBallCorner = true;

    /// Stable geometric signatures for the selected edges.
    /// Populated when the feature is first created (or when edge selection
    /// changes).  Used during recompute to remap edgeIds if topology
    /// renumbers.
    std::vector<kernel::EdgeSignature> edgeSignatures;
};

class FilletFeature : public Feature
{
public:
    explicit FilletFeature(std::string id, FilletParams params);

    FeatureType type() const override { return FeatureType::Fillet; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Fillet"; }

    FilletParams&       params()       { return m_params; }
    const FilletParams& params() const { return m_params; }

    /// Execute fillet on the target shape and return the modified shape.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape) const;

private:
    std::string  m_id;
    FilletParams m_params;
};

} // namespace features
