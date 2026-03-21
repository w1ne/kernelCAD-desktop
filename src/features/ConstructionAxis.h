#pragma once
#include "Feature.h"
#include <string>

namespace features {

enum class AxisDefinitionType {
    Standard,          // X, Y, Z
    ThroughTwoPoints,
    NormalToFace,
    EdgeAxis,
    Intersection       // intersection of two planes
};

struct ConstructionAxisParams {
    AxisDefinitionType definitionType = AxisDefinitionType::Standard;
    std::string standardAxis; // "X", "Y", "Z"
    double originX = 0, originY = 0, originZ = 0;
    double dirX = 0, dirY = 0, dirZ = 1;
    std::string point1Id, point2Id;       // for ThroughTwoPoints
    std::string plane1Id, plane2Id;       // for Intersection
};

class ConstructionAxis : public Feature
{
public:
    ConstructionAxis(std::string id, ConstructionAxisParams params);

    FeatureType type() const override { return FeatureType::ConstructionAxis; }
    std::string id()   const override;
    std::string name() const override;

    const ConstructionAxisParams& params() const;

    // Get the resolved axis geometry
    void origin(double& x, double& y, double& z) const;
    void direction(double& dx, double& dy, double& dz) const;

private:
    std::string m_id;
    ConstructionAxisParams m_params;
};

} // namespace features
