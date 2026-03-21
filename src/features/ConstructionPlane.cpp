#include "ConstructionPlane.h"

namespace features {

ConstructionPlane::ConstructionPlane(std::string id, ConstructionPlaneParams params)
    : m_id(std::move(id))
    , m_params(std::move(params))
{}

std::string ConstructionPlane::id() const
{
    return m_id;
}

std::string ConstructionPlane::name() const
{
    if (m_params.definitionType == PlaneDefinitionType::Standard &&
        !m_params.standardPlane.empty()) {
        return m_params.standardPlane + " Plane";
    }
    return "Construction Plane";
}

const ConstructionPlaneParams& ConstructionPlane::params() const
{
    return m_params;
}

void ConstructionPlane::origin(double& x, double& y, double& z) const
{
    x = m_params.originX;
    y = m_params.originY;
    z = m_params.originZ;
}

void ConstructionPlane::normal(double& nx, double& ny, double& nz) const
{
    nx = m_params.normalX;
    ny = m_params.normalY;
    nz = m_params.normalZ;
}

void ConstructionPlane::xDirection(double& x, double& y, double& z) const
{
    x = m_params.xDirX;
    y = m_params.xDirY;
    z = m_params.xDirZ;
}

void ConstructionPlane::yDirection(double& x, double& y, double& z) const
{
    // Y direction = normal x xDir (cross product)
    double nx = m_params.normalX, ny = m_params.normalY, nz = m_params.normalZ;
    double xx = m_params.xDirX,   xy = m_params.xDirY,   xz = m_params.xDirZ;
    x = ny * xz - nz * xy;
    y = nz * xx - nx * xz;
    z = nx * xy - ny * xx;
}

} // namespace features
