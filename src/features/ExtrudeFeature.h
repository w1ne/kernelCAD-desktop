#pragma once
#include "Feature.h"
#include <string>

namespace features {

enum class ExtentType { Distance, ThroughAll, ToEntity, Symmetric };
enum class FeatureOperation { NewBody, Join, Cut, Intersect };

struct ExtrudeParams {
    std::string    profileId;        // EntityId of sketch profile
    std::string    distanceExpr;     // e.g. "10 mm" or param name
    ExtentType     extentType  = ExtentType::Distance;
    FeatureOperation operation = FeatureOperation::Join;
    bool           isSymmetric = false;
    double         taperAngleDeg = 0.0;
    std::string    targetBodyId; // for Join/Cut/Intersect
};

class ExtrudeFeature : public Feature
{
public:
    explicit ExtrudeFeature(std::string id, ExtrudeParams params);

    FeatureType type() const override { return FeatureType::Extrude; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Extrude"; }

    ExtrudeParams&       params()       { return m_params; }
    const ExtrudeParams& params() const { return m_params; }

private:
    std::string   m_id;
    ExtrudeParams m_params;
};

} // namespace features
