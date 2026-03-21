#pragma once
#include "Feature.h"
#include "../kernel/StableReference.h"
#include <string>
#include <vector>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

struct OffsetFacesParams {
    std::string      targetBodyId;
    std::vector<int> faceIndices;
    double           distance = 2.0;  // offset distance in mm

    /// Stable geometric signatures for the offset faces.
    std::vector<kernel::FaceSignature> faceSignatures;
};

class OffsetFacesFeature : public Feature
{
public:
    explicit OffsetFacesFeature(std::string id, OffsetFacesParams params);

    FeatureType type() const override { return FeatureType::OffsetFaces; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Offset Faces"; }

    OffsetFacesParams&       params()       { return m_params; }
    const OffsetFacesParams& params() const { return m_params; }

    /// Execute offset faces on the target shape and return the modified shape.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape) const;

private:
    std::string       m_id;
    OffsetFacesParams m_params;
};

} // namespace features
