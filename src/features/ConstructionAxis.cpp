#include "ConstructionAxis.h"

namespace features {

ConstructionAxis::ConstructionAxis(std::string id, ConstructionAxisParams params)
    : m_id(std::move(id))
    , m_params(std::move(params))
{}

std::string ConstructionAxis::id() const
{
    return m_id;
}

std::string ConstructionAxis::name() const
{
    if (m_params.definitionType == AxisDefinitionType::Standard &&
        !m_params.standardAxis.empty()) {
        return m_params.standardAxis + " Axis";
    }
    return "Construction Axis";
}

const ConstructionAxisParams& ConstructionAxis::params() const
{
    return m_params;
}

void ConstructionAxis::origin(double& x, double& y, double& z) const
{
    x = m_params.originX;
    y = m_params.originY;
    z = m_params.originZ;
}

void ConstructionAxis::direction(double& dx, double& dy, double& dz) const
{
    dx = m_params.dirX;
    dy = m_params.dirY;
    dz = m_params.dirZ;
}

} // namespace features
