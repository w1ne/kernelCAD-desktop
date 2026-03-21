#pragma once
#include "../features/ConstructionPlane.h"
#include "../features/ConstructionAxis.h"
#include "../features/ConstructionPoint.h"
#include <memory>
#include <vector>

namespace document {

class Origin
{
public:
    Origin();

    const features::ConstructionPlane& xyPlane() const;
    const features::ConstructionPlane& xzPlane() const;
    const features::ConstructionPlane& yzPlane() const;

    const features::ConstructionAxis& xAxis() const;
    const features::ConstructionAxis& yAxis() const;
    const features::ConstructionAxis& zAxis() const;

    const features::ConstructionPoint& originPoint() const;

    /// Get a construction entity by ID. Returns nullptr if not found.
    const features::Feature* findById(const std::string& id) const;

    /// Return all origin features for enumeration (e.g. by the feature tree).
    const std::vector<const features::Feature*>& allFeatures() const;

private:
    std::unique_ptr<features::ConstructionPlane> m_xyPlane;
    std::unique_ptr<features::ConstructionPlane> m_xzPlane;
    std::unique_ptr<features::ConstructionPlane> m_yzPlane;

    std::unique_ptr<features::ConstructionAxis> m_xAxis;
    std::unique_ptr<features::ConstructionAxis> m_yAxis;
    std::unique_ptr<features::ConstructionAxis> m_zAxis;

    std::unique_ptr<features::ConstructionPoint> m_originPoint;

    std::vector<const features::Feature*> m_allFeatures;
};

} // namespace document
