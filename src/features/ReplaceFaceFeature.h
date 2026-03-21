#pragma once
#include "Feature.h"
#include "../kernel/StableReference.h"
#include <string>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

struct ReplaceFaceParams {
    std::string targetBodyId;
    int         faceIndex = 0;         // index of the face to replace
    std::string replacementBodyId;     // body containing the replacement face/surface

    /// Stable geometric signature for the target face.
    kernel::FaceSignature faceSignature;
};

class ReplaceFaceFeature : public Feature
{
public:
    explicit ReplaceFaceFeature(std::string id, ReplaceFaceParams params);

    FeatureType type() const override { return FeatureType::ReplaceFace; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Replace Face"; }

    ReplaceFaceParams&       params()       { return m_params; }
    const ReplaceFaceParams& params() const { return m_params; }

    /// Execute: replace a face on the target with a face from the replacement body.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape,
                         const TopoDS_Shape& newFace) const;

private:
    std::string       m_id;
    ReplaceFaceParams m_params;
};

} // namespace features
