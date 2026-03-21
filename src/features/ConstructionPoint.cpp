#include "ConstructionPoint.h"

namespace features {

ConstructionPoint::ConstructionPoint(std::string id, ConstructionPointParams params)
    : m_id(std::move(id))
    , m_params(std::move(params))
{}

std::string ConstructionPoint::id() const
{
    return m_id;
}

std::string ConstructionPoint::name() const
{
    if (m_params.definitionType == PointDefinitionType::Standard)
        return "Origin Point";
    return "Construction Point";
}

const ConstructionPointParams& ConstructionPoint::params() const
{
    return m_params;
}

void ConstructionPoint::position(double& px, double& py, double& pz) const
{
    px = m_params.x;
    py = m_params.y;
    pz = m_params.z;
}

} // namespace features
