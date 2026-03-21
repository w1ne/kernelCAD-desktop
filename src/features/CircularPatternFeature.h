#pragma once
#include "Feature.h"
#include "ExtrudeFeature.h"  // for FeatureOperation
#include <string>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

struct CircularPatternParams {
    std::string targetBodyId;
    // Rotation axis (origin + direction)
    double axisOx = 0, axisOy = 0, axisOz = 0;
    double axisDx = 0, axisDy = 0, axisDz = 1;  // default: Z axis
    int count = 6;
    double totalAngleDeg = 360.0;
    FeatureOperation operation = FeatureOperation::Join;
};

class CircularPatternFeature : public Feature
{
public:
    explicit CircularPatternFeature(std::string id, CircularPatternParams params);

    FeatureType type() const override { return FeatureType::CircularPattern; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Circular Pattern"; }

    CircularPatternParams&       params()       { return m_params; }
    const CircularPatternParams& params() const { return m_params; }

    /// Execute circular pattern on the target shape and return the resulting shape.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape) const;

private:
    std::string           m_id;
    CircularPatternParams m_params;
};

} // namespace features
