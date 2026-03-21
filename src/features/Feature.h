#pragma once
#include <string>

// Forward declare to avoid pulling OCCT into every header
class TopoDS_Shape;

namespace features {

enum class FeatureType {
    Extrude, Revolve, Fillet, Chamfer, Shell,
    Loft, Sweep, Hole, Thread,
    RectangularPattern, CircularPattern, Mirror,
    Draft, Scale, SplitBody, Combine,
    Sketch, ConstructionPlane, ConstructionAxis, ConstructionPoint,
    BaseFeature
};

enum class HealthState { Healthy, Warning, Error };

class Feature
{
public:
    virtual ~Feature() = default;

    virtual FeatureType type() const = 0;
    virtual std::string id()   const = 0;
    virtual std::string name() const = 0;

    HealthState healthState() const               { return m_health; }
    void        setHealthState(HealthState h)      { m_health = h; }
    const std::string& errorMessage() const        { return m_errorMsg; }
    void setErrorMessage(const std::string& msg)   { m_errorMsg = msg; }

protected:
    HealthState m_health   = HealthState::Healthy;
    std::string m_errorMsg;
};

} // namespace features
