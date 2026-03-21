#pragma once
#include "Feature.h"
#include <string>

// Forward declare
namespace kernel { class OCCTKernel; }
namespace sketch { class Sketch; }

namespace features {

enum class ExtentType { Distance, ThroughAll, ToEntity, Symmetric };
enum class ExtentDirection { Positive, Negative, Symmetric };
enum class FeatureOperation { NewBody, Join, Cut, Intersect, NewComponent };

struct ExtrudeParams {
    std::string      profileId;        // EntityId of sketch profile
    std::string      sketchId;         // ID of the parent sketch feature
    std::string      distanceExpr;     // e.g. "10 mm" or param name
    ExtentType       extentType  = ExtentType::Distance;
    ExtentDirection  direction   = ExtentDirection::Positive;
    FeatureOperation operation   = FeatureOperation::Join;
    bool             isSymmetric = false;
    double           taperAngleDeg = 0.0;
    std::string      targetBodyId; // for Join/Cut/Intersect
    // Two-sided extrude
    std::string      distance2Expr;    // second side distance (if TwoSides)
    double           taperAngle2Deg = 0.0;
    // Thin extrude
    bool             isThinExtrude = false;
    double           wallThickness = 1.0;
};

class ExtrudeFeature : public Feature
{
public:
    explicit ExtrudeFeature(std::string id, ExtrudeParams params);

    FeatureType type() const override { return FeatureType::Extrude; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Extrude"; }

    ExtrudeParams&       params()       { return m_params; }
    const ExtrudeParams& params() const { return m_params; }

    /// Execute this feature against the kernel and return the resulting shape.
    /// When profileId is empty, uses makeBox as a base-feature shortcut.
    /// When profileId is set and a sketch is provided, resolves the profile
    /// via SketchToShape and extrudes it.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const sketch::Sketch* sketch = nullptr) const;

private:
    std::string   m_id;
    ExtrudeParams m_params;
};

} // namespace features
