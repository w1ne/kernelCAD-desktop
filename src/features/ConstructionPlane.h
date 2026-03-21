#pragma once
#include "Feature.h"
#include <string>

namespace features {

enum class PlaneDefinitionType {
    Standard,        // XY, XZ, YZ origin planes
    OffsetFromPlane,
    AngleFromPlane,
    TangentToFace,
    MidPlane,
    ThreePoints
};

struct ConstructionPlaneParams {
    PlaneDefinitionType definitionType = PlaneDefinitionType::Standard;
    std::string standardPlane;   // "XY", "XZ", "YZ" for standard
    std::string parentPlaneId;   // for offset/angle
    double offsetDistance = 0.0;
    double angleDeg = 0.0;
    // Plane definition in world space
    double originX = 0, originY = 0, originZ = 0;
    double normalX = 0, normalY = 0, normalZ = 1;
    double xDirX = 1, xDirY = 0, xDirZ = 0;
};

class ConstructionPlane : public Feature
{
public:
    ConstructionPlane(std::string id, ConstructionPlaneParams params);

    FeatureType type() const override { return FeatureType::ConstructionPlane; }
    std::string id()   const override;
    std::string name() const override;

    const ConstructionPlaneParams& params() const;

    // Get the resolved plane geometry
    void origin(double& x, double& y, double& z) const;
    void normal(double& nx, double& ny, double& nz) const;
    void xDirection(double& x, double& y, double& z) const;
    void yDirection(double& x, double& y, double& z) const;

private:
    std::string m_id;
    ConstructionPlaneParams m_params;
};

} // namespace features
