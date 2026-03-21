#pragma once
#include <string>
#include <vector>

namespace sketch {

enum class ConstraintType {
    Coincident,         // point-point: 2 DOF removed
    PointOnLine,        // point lies on line: 1 DOF removed
    PointOnCircle,      // point lies on circle: 1 DOF removed
    Distance,           // point-point distance: 1 DOF removed
    DistancePointLine,  // point-to-line distance: 1 DOF removed
    Horizontal,         // line is horizontal: 1 DOF removed
    Vertical,           // line is vertical: 1 DOF removed
    Parallel,           // lines parallel: 1 DOF removed
    Perpendicular,      // lines perpendicular: 1 DOF removed
    Tangent,            // line-circle or circle-circle: 1 DOF removed
    Equal,              // equal length or radius: 1 DOF removed
    Symmetric,          // point-point about line: 2 DOF removed
    Midpoint,           // point at midpoint of line: 2 DOF removed
    Concentric,         // circles same center: 2 DOF removed
    FixedAngle,         // line at angle: 1 DOF removed
    AngleBetween,       // angle between two lines: 1 DOF removed
    Radius,             // circle/arc radius = value: 1 DOF removed
    Fix,                // pin a point to its current position: 2 DOF removed
};

struct SketchConstraint {
    std::string id;
    ConstraintType type;
    std::vector<std::string> entityIds;  // references to points, lines, circles
    double value = 0.0;                  // for dimension constraints (distance, angle, radius)
    double value2 = 0.0;                 // secondary value (e.g. Fix constraint y-coordinate)
    int dofRemoved = 0;                  // degrees of freedom removed by this constraint

    static int defaultDofRemoved(ConstraintType type) {
        switch (type) {
            case ConstraintType::Coincident:        return 2;
            case ConstraintType::PointOnLine:       return 1;
            case ConstraintType::PointOnCircle:     return 1;
            case ConstraintType::Distance:          return 1;
            case ConstraintType::DistancePointLine: return 1;
            case ConstraintType::Horizontal:        return 1;
            case ConstraintType::Vertical:          return 1;
            case ConstraintType::Parallel:          return 1;
            case ConstraintType::Perpendicular:     return 1;
            case ConstraintType::Tangent:           return 1;
            case ConstraintType::Equal:             return 1;
            case ConstraintType::Symmetric:         return 2;
            case ConstraintType::Midpoint:          return 2;
            case ConstraintType::Concentric:        return 2;
            case ConstraintType::FixedAngle:        return 1;
            case ConstraintType::AngleBetween:      return 1;
            case ConstraintType::Radius:            return 1;
            case ConstraintType::Fix:               return 2;
        }
        return 0;
    }
};

} // namespace sketch
