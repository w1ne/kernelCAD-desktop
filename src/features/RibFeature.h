#pragma once
#include "Feature.h"
#include <string>

namespace kernel { class OCCTKernel; }
namespace sketch { class Sketch; }
class TopoDS_Shape;

namespace features {

struct RibParams {
    std::string targetBodyId;
    std::string sketchId;
    std::string profileId;   // comma-separated curve IDs
    double thickness = 2.0;
    double depth     = 10.0;
};

class RibFeature : public Feature
{
public:
    explicit RibFeature(std::string id, RibParams params);

    FeatureType type() const override { return FeatureType::Rib; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Rib"; }

    RibParams&       params()       { return m_params; }
    const RibParams& params() const { return m_params; }

    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& body,
                         const sketch::Sketch* sketch = nullptr) const;

private:
    std::string m_id;
    RibParams m_params;
};

} // namespace features
