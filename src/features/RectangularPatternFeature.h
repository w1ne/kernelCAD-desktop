#pragma once
#include "Feature.h"
#include "ExtrudeFeature.h"  // for FeatureOperation
#include <string>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

struct RectangularPatternParams {
    std::string targetBodyId;
    // Direction 1
    double dir1X = 1, dir1Y = 0, dir1Z = 0;
    std::string spacing1Expr = "20 mm";
    int count1 = 3;
    // Direction 2 (optional)
    double dir2X = 0, dir2Y = 1, dir2Z = 0;
    std::string spacing2Expr = "20 mm";
    int count2 = 1;  // 1 = no pattern in this direction
    FeatureOperation operation = FeatureOperation::Join;
};

class RectangularPatternFeature : public Feature
{
public:
    explicit RectangularPatternFeature(std::string id, RectangularPatternParams params);

    FeatureType type() const override { return FeatureType::RectangularPattern; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Rectangular Pattern"; }

    RectangularPatternParams&       params()       { return m_params; }
    const RectangularPatternParams& params() const { return m_params; }

    /// Execute rectangular pattern on the target shape and return the resulting shape.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape) const;

private:
    std::string              m_id;
    RectangularPatternParams m_params;
};

} // namespace features
