#pragma once
#include "Feature.h"
#include <string>

namespace kernel { class OCCTKernel; }
namespace sketch { class Sketch; }
class TopoDS_Shape;

namespace features {

struct SplitFaceParams {
    std::string targetBodyId;
    int         faceIndex = 0;
    std::string sketchId;
    std::string profileId;   // comma-separated curve IDs
};

class SplitFaceFeature : public Feature
{
public:
    explicit SplitFaceFeature(std::string id, SplitFaceParams params);

    FeatureType type() const override { return FeatureType::SplitFace; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Split Face"; }

    SplitFaceParams&       params()       { return m_params; }
    const SplitFaceParams& params() const { return m_params; }

    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape,
                         const sketch::Sketch* sketch = nullptr) const;

private:
    std::string m_id;
    SplitFaceParams m_params;
};

} // namespace features
