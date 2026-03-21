#pragma once
#include "Feature.h"
#include "../kernel/StableReference.h"
#include <string>
#include <vector>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

struct DeleteFaceParams {
    std::string      targetBodyId;
    std::vector<int> faceIndices;

    /// Stable geometric signatures for the selected faces.
    std::vector<kernel::FaceSignature> faceSignatures;
};

class DeleteFaceFeature : public Feature
{
public:
    explicit DeleteFaceFeature(std::string id, DeleteFaceParams params);

    FeatureType type() const override { return FeatureType::DeleteFace; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Delete Face"; }

    DeleteFaceParams&       params()       { return m_params; }
    const DeleteFaceParams& params() const { return m_params; }

    /// Execute: remove faces from the solid, healing the gap.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape) const;

private:
    std::string      m_id;
    DeleteFaceParams m_params;
};

} // namespace features
