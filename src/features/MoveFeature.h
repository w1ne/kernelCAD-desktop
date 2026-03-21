#pragma once
#include "Feature.h"
#include <string>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

enum class MoveMode { FreeTransform, TranslateXYZ, Rotate };

struct MoveParams {
    std::string targetBodyId;
    MoveMode mode = MoveMode::TranslateXYZ;
    // TranslateXYZ
    double dx = 0, dy = 0, dz = 0;
    // Rotate
    double axisOx = 0, axisOy = 0, axisOz = 0;
    double axisDx = 0, axisDy = 0, axisDz = 1;
    double angleDeg = 0;
    // FreeTransform (4x4 column-major matrix, identity default)
    double matrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    bool createCopy = false;  // if true, keep original and create a new body
};

class MoveFeature : public Feature
{
public:
    explicit MoveFeature(std::string id, MoveParams params);

    FeatureType type() const override { return FeatureType::Move; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Move"; }

    MoveParams&       params()       { return m_params; }
    const MoveParams& params() const { return m_params; }

    /// Execute move/transform on the target shape and return the resulting shape.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape) const;

private:
    std::string m_id;
    MoveParams  m_params;
};

} // namespace features
