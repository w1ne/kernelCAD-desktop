#pragma once
#include "Feature.h"
#include <string>
#include <vector>

namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

struct StitchParams {
    std::vector<std::string> targetBodyIds;
    double tolerance = 1e-3;
};

class StitchFeature : public Feature
{
public:
    explicit StitchFeature(std::string id, StitchParams params);

    FeatureType type() const override { return FeatureType::Stitch; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Stitch"; }

    StitchParams&       params()       { return m_params; }
    const StitchParams& params() const { return m_params; }

    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const std::vector<TopoDS_Shape>& shapes) const;

private:
    std::string m_id;
    StitchParams m_params;
};

} // namespace features
