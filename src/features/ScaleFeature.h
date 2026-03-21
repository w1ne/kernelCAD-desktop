#pragma once
#include "Feature.h"
#include <string>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

enum class ScaleType { Uniform, NonUniform };

struct ScaleParams {
    std::string targetBodyId;
    ScaleType scaleType = ScaleType::Uniform;
    double factor = 2.0;          // uniform scale factor
    double factorX = 1.0, factorY = 1.0, factorZ = 1.0;  // non-uniform
    double centerX = 0, centerY = 0, centerZ = 0;  // scale center point
};

class ScaleFeature : public Feature
{
public:
    explicit ScaleFeature(std::string id, ScaleParams params);

    FeatureType type() const override { return FeatureType::Scale; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Scale"; }

    ScaleParams&       params()       { return m_params; }
    const ScaleParams& params() const { return m_params; }

    /// Execute scale on the target shape and return the modified shape.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape) const;

private:
    std::string m_id;
    ScaleParams m_params;
};

} // namespace features
