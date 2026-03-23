#pragma once
#include "Feature.h"
#include <string>
#include <vector>

namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

struct UnstitchParams {
    std::string targetBodyId;
};

class UnstitchFeature : public Feature
{
public:
    explicit UnstitchFeature(std::string id, UnstitchParams params);

    FeatureType type() const override { return FeatureType::Unstitch; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Unstitch"; }

    UnstitchParams&       params()       { return m_params; }
    const UnstitchParams& params() const { return m_params; }

    /// Separate a compound/solid into individual face shells.
    /// Returns a compound of separate faces.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& shape) const;

private:
    std::string m_id;
    UnstitchParams m_params;
};

} // namespace features
