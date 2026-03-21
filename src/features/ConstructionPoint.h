#pragma once
#include "Feature.h"
#include <string>

namespace features {

enum class PointDefinitionType {
    Standard,          // origin
    AtCoordinate,
    CenterOfCircle,
    Intersection       // intersection of line and plane
};

struct ConstructionPointParams {
    PointDefinitionType definitionType = PointDefinitionType::Standard;
    double x = 0, y = 0, z = 0;
    std::string lineId;    // for Intersection
    std::string planeId;   // for Intersection
    std::string circleId;  // for CenterOfCircle
};

class ConstructionPoint : public Feature
{
public:
    ConstructionPoint(std::string id, ConstructionPointParams params);

    FeatureType type() const override { return FeatureType::ConstructionPoint; }
    std::string id()   const override;
    std::string name() const override;

    const ConstructionPointParams& params() const;

    // Get the resolved point location
    void position(double& px, double& py, double& pz) const;

private:
    std::string m_id;
    ConstructionPointParams m_params;
};

} // namespace features
