#pragma once
#include "Feature.h"
#include <string>
#include <array>

namespace features {

enum class JointType {
    Rigid,        // 0 DOF -- fixed connection
    Revolute,     // 1 DOF -- rotation around 1 axis (hinge)
    Slider,       // 1 DOF -- translation along 1 axis
    Cylindrical,  // 2 DOF -- rotation + translation along same axis
    PinSlot,      // 2 DOF -- rotation around 1 axis + slide along perpendicular
    Planar,       // 3 DOF -- 2D translation + rotation in a plane
    Ball          // 3 DOF -- full rotation (ball-and-socket)
};

struct JointLimits {
    bool hasMin = false, hasMax = false;
    double minValue = 0.0, maxValue = 0.0;
    double restValue = 0.0;
};

struct JointGeometry {
    // The reference point and axes for each side of the joint
    double originX = 0, originY = 0, originZ = 0;
    double primaryAxisX = 0, primaryAxisY = 0, primaryAxisZ = 1;    // Z by default
    double secondaryAxisX = 1, secondaryAxisY = 0, secondaryAxisZ = 0; // X by default
};

struct JointParams {
    std::string occurrenceOneId;   // first component occurrence
    std::string occurrenceTwoId;   // second component occurrence
    JointType jointType = JointType::Rigid;

    JointGeometry geometryOne;
    JointGeometry geometryTwo;

    // Current motion values
    double rotationValue = 0.0;    // radians (for Revolute, Cylindrical, etc.)
    double translationValue = 0.0; // mm (for Slider, Cylindrical, etc.)
    double rotation2Value = 0.0;   // second rotation (PinSlot, Planar, Ball)
    double translation2Value = 0.0; // second translation (Planar)
    double rotation3Value = 0.0;   // third rotation (Ball)

    // Limits per DOF
    JointLimits rotationLimits;
    JointLimits translationLimits;

    bool isFlipped = false;
    bool isLocked = false;
    bool isSuppressed = false;
};

class Joint : public Feature {
public:
    explicit Joint(std::string id, JointParams params);

    FeatureType type() const override;
    std::string id() const override;
    std::string name() const override;

    JointParams& params();
    const JointParams& params() const;

    /// Compute the transform matrix (4x4 column-major) for occurrence two
    /// based on the joint type, geometry, and current motion values.
    /// This positions occurrence two relative to occurrence one.
    void computeTransform(double outMatrix[16]) const;

    /// Get the number of DOFs for this joint type.
    int dofCount() const;

    /// Set a DOF value by index (0-based). Clamps to limits.
    void setDOFValue(int dofIndex, double value);
    double getDOFValue(int dofIndex) const;

private:
    std::string m_id;
    JointParams m_params;
};

} // namespace features
