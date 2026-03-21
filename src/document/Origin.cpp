#include "Origin.h"

namespace document {

Origin::Origin()
{
    // ── XY Plane: origin (0,0,0), normal (0,0,1), xDir (1,0,0) ──
    {
        features::ConstructionPlaneParams p;
        p.definitionType = features::PlaneDefinitionType::Standard;
        p.standardPlane  = "XY";
        p.originX = 0; p.originY = 0; p.originZ = 0;
        p.normalX = 0; p.normalY = 0; p.normalZ = 1;
        p.xDirX   = 1; p.xDirY   = 0; p.xDirZ   = 0;
        m_xyPlane = std::make_unique<features::ConstructionPlane>("origin_xy", std::move(p));
    }

    // ── XZ Plane: origin (0,0,0), normal (0,1,0), xDir (1,0,0) ──
    {
        features::ConstructionPlaneParams p;
        p.definitionType = features::PlaneDefinitionType::Standard;
        p.standardPlane  = "XZ";
        p.originX = 0; p.originY = 0; p.originZ = 0;
        p.normalX = 0; p.normalY = 1; p.normalZ = 0;
        p.xDirX   = 1; p.xDirY   = 0; p.xDirZ   = 0;
        m_xzPlane = std::make_unique<features::ConstructionPlane>("origin_xz", std::move(p));
    }

    // ── YZ Plane: origin (0,0,0), normal (1,0,0), xDir (0,1,0) ──
    {
        features::ConstructionPlaneParams p;
        p.definitionType = features::PlaneDefinitionType::Standard;
        p.standardPlane  = "YZ";
        p.originX = 0; p.originY = 0; p.originZ = 0;
        p.normalX = 1; p.normalY = 0; p.normalZ = 0;
        p.xDirX   = 0; p.xDirY   = 1; p.xDirZ   = 0;
        m_yzPlane = std::make_unique<features::ConstructionPlane>("origin_yz", std::move(p));
    }

    // ── X Axis ──
    {
        features::ConstructionAxisParams p;
        p.definitionType = features::AxisDefinitionType::Standard;
        p.standardAxis   = "X";
        p.originX = 0; p.originY = 0; p.originZ = 0;
        p.dirX    = 1; p.dirY    = 0; p.dirZ    = 0;
        m_xAxis = std::make_unique<features::ConstructionAxis>("origin_x_axis", std::move(p));
    }

    // ── Y Axis ──
    {
        features::ConstructionAxisParams p;
        p.definitionType = features::AxisDefinitionType::Standard;
        p.standardAxis   = "Y";
        p.originX = 0; p.originY = 0; p.originZ = 0;
        p.dirX    = 0; p.dirY    = 1; p.dirZ    = 0;
        m_yAxis = std::make_unique<features::ConstructionAxis>("origin_y_axis", std::move(p));
    }

    // ── Z Axis ──
    {
        features::ConstructionAxisParams p;
        p.definitionType = features::AxisDefinitionType::Standard;
        p.standardAxis   = "Z";
        p.originX = 0; p.originY = 0; p.originZ = 0;
        p.dirX    = 0; p.dirY    = 0; p.dirZ    = 1;
        m_zAxis = std::make_unique<features::ConstructionAxis>("origin_z_axis", std::move(p));
    }

    // ── Origin Point ──
    {
        features::ConstructionPointParams p;
        p.definitionType = features::PointDefinitionType::Standard;
        p.x = 0; p.y = 0; p.z = 0;
        m_originPoint = std::make_unique<features::ConstructionPoint>("origin_point", std::move(p));
    }

    // Build the flat list for enumeration
    m_allFeatures = {
        m_xyPlane.get(),
        m_xzPlane.get(),
        m_yzPlane.get(),
        m_xAxis.get(),
        m_yAxis.get(),
        m_zAxis.get(),
        m_originPoint.get()
    };
}

const features::ConstructionPlane& Origin::xyPlane() const { return *m_xyPlane; }
const features::ConstructionPlane& Origin::xzPlane() const { return *m_xzPlane; }
const features::ConstructionPlane& Origin::yzPlane() const { return *m_yzPlane; }

const features::ConstructionAxis& Origin::xAxis() const { return *m_xAxis; }
const features::ConstructionAxis& Origin::yAxis() const { return *m_yAxis; }
const features::ConstructionAxis& Origin::zAxis() const { return *m_zAxis; }

const features::ConstructionPoint& Origin::originPoint() const { return *m_originPoint; }

const features::Feature* Origin::findById(const std::string& id) const
{
    for (auto* f : m_allFeatures) {
        if (f->id() == id)
            return f;
    }
    return nullptr;
}

const std::vector<const features::Feature*>& Origin::allFeatures() const
{
    return m_allFeatures;
}

} // namespace document
