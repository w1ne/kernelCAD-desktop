#pragma once
#include "Feature.h"
#include <string>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

struct CoilParams {
    std::string profileBodyId;     // body containing the cross-section profile
    // Helix axis (origin + direction)
    double axisOx = 0, axisOy = 0, axisOz = 0;
    double axisDx = 0, axisDy = 0, axisDz = 1;  // default: Z axis
    double radius = 10.0;
    double pitch = 5.0;            // distance per revolution
    double turns = 5.0;
    double taperAngleDeg = 0.0;    // 0 = cylindrical, >0 = conical
};

class CoilFeature : public Feature
{
public:
    explicit CoilFeature(std::string id, CoilParams params);

    FeatureType type() const override { return FeatureType::Coil; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Coil"; }

    CoilParams&       params()       { return m_params; }
    const CoilParams& params() const { return m_params; }

    /// Execute coil: sweep profile along helical path.
    /// If profileShape is null, creates a default circular cross-section.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& profileShape) const;

private:
    std::string m_id;
    CoilParams  m_params;
};

} // namespace features
