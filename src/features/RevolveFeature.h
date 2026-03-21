#pragma once
#include "Feature.h"
#include "ExtrudeFeature.h" // for FeatureOperation
#include <string>

// Forward declare
namespace kernel { class OCCTKernel; }
namespace sketch { class Sketch; }

namespace features {

enum class AxisType { XAxis, YAxis, ZAxis, Custom };

struct RevolveParams {
    std::string      profileId;       // EntityId of sketch profile
    std::string      sketchId;        // ID of the parent sketch feature
    AxisType         axisType  = AxisType::YAxis;
    std::string      angleExpr;       // e.g. "360 deg"
    bool             isFullRevolution = true;
    bool             isProjectAxis = false; // project axis onto profile plane
    FeatureOperation operation = FeatureOperation::NewBody;
    // Two-sided revolve
    std::string      angle2Expr;      // second side angle (if TwoSides)
};

class RevolveFeature : public Feature
{
public:
    explicit RevolveFeature(std::string id, RevolveParams params);

    FeatureType type() const override { return FeatureType::Revolve; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Revolve"; }

    RevolveParams&       params()       { return m_params; }
    const RevolveParams& params() const { return m_params; }

    /// Execute this feature against the kernel and return the resulting shape.
    /// When profileId is empty, creates a test cylinder (makeCylinder) as a
    /// base-feature shortcut; otherwise calls kernel.revolve().
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const sketch::Sketch* sketch = nullptr) const;

private:
    std::string   m_id;
    RevolveParams m_params;
};

} // namespace features
