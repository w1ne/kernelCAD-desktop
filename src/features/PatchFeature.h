#pragma once
#include "Feature.h"
#include <string>

namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

struct PatchParams {
    std::string boundaryBodyId;   // body whose wire defines the boundary
};

class PatchFeature : public Feature
{
public:
    explicit PatchFeature(std::string id, PatchParams params);

    FeatureType type() const override { return FeatureType::Patch; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Patch"; }

    PatchParams&       params()       { return m_params; }
    const PatchParams& params() const { return m_params; }

    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& boundaryWire) const;

private:
    std::string m_id;
    PatchParams m_params;
};

} // namespace features
