#pragma once
#include "Feature.h"
#include <string>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

struct SplitBodyParams {
    std::string targetBodyId;
    std::string splittingToolId;  // body ID or construction plane ID
    // If usePlane is true, these define the splitting plane
    double planeOx = 0, planeOy = 0, planeOz = 0;
    double planeNx = 0, planeNy = 0, planeNz = 1;
    bool usePlane = true;  // true = use plane params, false = use tool body
};

class SplitBodyFeature : public Feature
{
public:
    explicit SplitBodyFeature(std::string id, SplitBodyParams params);

    FeatureType type() const override { return FeatureType::SplitBody; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Split Body"; }

    SplitBodyParams&       params()       { return m_params; }
    const SplitBodyParams& params() const { return m_params; }

    /// Execute split on the target shape using either a plane or a tool body.
    /// When usePlane is true, the kernel builds a large planar face internally.
    /// When usePlane is false, the caller must supply the tool shape.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& target) const;

    /// Overload for splitting with an explicit tool body shape.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& target,
                         const TopoDS_Shape& tool) const;

private:
    std::string     m_id;
    SplitBodyParams m_params;
};

} // namespace features
