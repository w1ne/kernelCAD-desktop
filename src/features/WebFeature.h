#pragma once
#include "Feature.h"
#include <string>

namespace kernel { class OCCTKernel; }
namespace sketch { class Sketch; }
class TopoDS_Shape;

namespace features {

struct WebParams {
    std::string targetBodyId;
    std::string sketchId;
    std::string profileId;   // comma-separated curve IDs
    double thickness = 2.0;
    double depth     = 10.0;
    int    count     = 3;
    double spacing   = 10.0;
};

class WebFeature : public Feature
{
public:
    explicit WebFeature(std::string id, WebParams params);

    FeatureType type() const override { return FeatureType::Web; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Web"; }

    WebParams&       params()       { return m_params; }
    const WebParams& params() const { return m_params; }

    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& body,
                         const sketch::Sketch* sketch = nullptr) const;

private:
    std::string m_id;
    WebParams m_params;
};

} // namespace features
