#pragma once
#include "Feature.h"
#include "../kernel/StableReference.h"
#include <string>
#include <vector>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

enum class ChamferType { EqualDistance, TwoDistances, DistanceAndAngle };

struct ChamferParams {
    std::string      targetBodyId;
    std::vector<int> edgeIds;
    ChamferType      chamferType = ChamferType::EqualDistance;
    std::string      distanceExpr;        // e.g. "1 mm"
    std::string      distance2Expr;       // second distance (TwoDistances mode)
    double           angleDeg = 45.0;     // angle (DistanceAndAngle mode)
    bool             isTangentChain = true;

    /// Stable geometric signatures for the selected edges.
    std::vector<kernel::EdgeSignature> edgeSignatures;
};

class ChamferFeature : public Feature
{
public:
    explicit ChamferFeature(std::string id, ChamferParams params);

    FeatureType type() const override { return FeatureType::Chamfer; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Chamfer"; }

    ChamferParams&       params()       { return m_params; }
    const ChamferParams& params() const { return m_params; }

    /// Execute chamfer on the target shape and return the modified shape.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape) const;

private:
    std::string   m_id;
    ChamferParams m_params;
};

} // namespace features
