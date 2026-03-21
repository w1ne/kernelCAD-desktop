#pragma once
#include "Feature.h"
#include "../kernel/StableReference.h"
#include <string>
#include <vector>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

struct ShellParams {
    std::string      targetBodyId;
    double           thicknessExpr = 2.0;     // wall thickness
    std::vector<int> removedFaceIds;           // faces to remove (hollow through)

    /// Stable geometric signatures for the removed faces.
    std::vector<kernel::FaceSignature> faceSignatures;
};

class ShellFeature : public Feature
{
public:
    explicit ShellFeature(std::string id, ShellParams params);

    FeatureType type() const override { return FeatureType::Shell; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Shell"; }

    ShellParams&       params()       { return m_params; }
    const ShellParams& params() const { return m_params; }

    /// Execute shell on the target shape and return the modified shape.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& targetShape) const;

private:
    std::string m_id;
    ShellParams m_params;
};

} // namespace features
