#pragma once
#include <string>
#include <vector>
#include <cstddef>

namespace sketch {

enum class EntityType { Point, Line, Circle, Arc, Spline, Ellipse };

struct SketchPoint {
    std::string id;
    double x = 0.0, y = 0.0;   // 2D coordinates in sketch space
    bool isFixed = false;
    size_t paramOffset = 0;     // index into solver state vector
    static constexpr int DOF = 2;
};

struct SketchLine {
    std::string id;
    std::string startPointId;   // references SketchPoint
    std::string endPointId;
    bool isConstruction = false;
    bool isCenterLine = false;
};

struct SketchCircle {
    std::string id;
    std::string centerPointId;
    double radius = 10.0;
    bool isConstruction = false;
    size_t radiusParamOffset = 0;
    static constexpr int DOF = 1;  // center is a separate SketchPoint (2 DOF), radius adds 1
};

struct SketchArc {
    std::string id;
    std::string centerPointId;
    std::string startPointId;
    std::string endPointId;
    double radius = 10.0;
    bool isConstruction = false;
    size_t radiusParamOffset = 0;
    static constexpr int DOF = 1;  // center + endpoints are separate points
};

struct SketchSpline {
    std::string id;
    std::vector<std::string> controlPointIds;  // references to SketchPoints
    int degree = 3;                             // cubic by default
    bool isClosed = false;
    bool isConstruction = false;
    static constexpr int DOF = 0;  // control points are separate SketchPoints
};

struct SketchEllipse {
    std::string id;
    std::string centerPointId;
    double majorRadius = 20.0;
    double minorRadius = 10.0;
    double rotationAngle = 0.0;  // radians, rotation of major axis from sketch X
    bool isConstruction = false;
    size_t majorRadiusParamOffset = 0;
    size_t minorRadiusParamOffset = 0;
    static constexpr int DOF = 2;  // center separate, + major + minor radius
};

} // namespace sketch
