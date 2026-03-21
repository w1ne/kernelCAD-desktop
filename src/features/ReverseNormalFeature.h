#pragma once
#include "Feature.h"
#include "../kernel/StableReference.h"
#include <string>
#include <vector>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

struct ReverseNormalParams {
    std::string      targetBodyId;
    std::vector<int> faceIndices;      // faces whose normals to reverse (empty = all)

    /// Stable geometric signatures for the selected faces.
    std::vector<kernel::FaceSignature> faceSignatures;
};

class ReverseNormalFeature : public Feature
{
public:
    explicit ReverseNormalFeature(std::string id, ReverseNormalParams params);

    FeatureType type() const override { return FeatureType::ReverseNormal; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Reverse Normal"; }

    ReverseNormalParams&       params()       { return m_params; }
    const ReverseNormalParams& params() const { return m_params; }

    /// Execute: reverse normals of specified faces (or entire shape).
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape) const;

private:
    std::string         m_id;
    ReverseNormalParams m_params;
};

} // namespace features
