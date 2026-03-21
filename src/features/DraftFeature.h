#pragma once
#include "Feature.h"
#include "../kernel/StableReference.h"
#include <string>
#include <vector>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

struct DraftParams {
    std::string      targetBodyId;
    std::vector<int> faceIndices;
    std::string      angleExpr = "3 deg";  // draft angle expression
    double pullDirX = 0, pullDirY = 0, pullDirZ = 1;  // pull direction

    /// Stable geometric signatures for the draft faces.
    std::vector<kernel::FaceSignature> faceSignatures;
};

class DraftFeature : public Feature
{
public:
    explicit DraftFeature(std::string id, DraftParams params);

    FeatureType type() const override { return FeatureType::Draft; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Draft"; }

    DraftParams&       params()       { return m_params; }
    const DraftParams& params() const { return m_params; }

    /// Execute draft on the target shape and return the modified shape.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape) const;

private:
    std::string m_id;
    DraftParams m_params;
};

} // namespace features
