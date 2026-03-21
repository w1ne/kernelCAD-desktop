#pragma once
#include "Feature.h"
#include <string>

// Forward declare
namespace kernel { class OCCTKernel; }
class TopoDS_Shape;

namespace features {

enum class CombineOperation { Join, Cut, Intersect };

struct CombineParams {
    std::string targetBodyId;
    std::string toolBodyId;
    CombineOperation operation = CombineOperation::Join;
    bool keepToolBody = false;
};

class CombineFeature : public Feature
{
public:
    explicit CombineFeature(std::string id, CombineParams params);

    FeatureType type() const override { return FeatureType::Combine; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Combine"; }

    CombineParams&       params()       { return m_params; }
    const CombineParams& params() const { return m_params; }

    /// Execute combine: apply boolean operation between target and tool shapes.
    TopoDS_Shape execute(kernel::OCCTKernel& kernel,
                         const TopoDS_Shape& target,
                         const TopoDS_Shape& tool) const;

private:
    std::string   m_id;
    CombineParams m_params;
};

} // namespace features
