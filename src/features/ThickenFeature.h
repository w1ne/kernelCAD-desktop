#pragma once
#include "Feature.h"
#include <string>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

struct ThickenParams {
    std::string targetBodyId;
    std::string thicknessExpr = "2 mm";  // thickness expression
    bool isSymmetric = false;            // offset both sides
};

class ThickenFeature : public Feature
{
public:
    explicit ThickenFeature(std::string id, ThickenParams params);

    FeatureType type() const override { return FeatureType::Thicken; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Thicken"; }

    ThickenParams&       params()       { return m_params; }
    const ThickenParams& params() const { return m_params; }

    /// Execute thicken on the target shape and return the resulting solid.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape) const;

private:
    std::string   m_id;
    ThickenParams m_params;
};

} // namespace features
