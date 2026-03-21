#include "Serializer.h"
#include "Document.h"
#include "JsonWriter.h"
#include "JsonReader.h"
#include "../kernel/StableReference.h"
#include "../features/ConstructionPlane.h"
#include "../features/ConstructionAxis.h"
#include "../features/ConstructionPoint.h"
#include "../features/PathPatternFeature.h"
#include "../features/CoilFeature.h"
#include "../features/DeleteFaceFeature.h"
#include "../features/ReplaceFaceFeature.h"
#include "../features/ReverseNormalFeature.h"
#include "../features/Joint.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace document {

// ── Enum string helpers ─────────────────────────────────────────────────────

static const char* featureTypeToStr(features::FeatureType t)
{
    switch (t) {
    case features::FeatureType::Extrude:             return "Extrude";
    case features::FeatureType::Revolve:             return "Revolve";
    case features::FeatureType::Fillet:              return "Fillet";
    case features::FeatureType::Chamfer:             return "Chamfer";
    case features::FeatureType::Shell:               return "Shell";
    case features::FeatureType::Loft:                return "Loft";
    case features::FeatureType::Sweep:               return "Sweep";
    case features::FeatureType::Mirror:              return "Mirror";
    case features::FeatureType::RectangularPattern:  return "RectangularPattern";
    case features::FeatureType::CircularPattern:     return "CircularPattern";
    case features::FeatureType::Sketch:              return "Sketch";
    case features::FeatureType::Hole:                return "Hole";
    case features::FeatureType::Combine:             return "Combine";
    case features::FeatureType::SplitBody:           return "SplitBody";
    case features::FeatureType::OffsetFaces:         return "OffsetFaces";
    case features::FeatureType::Move:                return "Move";
    case features::FeatureType::Draft:               return "Draft";
    case features::FeatureType::Thicken:             return "Thicken";
    case features::FeatureType::Thread:              return "Thread";
    case features::FeatureType::Scale:               return "Scale";
    case features::FeatureType::ConstructionPlane:   return "ConstructionPlane";
    case features::FeatureType::ConstructionAxis:    return "ConstructionAxis";
    case features::FeatureType::ConstructionPoint:   return "ConstructionPoint";
    case features::FeatureType::PathPattern:         return "PathPattern";
    case features::FeatureType::Coil:               return "Coil";
    case features::FeatureType::DeleteFace:         return "DeleteFace";
    case features::FeatureType::ReplaceFace:        return "ReplaceFace";
    case features::FeatureType::ReverseNormal:      return "ReverseNormal";
    case features::FeatureType::Joint:              return "Joint";
    default:                                         return "Unknown";
    }
}

static features::FeatureType featureTypeFromStr(const std::string& s)
{
    if (s == "Extrude")             return features::FeatureType::Extrude;
    if (s == "Revolve")             return features::FeatureType::Revolve;
    if (s == "Fillet")              return features::FeatureType::Fillet;
    if (s == "Chamfer")             return features::FeatureType::Chamfer;
    if (s == "Shell")               return features::FeatureType::Shell;
    if (s == "Loft")                return features::FeatureType::Loft;
    if (s == "Sweep")               return features::FeatureType::Sweep;
    if (s == "Mirror")              return features::FeatureType::Mirror;
    if (s == "RectangularPattern")  return features::FeatureType::RectangularPattern;
    if (s == "CircularPattern")     return features::FeatureType::CircularPattern;
    if (s == "Sketch")              return features::FeatureType::Sketch;
    if (s == "Hole")                return features::FeatureType::Hole;
    if (s == "Combine")             return features::FeatureType::Combine;
    if (s == "SplitBody")           return features::FeatureType::SplitBody;
    if (s == "OffsetFaces")         return features::FeatureType::OffsetFaces;
    if (s == "Move")                return features::FeatureType::Move;
    if (s == "Draft")               return features::FeatureType::Draft;
    if (s == "Thicken")             return features::FeatureType::Thicken;
    if (s == "Thread")              return features::FeatureType::Thread;
    if (s == "Scale")               return features::FeatureType::Scale;
    if (s == "ConstructionPlane")   return features::FeatureType::ConstructionPlane;
    if (s == "ConstructionAxis")    return features::FeatureType::ConstructionAxis;
    if (s == "ConstructionPoint")   return features::FeatureType::ConstructionPoint;
    if (s == "PathPattern")         return features::FeatureType::PathPattern;
    if (s == "Coil")                return features::FeatureType::Coil;
    if (s == "DeleteFace")          return features::FeatureType::DeleteFace;
    if (s == "ReplaceFace")         return features::FeatureType::ReplaceFace;
    if (s == "ReverseNormal")       return features::FeatureType::ReverseNormal;
    if (s == "Joint")               return features::FeatureType::Joint;
    return features::FeatureType::BaseFeature; // fallback
}

static const char* extentTypeToStr(features::ExtentType t)
{
    switch (t) {
    case features::ExtentType::Distance:   return "Distance";
    case features::ExtentType::ThroughAll: return "ThroughAll";
    case features::ExtentType::ToEntity:   return "ToEntity";
    case features::ExtentType::Symmetric:  return "Symmetric";
    }
    return "Distance";
}

static features::ExtentType extentTypeFromStr(const std::string& s)
{
    if (s == "ThroughAll") return features::ExtentType::ThroughAll;
    if (s == "ToEntity")   return features::ExtentType::ToEntity;
    if (s == "Symmetric")  return features::ExtentType::Symmetric;
    return features::ExtentType::Distance;
}

static const char* extentDirToStr(features::ExtentDirection d)
{
    switch (d) {
    case features::ExtentDirection::Positive:  return "Positive";
    case features::ExtentDirection::Negative:  return "Negative";
    case features::ExtentDirection::Symmetric: return "Symmetric";
    }
    return "Positive";
}

static features::ExtentDirection extentDirFromStr(const std::string& s)
{
    if (s == "Negative")  return features::ExtentDirection::Negative;
    if (s == "Symmetric") return features::ExtentDirection::Symmetric;
    return features::ExtentDirection::Positive;
}

static const char* featureOpToStr(features::FeatureOperation op)
{
    switch (op) {
    case features::FeatureOperation::NewBody:      return "NewBody";
    case features::FeatureOperation::Join:         return "Join";
    case features::FeatureOperation::Cut:          return "Cut";
    case features::FeatureOperation::Intersect:    return "Intersect";
    case features::FeatureOperation::NewComponent: return "NewComponent";
    }
    return "NewBody";
}

static features::FeatureOperation featureOpFromStr(const std::string& s)
{
    if (s == "Join")         return features::FeatureOperation::Join;
    if (s == "Cut")          return features::FeatureOperation::Cut;
    if (s == "Intersect")    return features::FeatureOperation::Intersect;
    if (s == "NewComponent") return features::FeatureOperation::NewComponent;
    return features::FeatureOperation::NewBody;
}

static const char* axisTypeToStr(features::AxisType a)
{
    switch (a) {
    case features::AxisType::XAxis:  return "XAxis";
    case features::AxisType::YAxis:  return "YAxis";
    case features::AxisType::ZAxis:  return "ZAxis";
    case features::AxisType::Custom: return "Custom";
    }
    return "YAxis";
}

static features::AxisType axisTypeFromStr(const std::string& s)
{
    if (s == "XAxis")  return features::AxisType::XAxis;
    if (s == "ZAxis")  return features::AxisType::ZAxis;
    if (s == "Custom") return features::AxisType::Custom;
    return features::AxisType::YAxis;
}

static const char* chamferTypeToStr(features::ChamferType t)
{
    switch (t) {
    case features::ChamferType::EqualDistance:     return "EqualDistance";
    case features::ChamferType::TwoDistances:      return "TwoDistances";
    case features::ChamferType::DistanceAndAngle:  return "DistanceAndAngle";
    }
    return "EqualDistance";
}

static features::ChamferType chamferTypeFromStr(const std::string& s)
{
    if (s == "TwoDistances")     return features::ChamferType::TwoDistances;
    if (s == "DistanceAndAngle") return features::ChamferType::DistanceAndAngle;
    return features::ChamferType::EqualDistance;
}

static const char* holeTypeToStr(features::HoleType t)
{
    switch (t) {
    case features::HoleType::Simple:      return "Simple";
    case features::HoleType::Counterbore: return "Counterbore";
    case features::HoleType::Countersink: return "Countersink";
    }
    return "Simple";
}

static features::HoleType holeTypeFromStr(const std::string& s)
{
    if (s == "Counterbore") return features::HoleType::Counterbore;
    if (s == "Countersink") return features::HoleType::Countersink;
    return features::HoleType::Simple;
}

static const char* holeTapTypeToStr(features::HoleTapType t)
{
    switch (t) {
    case features::HoleTapType::Simple:      return "Simple";
    case features::HoleTapType::Clearance:   return "Clearance";
    case features::HoleTapType::Tapped:      return "Tapped";
    case features::HoleTapType::TaperTapped: return "TaperTapped";
    }
    return "Simple";
}

static features::HoleTapType holeTapTypeFromStr(const std::string& s)
{
    if (s == "Clearance")   return features::HoleTapType::Clearance;
    if (s == "Tapped")      return features::HoleTapType::Tapped;
    if (s == "TaperTapped") return features::HoleTapType::TaperTapped;
    return features::HoleTapType::Simple;
}

static const char* combineOpToStr(features::CombineOperation op)
{
    switch (op) {
    case features::CombineOperation::Join:      return "Join";
    case features::CombineOperation::Cut:       return "Cut";
    case features::CombineOperation::Intersect: return "Intersect";
    }
    return "Join";
}

static features::CombineOperation combineOpFromStr(const std::string& s)
{
    if (s == "Cut")       return features::CombineOperation::Cut;
    if (s == "Intersect") return features::CombineOperation::Intersect;
    return features::CombineOperation::Join;
}

static const char* moveModeToStr(features::MoveMode m)
{
    switch (m) {
    case features::MoveMode::FreeTransform:  return "FreeTransform";
    case features::MoveMode::TranslateXYZ:   return "TranslateXYZ";
    case features::MoveMode::Rotate:         return "Rotate";
    }
    return "TranslateXYZ";
}

static features::MoveMode moveModeFromStr(const std::string& s)
{
    if (s == "FreeTransform") return features::MoveMode::FreeTransform;
    if (s == "Rotate")        return features::MoveMode::Rotate;
    return features::MoveMode::TranslateXYZ;
}

static const char* scaleTypeToStr(features::ScaleType t)
{
    switch (t) {
    case features::ScaleType::Uniform:    return "Uniform";
    case features::ScaleType::NonUniform: return "NonUniform";
    }
    return "Uniform";
}

static features::ScaleType scaleTypeFromStr(const std::string& s)
{
    if (s == "NonUniform") return features::ScaleType::NonUniform;
    return features::ScaleType::Uniform;
}

static const char* threadTypeToStr(features::ThreadType t)
{
    switch (t) {
    case features::ThreadType::MetricCoarse: return "MetricCoarse";
    case features::ThreadType::MetricFine:   return "MetricFine";
    case features::ThreadType::UNC:          return "UNC";
    case features::ThreadType::UNF:          return "UNF";
    }
    return "MetricCoarse";
}

static features::ThreadType threadTypeFromStr(const std::string& s)
{
    if (s == "MetricFine") return features::ThreadType::MetricFine;
    if (s == "UNC")        return features::ThreadType::UNC;
    if (s == "UNF")        return features::ThreadType::UNF;
    return features::ThreadType::MetricCoarse;
}

static const char* planeDefTypeToStr(features::PlaneDefinitionType t)
{
    switch (t) {
    case features::PlaneDefinitionType::Standard:        return "Standard";
    case features::PlaneDefinitionType::OffsetFromPlane: return "OffsetFromPlane";
    case features::PlaneDefinitionType::AngleFromPlane:  return "AngleFromPlane";
    case features::PlaneDefinitionType::TangentToFace:   return "TangentToFace";
    case features::PlaneDefinitionType::MidPlane:        return "MidPlane";
    case features::PlaneDefinitionType::ThreePoints:     return "ThreePoints";
    }
    return "Standard";
}

static features::PlaneDefinitionType planeDefTypeFromStr(const std::string& s)
{
    if (s == "OffsetFromPlane") return features::PlaneDefinitionType::OffsetFromPlane;
    if (s == "AngleFromPlane")  return features::PlaneDefinitionType::AngleFromPlane;
    if (s == "TangentToFace")   return features::PlaneDefinitionType::TangentToFace;
    if (s == "MidPlane")        return features::PlaneDefinitionType::MidPlane;
    if (s == "ThreePoints")     return features::PlaneDefinitionType::ThreePoints;
    return features::PlaneDefinitionType::Standard;
}

static const char* axisDefTypeToStr(features::AxisDefinitionType t)
{
    switch (t) {
    case features::AxisDefinitionType::Standard:         return "Standard";
    case features::AxisDefinitionType::ThroughTwoPoints: return "ThroughTwoPoints";
    case features::AxisDefinitionType::NormalToFace:     return "NormalToFace";
    case features::AxisDefinitionType::EdgeAxis:         return "EdgeAxis";
    case features::AxisDefinitionType::Intersection:     return "Intersection";
    }
    return "Standard";
}

static features::AxisDefinitionType axisDefTypeFromStr(const std::string& s)
{
    if (s == "ThroughTwoPoints") return features::AxisDefinitionType::ThroughTwoPoints;
    if (s == "NormalToFace")     return features::AxisDefinitionType::NormalToFace;
    if (s == "EdgeAxis")         return features::AxisDefinitionType::EdgeAxis;
    if (s == "Intersection")     return features::AxisDefinitionType::Intersection;
    return features::AxisDefinitionType::Standard;
}

static const char* pointDefTypeToStr(features::PointDefinitionType t)
{
    switch (t) {
    case features::PointDefinitionType::Standard:       return "Standard";
    case features::PointDefinitionType::AtCoordinate:   return "AtCoordinate";
    case features::PointDefinitionType::CenterOfCircle: return "CenterOfCircle";
    case features::PointDefinitionType::Intersection:   return "Intersection";
    }
    return "Standard";
}

static features::PointDefinitionType pointDefTypeFromStr(const std::string& s)
{
    if (s == "AtCoordinate")   return features::PointDefinitionType::AtCoordinate;
    if (s == "CenterOfCircle") return features::PointDefinitionType::CenterOfCircle;
    if (s == "Intersection")   return features::PointDefinitionType::Intersection;
    return features::PointDefinitionType::Standard;
}

static const char* jointTypeToStr(features::JointType t)
{
    switch (t) {
    case features::JointType::Rigid:       return "Rigid";
    case features::JointType::Revolute:    return "Revolute";
    case features::JointType::Slider:      return "Slider";
    case features::JointType::Cylindrical: return "Cylindrical";
    case features::JointType::PinSlot:     return "PinSlot";
    case features::JointType::Planar:      return "Planar";
    case features::JointType::Ball:        return "Ball";
    }
    return "Rigid";
}

static features::JointType jointTypeFromStr(const std::string& s)
{
    if (s == "Revolute")    return features::JointType::Revolute;
    if (s == "Slider")      return features::JointType::Slider;
    if (s == "Cylindrical") return features::JointType::Cylindrical;
    if (s == "PinSlot")     return features::JointType::PinSlot;
    if (s == "Planar")      return features::JointType::Planar;
    if (s == "Ball")        return features::JointType::Ball;
    return features::JointType::Rigid;
}

static const char* constraintTypeToStr(sketch::ConstraintType t)
{
    switch (t) {
    case sketch::ConstraintType::Coincident:        return "Coincident";
    case sketch::ConstraintType::PointOnLine:       return "PointOnLine";
    case sketch::ConstraintType::PointOnCircle:     return "PointOnCircle";
    case sketch::ConstraintType::Distance:          return "Distance";
    case sketch::ConstraintType::DistancePointLine: return "DistancePointLine";
    case sketch::ConstraintType::Horizontal:        return "Horizontal";
    case sketch::ConstraintType::Vertical:          return "Vertical";
    case sketch::ConstraintType::Parallel:          return "Parallel";
    case sketch::ConstraintType::Perpendicular:     return "Perpendicular";
    case sketch::ConstraintType::Tangent:           return "Tangent";
    case sketch::ConstraintType::Equal:             return "Equal";
    case sketch::ConstraintType::Symmetric:         return "Symmetric";
    case sketch::ConstraintType::Midpoint:          return "Midpoint";
    case sketch::ConstraintType::Concentric:        return "Concentric";
    case sketch::ConstraintType::FixedAngle:        return "FixedAngle";
    case sketch::ConstraintType::AngleBetween:      return "AngleBetween";
    case sketch::ConstraintType::Radius:            return "Radius";
    case sketch::ConstraintType::Fix:               return "Fix";
    }
    return "Coincident";
}

static sketch::ConstraintType constraintTypeFromStr(const std::string& s)
{
    if (s == "Coincident")        return sketch::ConstraintType::Coincident;
    if (s == "PointOnLine")       return sketch::ConstraintType::PointOnLine;
    if (s == "PointOnCircle")     return sketch::ConstraintType::PointOnCircle;
    if (s == "Distance")          return sketch::ConstraintType::Distance;
    if (s == "DistancePointLine") return sketch::ConstraintType::DistancePointLine;
    if (s == "Horizontal")        return sketch::ConstraintType::Horizontal;
    if (s == "Vertical")          return sketch::ConstraintType::Vertical;
    if (s == "Parallel")          return sketch::ConstraintType::Parallel;
    if (s == "Perpendicular")     return sketch::ConstraintType::Perpendicular;
    if (s == "Tangent")           return sketch::ConstraintType::Tangent;
    if (s == "Equal")             return sketch::ConstraintType::Equal;
    if (s == "Symmetric")         return sketch::ConstraintType::Symmetric;
    if (s == "Midpoint")          return sketch::ConstraintType::Midpoint;
    if (s == "Concentric")        return sketch::ConstraintType::Concentric;
    if (s == "FixedAngle")        return sketch::ConstraintType::FixedAngle;
    if (s == "AngleBetween")      return sketch::ConstraintType::AngleBetween;
    if (s == "Radius")            return sketch::ConstraintType::Radius;
    if (s == "Fix")               return sketch::ConstraintType::Fix;
    return sketch::ConstraintType::Coincident;
}

// ── Sketch serialization ────────────────────────────────────────────────────

static void writeSketch(JsonWriter& w, const sketch::Sketch& sk)
{
    w.beginObject();

    // Points
    w.beginArray("points");
    for (const auto& [id, pt] : sk.points()) {
        w.beginObject();
        w.writeString("id", pt.id);
        w.writeNumber("x", pt.x);
        w.writeNumber("y", pt.y);
        w.writeBool("fixed", pt.isFixed);
        w.endObject();
    }
    w.endArray();

    // Lines
    w.beginArray("lines");
    for (const auto& [id, ln] : sk.lines()) {
        w.beginObject();
        w.writeString("id", ln.id);
        w.writeString("start", ln.startPointId);
        w.writeString("end", ln.endPointId);
        w.writeBool("construction", ln.isConstruction);
        w.writeBool("centerLine", ln.isCenterLine);
        w.endObject();
    }
    w.endArray();

    // Circles
    w.beginArray("circles");
    for (const auto& [id, circ] : sk.circles()) {
        w.beginObject();
        w.writeString("id", circ.id);
        w.writeString("center", circ.centerPointId);
        w.writeNumber("radius", circ.radius);
        w.writeBool("construction", circ.isConstruction);
        w.endObject();
    }
    w.endArray();

    // Arcs
    w.beginArray("arcs");
    for (const auto& [id, arc] : sk.arcs()) {
        w.beginObject();
        w.writeString("id", arc.id);
        w.writeString("center", arc.centerPointId);
        w.writeString("start", arc.startPointId);
        w.writeString("end", arc.endPointId);
        w.writeNumber("radius", arc.radius);
        w.writeBool("construction", arc.isConstruction);
        w.endObject();
    }
    w.endArray();

    // Splines
    w.beginArray("splines");
    for (const auto& [id, spl] : sk.splines()) {
        w.beginObject();
        w.writeString("id", spl.id);
        w.writeInt("degree", spl.degree);
        w.writeBool("closed", spl.isClosed);
        w.writeBool("construction", spl.isConstruction);
        w.beginArray("controlPoints");
        for (const auto& cpId : spl.controlPointIds) {
            w.beginObject();
            w.writeString("ref", cpId);
            w.endObject();
        }
        w.endArray();
        w.endObject();
    }
    w.endArray();

    // Ellipses
    w.beginArray("ellipses");
    for (const auto& [id, ell] : sk.ellipses()) {
        w.beginObject();
        w.writeString("id", ell.id);
        w.writeString("center", ell.centerPointId);
        w.writeNumber("majorRadius", ell.majorRadius);
        w.writeNumber("minorRadius", ell.minorRadius);
        w.writeNumber("rotation", ell.rotationAngle);
        w.writeBool("construction", ell.isConstruction);
        w.endObject();
    }
    w.endArray();

    // Constraints
    w.beginArray("constraints");
    for (const auto& [id, con] : sk.constraints()) {
        w.beginObject();
        w.writeString("id", con.id);
        w.writeString("type", constraintTypeToStr(con.type));
        w.beginArray("entities");
        for (const auto& eid : con.entityIds) {
            // Write bare string elements in array
            w.beginObject();
            w.writeString("ref", eid);
            w.endObject();
        }
        w.endArray();
        w.writeNumber("value", con.value);
        w.endObject();
    }
    w.endArray();

    w.endObject();
}

// ── Feature params serialization ────────────────────────────────────────────

static void writeFeatureParams(JsonWriter& w,
                                const features::Feature& feat,
                                const features::SketchFeature* sketchFeat)
{
    auto ft = feat.type();

    w.writeKey("params");
    w.beginObject();

    if (ft == features::FeatureType::Extrude) {
        const auto& p = static_cast<const features::ExtrudeFeature&>(feat).params();
        w.writeString("profileId", p.profileId);
        w.writeString("sketchId", p.sketchId);
        w.writeString("distanceExpr", p.distanceExpr);
        w.writeString("extentType", extentTypeToStr(p.extentType));
        w.writeString("direction", extentDirToStr(p.direction));
        w.writeString("operation", featureOpToStr(p.operation));
        w.writeBool("isSymmetric", p.isSymmetric);
        w.writeNumber("taperAngleDeg", p.taperAngleDeg);
        w.writeString("targetBodyId", p.targetBodyId);
        w.writeString("distance2Expr", p.distance2Expr);
        w.writeNumber("taperAngle2Deg", p.taperAngle2Deg);
        w.writeBool("isThinExtrude", p.isThinExtrude);
        w.writeNumber("wallThickness", p.wallThickness);
    }
    else if (ft == features::FeatureType::Revolve) {
        const auto& p = static_cast<const features::RevolveFeature&>(feat).params();
        w.writeString("profileId", p.profileId);
        w.writeString("sketchId", p.sketchId);
        w.writeString("axisType", axisTypeToStr(p.axisType));
        w.writeString("angleExpr", p.angleExpr);
        w.writeBool("isFullRevolution", p.isFullRevolution);
        w.writeBool("isProjectAxis", p.isProjectAxis);
        w.writeString("operation", featureOpToStr(p.operation));
        w.writeString("angle2Expr", p.angle2Expr);
    }
    else if (ft == features::FeatureType::Fillet) {
        const auto& p = static_cast<const features::FilletFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.beginArray("edgeIds");
        for (int eid : p.edgeIds) {
            w.beginObject();
            w.writeInt("v", eid);
            w.endObject();
        }
        w.endArray();
        // Stable edge signatures
        w.beginArray("edgeSignatures");
        for (const auto& sig : p.edgeSignatures) {
            w.beginObject();
            w.writeString("v", sig.toString());
            w.endObject();
        }
        w.endArray();
        w.writeString("radiusExpr", p.radiusExpr);
        w.writeBool("isVariableRadius", p.isVariableRadius);
        w.writeBool("isG2", p.isG2);
        w.writeBool("isTangentChain", p.isTangentChain);
        w.writeBool("isRollingBallCorner", p.isRollingBallCorner);
    }
    else if (ft == features::FeatureType::Chamfer) {
        const auto& p = static_cast<const features::ChamferFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.beginArray("edgeIds");
        for (int eid : p.edgeIds) {
            w.beginObject();
            w.writeInt("v", eid);
            w.endObject();
        }
        w.endArray();
        // Stable edge signatures
        w.beginArray("edgeSignatures");
        for (const auto& sig : p.edgeSignatures) {
            w.beginObject();
            w.writeString("v", sig.toString());
            w.endObject();
        }
        w.endArray();
        w.writeString("chamferType", chamferTypeToStr(p.chamferType));
        w.writeString("distanceExpr", p.distanceExpr);
        w.writeString("distance2Expr", p.distance2Expr);
        w.writeNumber("angleDeg", p.angleDeg);
        w.writeBool("isTangentChain", p.isTangentChain);
    }
    else if (ft == features::FeatureType::Shell) {
        const auto& p = static_cast<const features::ShellFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.writeNumber("thicknessExpr", p.thicknessExpr);
        w.beginArray("removedFaceIds");
        for (int fid : p.removedFaceIds) {
            w.beginObject();
            w.writeInt("v", fid);
            w.endObject();
        }
        w.endArray();
        // Stable face signatures
        w.beginArray("faceSignatures");
        for (const auto& sig : p.faceSignatures) {
            w.beginObject();
            w.writeString("v", sig.toString());
            w.endObject();
        }
        w.endArray();
    }
    else if (ft == features::FeatureType::Sweep) {
        const auto& p = static_cast<const features::SweepFeature&>(feat).params();
        w.writeString("profileId", p.profileId);
        w.writeString("sketchId", p.sketchId);
        w.writeString("pathId", p.pathId);
        w.writeString("pathSketchId", p.pathSketchId);
        w.writeString("operation", featureOpToStr(p.operation));
        w.writeBool("isPerpendicularOrientation", p.isPerpendicularOrientation);
    }
    else if (ft == features::FeatureType::Loft) {
        const auto& p = static_cast<const features::LoftFeature&>(feat).params();
        w.beginArray("sectionIds");
        for (const auto& s : p.sectionIds) {
            w.beginObject();
            w.writeString("v", s);
            w.endObject();
        }
        w.endArray();
        w.beginArray("sectionSketchIds");
        for (const auto& s : p.sectionSketchIds) {
            w.beginObject();
            w.writeString("v", s);
            w.endObject();
        }
        w.endArray();
        w.writeBool("isClosed", p.isClosed);
        w.writeString("operation", featureOpToStr(p.operation));
    }
    else if (ft == features::FeatureType::Mirror) {
        const auto& p = static_cast<const features::MirrorFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.writeNumber("planeOx", p.planeOx);
        w.writeNumber("planeOy", p.planeOy);
        w.writeNumber("planeOz", p.planeOz);
        w.writeNumber("planeNx", p.planeNx);
        w.writeNumber("planeNy", p.planeNy);
        w.writeNumber("planeNz", p.planeNz);
        w.writeBool("isCombine", p.isCombine);
    }
    else if (ft == features::FeatureType::RectangularPattern) {
        const auto& p = static_cast<const features::RectangularPatternFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.writeNumber("dir1X", p.dir1X);
        w.writeNumber("dir1Y", p.dir1Y);
        w.writeNumber("dir1Z", p.dir1Z);
        w.writeString("spacing1Expr", p.spacing1Expr);
        w.writeInt("count1", p.count1);
        w.writeNumber("dir2X", p.dir2X);
        w.writeNumber("dir2Y", p.dir2Y);
        w.writeNumber("dir2Z", p.dir2Z);
        w.writeString("spacing2Expr", p.spacing2Expr);
        w.writeInt("count2", p.count2);
        w.writeString("operation", featureOpToStr(p.operation));
    }
    else if (ft == features::FeatureType::CircularPattern) {
        const auto& p = static_cast<const features::CircularPatternFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.writeNumber("axisOx", p.axisOx);
        w.writeNumber("axisOy", p.axisOy);
        w.writeNumber("axisOz", p.axisOz);
        w.writeNumber("axisDx", p.axisDx);
        w.writeNumber("axisDy", p.axisDy);
        w.writeNumber("axisDz", p.axisDz);
        w.writeInt("count", p.count);
        w.writeNumber("totalAngleDeg", p.totalAngleDeg);
        w.writeString("operation", featureOpToStr(p.operation));
    }
    else if (ft == features::FeatureType::Sketch) {
        if (sketchFeat) {
            const auto& p = sketchFeat->params();
            w.writeString("planeId", p.planeId);
            w.writeNumber("originX", p.originX);
            w.writeNumber("originY", p.originY);
            w.writeNumber("originZ", p.originZ);
            w.writeNumber("xDirX", p.xDirX);
            w.writeNumber("xDirY", p.xDirY);
            w.writeNumber("xDirZ", p.xDirZ);
            w.writeNumber("yDirX", p.yDirX);
            w.writeNumber("yDirY", p.yDirY);
            w.writeNumber("yDirZ", p.yDirZ);
        }
    }
    else if (ft == features::FeatureType::Hole) {
        const auto& p = static_cast<const features::HoleFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.writeString("holeType", holeTypeToStr(p.holeType));
        w.writeNumber("posX", p.posX);
        w.writeNumber("posY", p.posY);
        w.writeNumber("posZ", p.posZ);
        w.writeNumber("dirX", p.dirX);
        w.writeNumber("dirY", p.dirY);
        w.writeNumber("dirZ", p.dirZ);
        w.writeString("diameterExpr", p.diameterExpr);
        w.writeString("depthExpr", p.depthExpr);
        w.writeNumber("tipAngleDeg", p.tipAngleDeg);
        w.writeString("cboreDiameterExpr", p.cboreDiameterExpr);
        w.writeString("cboreDepthExpr", p.cboreDepthExpr);
        w.writeString("csinkDiameterExpr", p.csinkDiameterExpr);
        w.writeNumber("csinkAngleDeg", p.csinkAngleDeg);
        w.writeString("tapType", holeTapTypeToStr(p.tapType));
    }
    else if (ft == features::FeatureType::Combine) {
        const auto& p = static_cast<const features::CombineFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.writeString("toolBodyId", p.toolBodyId);
        w.writeString("operation", combineOpToStr(p.operation));
        w.writeBool("keepToolBody", p.keepToolBody);
    }
    else if (ft == features::FeatureType::SplitBody) {
        const auto& p = static_cast<const features::SplitBodyFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.writeString("splittingToolId", p.splittingToolId);
        w.writeNumber("planeOx", p.planeOx);
        w.writeNumber("planeOy", p.planeOy);
        w.writeNumber("planeOz", p.planeOz);
        w.writeNumber("planeNx", p.planeNx);
        w.writeNumber("planeNy", p.planeNy);
        w.writeNumber("planeNz", p.planeNz);
        w.writeBool("usePlane", p.usePlane);
    }
    else if (ft == features::FeatureType::OffsetFaces) {
        const auto& p = static_cast<const features::OffsetFacesFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.beginArray("faceIndices");
        for (int fi : p.faceIndices) {
            w.beginObject();
            w.writeInt("v", fi);
            w.endObject();
        }
        w.endArray();
        // Stable face signatures
        w.beginArray("faceSignatures");
        for (const auto& sig : p.faceSignatures) {
            w.beginObject();
            w.writeString("v", sig.toString());
            w.endObject();
        }
        w.endArray();
        w.writeNumber("distance", p.distance);
    }
    else if (ft == features::FeatureType::Move) {
        const auto& p = static_cast<const features::MoveFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.writeString("mode", moveModeToStr(p.mode));
        w.writeNumber("dx", p.dx);
        w.writeNumber("dy", p.dy);
        w.writeNumber("dz", p.dz);
        w.writeNumber("axisOx", p.axisOx);
        w.writeNumber("axisOy", p.axisOy);
        w.writeNumber("axisOz", p.axisOz);
        w.writeNumber("axisDx", p.axisDx);
        w.writeNumber("axisDy", p.axisDy);
        w.writeNumber("axisDz", p.axisDz);
        w.writeNumber("angleDeg", p.angleDeg);
        w.beginArray("matrix");
        for (int i = 0; i < 16; ++i) {
            w.beginObject();
            w.writeNumber("v", p.matrix[i]);
            w.endObject();
        }
        w.endArray();
        w.writeBool("createCopy", p.createCopy);
    }
    else if (ft == features::FeatureType::Draft) {
        const auto& p = static_cast<const features::DraftFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.beginArray("faceIndices");
        for (int fi : p.faceIndices) {
            w.beginObject();
            w.writeInt("v", fi);
            w.endObject();
        }
        w.endArray();
        // Stable face signatures
        w.beginArray("faceSignatures");
        for (const auto& sig : p.faceSignatures) {
            w.beginObject();
            w.writeString("v", sig.toString());
            w.endObject();
        }
        w.endArray();
        w.writeString("angleExpr", p.angleExpr);
        w.writeNumber("pullDirX", p.pullDirX);
        w.writeNumber("pullDirY", p.pullDirY);
        w.writeNumber("pullDirZ", p.pullDirZ);
    }
    else if (ft == features::FeatureType::Thicken) {
        const auto& p = static_cast<const features::ThickenFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.writeString("thicknessExpr", p.thicknessExpr);
        w.writeBool("isSymmetric", p.isSymmetric);
    }
    else if (ft == features::FeatureType::Thread) {
        const auto& p = static_cast<const features::ThreadFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.writeInt("cylindricalFaceIndex", p.cylindricalFaceIndex);
        // Stable face signatures
        w.beginArray("faceSignatures");
        for (const auto& sig : p.faceSignatures) {
            w.beginObject();
            w.writeString("v", sig.toString());
            w.endObject();
        }
        w.endArray();
        w.writeString("threadType", threadTypeToStr(p.threadType));
        w.writeNumber("pitch", p.pitch);
        w.writeNumber("depth", p.depth);
        w.writeBool("isInternal", p.isInternal);
        w.writeBool("isRightHanded", p.isRightHanded);
        w.writeBool("isModeled", p.isModeled);
        w.writeBool("isFullLength", p.isFullLength);
        w.writeNumber("threadLength", p.threadLength);
        w.writeNumber("threadOffset", p.threadOffset);
    }
    else if (ft == features::FeatureType::Scale) {
        const auto& p = static_cast<const features::ScaleFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.writeString("scaleType", scaleTypeToStr(p.scaleType));
        w.writeNumber("factor", p.factor);
        w.writeNumber("factorX", p.factorX);
        w.writeNumber("factorY", p.factorY);
        w.writeNumber("factorZ", p.factorZ);
        w.writeNumber("centerX", p.centerX);
        w.writeNumber("centerY", p.centerY);
        w.writeNumber("centerZ", p.centerZ);
    }
    else if (ft == features::FeatureType::ConstructionPlane) {
        const auto& p = static_cast<const features::ConstructionPlane&>(feat).params();
        w.writeString("definitionType", planeDefTypeToStr(p.definitionType));
        w.writeString("standardPlane", p.standardPlane);
        w.writeString("parentPlaneId", p.parentPlaneId);
        w.writeNumber("offsetDistance", p.offsetDistance);
        w.writeNumber("angleDeg", p.angleDeg);
        w.writeNumber("originX", p.originX);
        w.writeNumber("originY", p.originY);
        w.writeNumber("originZ", p.originZ);
        w.writeNumber("normalX", p.normalX);
        w.writeNumber("normalY", p.normalY);
        w.writeNumber("normalZ", p.normalZ);
        w.writeNumber("xDirX", p.xDirX);
        w.writeNumber("xDirY", p.xDirY);
        w.writeNumber("xDirZ", p.xDirZ);
    }
    else if (ft == features::FeatureType::ConstructionAxis) {
        const auto& p = static_cast<const features::ConstructionAxis&>(feat).params();
        w.writeString("definitionType", axisDefTypeToStr(p.definitionType));
        w.writeString("standardAxis", p.standardAxis);
        w.writeNumber("originX", p.originX);
        w.writeNumber("originY", p.originY);
        w.writeNumber("originZ", p.originZ);
        w.writeNumber("dirX", p.dirX);
        w.writeNumber("dirY", p.dirY);
        w.writeNumber("dirZ", p.dirZ);
        w.writeString("point1Id", p.point1Id);
        w.writeString("point2Id", p.point2Id);
        w.writeString("plane1Id", p.plane1Id);
        w.writeString("plane2Id", p.plane2Id);
    }
    else if (ft == features::FeatureType::ConstructionPoint) {
        const auto& p = static_cast<const features::ConstructionPoint&>(feat).params();
        w.writeString("definitionType", pointDefTypeToStr(p.definitionType));
        w.writeNumber("x", p.x);
        w.writeNumber("y", p.y);
        w.writeNumber("z", p.z);
        w.writeString("lineId", p.lineId);
        w.writeString("planeId", p.planeId);
        w.writeString("circleId", p.circleId);
    }
    else if (ft == features::FeatureType::PathPattern) {
        const auto& p = static_cast<const features::PathPatternFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.writeString("pathBodyId", p.pathBodyId);
        w.writeInt("count", p.count);
        w.writeNumber("startOffset", p.startOffset);
        w.writeNumber("endOffset", p.endOffset);
        w.writeString("operation", featureOpToStr(p.operation));
    }
    else if (ft == features::FeatureType::Coil) {
        const auto& p = static_cast<const features::CoilFeature&>(feat).params();
        w.writeString("profileBodyId", p.profileBodyId);
        w.writeNumber("axisOx", p.axisOx);
        w.writeNumber("axisOy", p.axisOy);
        w.writeNumber("axisOz", p.axisOz);
        w.writeNumber("axisDx", p.axisDx);
        w.writeNumber("axisDy", p.axisDy);
        w.writeNumber("axisDz", p.axisDz);
        w.writeNumber("radius", p.radius);
        w.writeNumber("pitch", p.pitch);
        w.writeNumber("turns", p.turns);
        w.writeNumber("taperAngleDeg", p.taperAngleDeg);
    }
    else if (ft == features::FeatureType::DeleteFace) {
        const auto& p = static_cast<const features::DeleteFaceFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.beginArray("faceIndices");
        for (int fi : p.faceIndices) {
            w.beginObject();
            w.writeInt("v", fi);
            w.endObject();
        }
        w.endArray();
        w.beginArray("faceSignatures");
        for (const auto& sig : p.faceSignatures) {
            w.beginObject();
            w.writeString("v", sig.toString());
            w.endObject();
        }
        w.endArray();
    }
    else if (ft == features::FeatureType::ReplaceFace) {
        const auto& p = static_cast<const features::ReplaceFaceFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.writeInt("faceIndex", p.faceIndex);
        w.writeString("replacementBodyId", p.replacementBodyId);
    }
    else if (ft == features::FeatureType::ReverseNormal) {
        const auto& p = static_cast<const features::ReverseNormalFeature&>(feat).params();
        w.writeString("targetBodyId", p.targetBodyId);
        w.beginArray("faceIndices");
        for (int fi : p.faceIndices) {
            w.beginObject();
            w.writeInt("v", fi);
            w.endObject();
        }
        w.endArray();
        w.beginArray("faceSignatures");
        for (const auto& sig : p.faceSignatures) {
            w.beginObject();
            w.writeString("v", sig.toString());
            w.endObject();
        }
        w.endArray();
    }
    else if (ft == features::FeatureType::Joint) {
        const auto& p = static_cast<const features::Joint&>(feat).params();
        w.writeString("occurrenceOneId", p.occurrenceOneId);
        w.writeString("occurrenceTwoId", p.occurrenceTwoId);
        w.writeString("jointType", jointTypeToStr(p.jointType));
        // Geometry one
        w.writeNumber("g1OriginX", p.geometryOne.originX);
        w.writeNumber("g1OriginY", p.geometryOne.originY);
        w.writeNumber("g1OriginZ", p.geometryOne.originZ);
        w.writeNumber("g1PrimaryAxisX", p.geometryOne.primaryAxisX);
        w.writeNumber("g1PrimaryAxisY", p.geometryOne.primaryAxisY);
        w.writeNumber("g1PrimaryAxisZ", p.geometryOne.primaryAxisZ);
        w.writeNumber("g1SecondaryAxisX", p.geometryOne.secondaryAxisX);
        w.writeNumber("g1SecondaryAxisY", p.geometryOne.secondaryAxisY);
        w.writeNumber("g1SecondaryAxisZ", p.geometryOne.secondaryAxisZ);
        // Geometry two
        w.writeNumber("g2OriginX", p.geometryTwo.originX);
        w.writeNumber("g2OriginY", p.geometryTwo.originY);
        w.writeNumber("g2OriginZ", p.geometryTwo.originZ);
        w.writeNumber("g2PrimaryAxisX", p.geometryTwo.primaryAxisX);
        w.writeNumber("g2PrimaryAxisY", p.geometryTwo.primaryAxisY);
        w.writeNumber("g2PrimaryAxisZ", p.geometryTwo.primaryAxisZ);
        w.writeNumber("g2SecondaryAxisX", p.geometryTwo.secondaryAxisX);
        w.writeNumber("g2SecondaryAxisY", p.geometryTwo.secondaryAxisY);
        w.writeNumber("g2SecondaryAxisZ", p.geometryTwo.secondaryAxisZ);
        // Motion values
        w.writeNumber("rotationValue", p.rotationValue);
        w.writeNumber("translationValue", p.translationValue);
        w.writeNumber("rotation2Value", p.rotation2Value);
        w.writeNumber("translation2Value", p.translation2Value);
        w.writeNumber("rotation3Value", p.rotation3Value);
        // Rotation limits
        w.writeBool("rotLimHasMin", p.rotationLimits.hasMin);
        w.writeBool("rotLimHasMax", p.rotationLimits.hasMax);
        w.writeNumber("rotLimMin", p.rotationLimits.minValue);
        w.writeNumber("rotLimMax", p.rotationLimits.maxValue);
        w.writeNumber("rotLimRest", p.rotationLimits.restValue);
        // Translation limits
        w.writeBool("transLimHasMin", p.translationLimits.hasMin);
        w.writeBool("transLimHasMax", p.translationLimits.hasMax);
        w.writeNumber("transLimMin", p.translationLimits.minValue);
        w.writeNumber("transLimMax", p.translationLimits.maxValue);
        w.writeNumber("transLimRest", p.translationLimits.restValue);
        // Flags
        w.writeBool("isFlipped", p.isFlipped);
        w.writeBool("isLocked", p.isLocked);
        w.writeBool("isSuppressed", p.isSuppressed);
    }

    w.endObject();
}

// ── save ────────────────────────────────────────────────────────────────────

bool Serializer::save(const Document& doc, const std::string& path)
{
    JsonWriter w;
    w.beginObject();

    w.writeInt("version", 1);
    w.writeString("name", doc.name());

    // Parameters
    w.writeKey("parameters");
    w.beginObject();
    for (const auto& [name, param] : doc.parameters().all()) {
        w.writeKey(name);
        w.beginObject();
        w.writeString("expression", param.expression);
        w.writeString("unit", param.unit);
        w.writeString("comment", param.comment);
        w.endObject();
    }
    w.endObject();

    // Timeline
    const auto& tl = doc.timeline();
    w.beginArray("timeline");
    for (size_t i = 0; i < tl.count(); ++i) {
        const auto& entry = tl.entry(i);
        if (!entry.feature)
            continue;

        w.beginObject();
        w.writeString("id", entry.id);
        w.writeString("type", featureTypeToStr(entry.feature->type()));
        w.writeBool("isSuppressed", entry.isSuppressed);
        if (!entry.customName.empty())
            w.writeString("customName", entry.customName);

        // Write feature-specific params
        const features::SketchFeature* sketchFeat = nullptr;
        if (entry.feature->type() == features::FeatureType::Sketch)
            sketchFeat = static_cast<const features::SketchFeature*>(entry.feature.get());

        writeFeatureParams(w, *entry.feature, sketchFeat);

        // For sketch features, embed the sketch geometry
        if (sketchFeat) {
            w.writeKey("sketch");
            writeSketch(w, sketchFeat->sketch());
        }

        w.endObject();
    }
    w.endArray();

    w.writeInt("markerPosition", static_cast<int>(tl.markerPosition()));

    // Timeline groups
    const auto& groups = tl.groups();
    w.beginArray("groups");
    for (const auto& g : groups) {
        w.beginObject();
        w.writeString("id", g.id);
        w.writeString("name", g.name);
        w.writeInt("startIndex", static_cast<int>(g.startIndex));
        w.writeInt("endIndex", static_cast<int>(g.endIndex));
        w.writeBool("isCollapsed", g.isCollapsed);
        w.writeBool("isSuppressed", g.isSuppressed);
        w.endObject();
    }
    w.endArray();

    // Material / appearance assignments
    const auto& appearances = doc.appearances();
    w.beginArray("materials");
    for (const auto& [bodyId, mat] : appearances.bodyMaterials()) {
        w.beginObject();
        w.writeString("bodyId", bodyId);
        w.writeString("name", mat.name);
        w.writeNumber("baseR", mat.baseR);
        w.writeNumber("baseG", mat.baseG);
        w.writeNumber("baseB", mat.baseB);
        w.writeNumber("opacity", mat.opacity);
        w.writeNumber("metallic", mat.metallic);
        w.writeNumber("roughness", mat.roughness);
        w.writeNumber("density", mat.density);
        w.endObject();
    }
    w.endArray();

    w.beginArray("faceMaterials");
    for (const auto& [bodyId, faceMap] : appearances.faceMaterials()) {
        for (const auto& [faceIdx, mat] : faceMap) {
            w.beginObject();
            w.writeString("bodyId", bodyId);
            w.writeInt("faceIndex", faceIdx);
            w.writeString("name", mat.name);
            w.writeNumber("baseR", mat.baseR);
            w.writeNumber("baseG", mat.baseG);
            w.writeNumber("baseB", mat.baseB);
            w.writeNumber("opacity", mat.opacity);
            w.writeNumber("metallic", mat.metallic);
            w.writeNumber("roughness", mat.roughness);
            w.writeNumber("density", mat.density);
            w.endObject();
        }
    }
    w.endArray();

    w.endObject();

    // Write to file
    std::ofstream out(path, std::ios::binary);
    if (!out)
        return false;
    out << w.result();
    return out.good();
}

// ── Sketch deserialization ──────────────────────────────────────────────────

static void deserializeSketch(const JsonValue& obj, sketch::Sketch& sk)
{
    // The sketch is freshly constructed with plane already set.
    // We add entities in order and build an old-ID -> new-ID mapping.
    std::unordered_map<std::string, std::string> idMap;

    // Points
    const auto* points = obj.getArray("points");
    if (points) {
        for (const auto& elem : points->arrayVal) {
            if (!elem || elem->type != JsonValue::Type::Object) continue;
            std::string oldId = elem->getString("id");
            double x = elem->getNumber("x");
            double y = elem->getNumber("y");
            bool fixed = elem->getBool("fixed");
            std::string newId = sk.addPoint(x, y, fixed);
            idMap[oldId] = newId;
        }
    }

    auto mapId = [&](const std::string& oldId) -> std::string {
        auto it = idMap.find(oldId);
        return (it != idMap.end()) ? it->second : oldId;
    };

    // Lines
    const auto* lines = obj.getArray("lines");
    if (lines) {
        for (const auto& elem : lines->arrayVal) {
            if (!elem || elem->type != JsonValue::Type::Object) continue;
            std::string oldId = elem->getString("id");
            std::string startId = mapId(elem->getString("start"));
            std::string endId = mapId(elem->getString("end"));
            bool construction = elem->getBool("construction");
            std::string newId = sk.addLine(startId, endId, construction);
            idMap[oldId] = newId;
        }
    }

    // Circles
    const auto* circles = obj.getArray("circles");
    if (circles) {
        for (const auto& elem : circles->arrayVal) {
            if (!elem || elem->type != JsonValue::Type::Object) continue;
            std::string oldId = elem->getString("id");
            std::string centerId = mapId(elem->getString("center"));
            double radius = elem->getNumber("radius");
            bool construction = elem->getBool("construction");
            std::string newId = sk.addCircle(centerId, radius, construction);
            idMap[oldId] = newId;
        }
    }

    // Arcs
    const auto* arcs = obj.getArray("arcs");
    if (arcs) {
        for (const auto& elem : arcs->arrayVal) {
            if (!elem || elem->type != JsonValue::Type::Object) continue;
            std::string oldId = elem->getString("id");
            std::string centerId = mapId(elem->getString("center"));
            std::string startId = mapId(elem->getString("start"));
            std::string endId = mapId(elem->getString("end"));
            double radius = elem->getNumber("radius");
            bool construction = elem->getBool("construction");
            std::string newId = sk.addArc(centerId, startId, endId, radius, construction);
            idMap[oldId] = newId;
        }
    }

    // Splines
    const auto* splines = obj.getArray("splines");
    if (splines) {
        for (const auto& elem : splines->arrayVal) {
            if (!elem || elem->type != JsonValue::Type::Object) continue;
            std::string oldId = elem->getString("id");
            int degree = elem->getInt("degree", 3);
            bool closed = elem->getBool("closed");
            bool construction = elem->getBool("construction");

            std::vector<std::string> cpIds;
            const auto* cpArr = elem->getArray("controlPoints");
            if (cpArr) {
                for (const auto& cpRef : cpArr->arrayVal) {
                    if (cpRef && cpRef->type == JsonValue::Type::Object)
                        cpIds.push_back(mapId(cpRef->getString("ref")));
                }
            }

            if (cpIds.size() >= 2) {
                std::string newId = sk.addSpline(cpIds, degree, closed);
                sk.spline(newId).isConstruction = construction;
                idMap[oldId] = newId;
            }
        }
    }

    // Ellipses
    const auto* ellipses = obj.getArray("ellipses");
    if (ellipses) {
        for (const auto& elem : ellipses->arrayVal) {
            if (!elem || elem->type != JsonValue::Type::Object) continue;
            std::string oldId = elem->getString("id");
            std::string centerId = mapId(elem->getString("center"));
            double majorR = elem->getNumber("majorRadius");
            double minorR = elem->getNumber("minorRadius");
            double rotation = elem->getNumber("rotation");
            bool construction = elem->getBool("construction");
            std::string newId = sk.addEllipse(centerId, majorR, minorR, rotation, construction);
            idMap[oldId] = newId;
        }
    }

    // Constraints
    const auto* constraints = obj.getArray("constraints");
    if (constraints) {
        for (const auto& elem : constraints->arrayVal) {
            if (!elem || elem->type != JsonValue::Type::Object) continue;
            auto cType = constraintTypeFromStr(elem->getString("type"));
            double value = elem->getNumber("value");

            std::vector<std::string> entityIds;
            const auto* entities = elem->getArray("entities");
            if (entities) {
                for (const auto& eRef : entities->arrayVal) {
                    if (eRef && eRef->type == JsonValue::Type::Object)
                        entityIds.push_back(mapId(eRef->getString("ref")));
                }
            }

            sk.addConstraint(cType, entityIds, value);
        }
    }
}

// Helper to read a vector<int> stored as array of {"v":N} objects
static std::vector<int> readIntArray(const JsonValue& obj, const std::string& key)
{
    std::vector<int> result;
    const auto* arr = obj.getArray(key);
    if (arr) {
        for (const auto& elem : arr->arrayVal) {
            if (elem && elem->type == JsonValue::Type::Object)
                result.push_back(elem->getInt("v"));
        }
    }
    return result;
}

// Helper to read a vector<string> stored as array of {"v":"..."} objects
static std::vector<std::string> readStringArray(const JsonValue& obj, const std::string& key)
{
    std::vector<std::string> result;
    const auto* arr = obj.getArray(key);
    if (arr) {
        for (const auto& elem : arr->arrayVal) {
            if (elem && elem->type == JsonValue::Type::Object)
                result.push_back(elem->getString("v"));
        }
    }
    return result;
}

// Helper to read edge signatures stored as array of {"v":"..."} objects
static std::vector<kernel::EdgeSignature> readEdgeSignatures(const JsonValue& obj, const std::string& key)
{
    std::vector<kernel::EdgeSignature> result;
    const auto* arr = obj.getArray(key);
    if (arr) {
        for (const auto& elem : arr->arrayVal) {
            if (elem && elem->type == JsonValue::Type::Object) {
                result.push_back(kernel::EdgeSignature::fromString(elem->getString("v")));
            }
        }
    }
    return result;
}

// Helper to read face signatures stored as array of {"v":"..."} objects
static std::vector<kernel::FaceSignature> readFaceSignatures(const JsonValue& obj, const std::string& key)
{
    std::vector<kernel::FaceSignature> result;
    const auto* arr = obj.getArray(key);
    if (arr) {
        for (const auto& elem : arr->arrayVal) {
            if (elem && elem->type == JsonValue::Type::Object) {
                result.push_back(kernel::FaceSignature::fromString(elem->getString("v")));
            }
        }
    }
    return result;
}

// ── Feature reconstruction from JSON ────────────────────────────────────────

static std::shared_ptr<features::Feature> reconstructFeature(
    const std::string& id,
    features::FeatureType ft,
    const JsonValue& params,
    const JsonValue* sketchObj)
{
    switch (ft) {
    case features::FeatureType::Extrude: {
        features::ExtrudeParams p;
        p.profileId     = params.getString("profileId");
        p.sketchId      = params.getString("sketchId");
        p.distanceExpr  = params.getString("distanceExpr");
        p.extentType    = extentTypeFromStr(params.getString("extentType", "Distance"));
        p.direction     = extentDirFromStr(params.getString("direction", "Positive"));
        p.operation     = featureOpFromStr(params.getString("operation", "NewBody"));
        p.isSymmetric   = params.getBool("isSymmetric");
        p.taperAngleDeg = params.getNumber("taperAngleDeg");
        p.targetBodyId  = params.getString("targetBodyId");
        p.distance2Expr = params.getString("distance2Expr");
        p.taperAngle2Deg = params.getNumber("taperAngle2Deg");
        p.isThinExtrude = params.getBool("isThinExtrude");
        p.wallThickness = params.getNumber("wallThickness", 1.0);
        return std::make_shared<features::ExtrudeFeature>(id, std::move(p));
    }
    case features::FeatureType::Revolve: {
        features::RevolveParams p;
        p.profileId        = params.getString("profileId");
        p.sketchId         = params.getString("sketchId");
        p.axisType         = axisTypeFromStr(params.getString("axisType", "YAxis"));
        p.angleExpr        = params.getString("angleExpr");
        p.isFullRevolution = params.getBool("isFullRevolution", true);
        p.isProjectAxis    = params.getBool("isProjectAxis");
        p.operation        = featureOpFromStr(params.getString("operation", "NewBody"));
        p.angle2Expr       = params.getString("angle2Expr");
        return std::make_shared<features::RevolveFeature>(id, std::move(p));
    }
    case features::FeatureType::Fillet: {
        features::FilletParams p;
        p.targetBodyId      = params.getString("targetBodyId");
        p.edgeIds           = readIntArray(params, "edgeIds");
        p.edgeSignatures    = readEdgeSignatures(params, "edgeSignatures");
        p.radiusExpr        = params.getString("radiusExpr");
        p.isVariableRadius  = params.getBool("isVariableRadius");
        p.isG2              = params.getBool("isG2");
        p.isTangentChain    = params.getBool("isTangentChain", true);
        p.isRollingBallCorner = params.getBool("isRollingBallCorner", true);
        return std::make_shared<features::FilletFeature>(id, std::move(p));
    }
    case features::FeatureType::Chamfer: {
        features::ChamferParams p;
        p.targetBodyId  = params.getString("targetBodyId");
        p.edgeIds       = readIntArray(params, "edgeIds");
        p.edgeSignatures = readEdgeSignatures(params, "edgeSignatures");
        p.chamferType   = chamferTypeFromStr(params.getString("chamferType", "EqualDistance"));
        p.distanceExpr  = params.getString("distanceExpr");
        p.distance2Expr = params.getString("distance2Expr");
        p.angleDeg      = params.getNumber("angleDeg", 45.0);
        p.isTangentChain = params.getBool("isTangentChain", true);
        return std::make_shared<features::ChamferFeature>(id, std::move(p));
    }
    case features::FeatureType::Shell: {
        features::ShellParams p;
        p.targetBodyId   = params.getString("targetBodyId");
        p.thicknessExpr  = params.getNumber("thicknessExpr", 2.0);
        p.removedFaceIds = readIntArray(params, "removedFaceIds");
        p.faceSignatures = readFaceSignatures(params, "faceSignatures");
        return std::make_shared<features::ShellFeature>(id, std::move(p));
    }
    case features::FeatureType::Sweep: {
        features::SweepParams p;
        p.profileId     = params.getString("profileId");
        p.sketchId      = params.getString("sketchId");
        p.pathId        = params.getString("pathId");
        p.pathSketchId  = params.getString("pathSketchId");
        p.operation     = featureOpFromStr(params.getString("operation", "NewBody"));
        p.isPerpendicularOrientation = params.getBool("isPerpendicularOrientation", true);
        return std::make_shared<features::SweepFeature>(id, std::move(p));
    }
    case features::FeatureType::Loft: {
        features::LoftParams p;
        p.sectionIds       = readStringArray(params, "sectionIds");
        p.sectionSketchIds = readStringArray(params, "sectionSketchIds");
        p.isClosed         = params.getBool("isClosed");
        p.operation        = featureOpFromStr(params.getString("operation", "NewBody"));
        return std::make_shared<features::LoftFeature>(id, std::move(p));
    }
    case features::FeatureType::Mirror: {
        features::MirrorParams p;
        p.targetBodyId = params.getString("targetBodyId");
        p.planeOx = params.getNumber("planeOx");
        p.planeOy = params.getNumber("planeOy");
        p.planeOz = params.getNumber("planeOz");
        p.planeNx = params.getNumber("planeNx", 1.0);
        p.planeNy = params.getNumber("planeNy");
        p.planeNz = params.getNumber("planeNz");
        p.isCombine = params.getBool("isCombine", true);
        return std::make_shared<features::MirrorFeature>(id, std::move(p));
    }
    case features::FeatureType::RectangularPattern: {
        features::RectangularPatternParams p;
        p.targetBodyId = params.getString("targetBodyId");
        p.dir1X = params.getNumber("dir1X", 1.0);
        p.dir1Y = params.getNumber("dir1Y");
        p.dir1Z = params.getNumber("dir1Z");
        p.spacing1Expr = params.getString("spacing1Expr", "20 mm");
        p.count1 = params.getInt("count1", 3);
        p.dir2X = params.getNumber("dir2X");
        p.dir2Y = params.getNumber("dir2Y", 1.0);
        p.dir2Z = params.getNumber("dir2Z");
        p.spacing2Expr = params.getString("spacing2Expr", "20 mm");
        p.count2 = params.getInt("count2", 1);
        p.operation = featureOpFromStr(params.getString("operation", "Join"));
        return std::make_shared<features::RectangularPatternFeature>(id, std::move(p));
    }
    case features::FeatureType::CircularPattern: {
        features::CircularPatternParams p;
        p.targetBodyId = params.getString("targetBodyId");
        p.axisOx = params.getNumber("axisOx");
        p.axisOy = params.getNumber("axisOy");
        p.axisOz = params.getNumber("axisOz");
        p.axisDx = params.getNumber("axisDx");
        p.axisDy = params.getNumber("axisDy");
        p.axisDz = params.getNumber("axisDz", 1.0);
        p.count = params.getInt("count", 6);
        p.totalAngleDeg = params.getNumber("totalAngleDeg", 360.0);
        p.operation = featureOpFromStr(params.getString("operation", "Join"));
        return std::make_shared<features::CircularPatternFeature>(id, std::move(p));
    }
    case features::FeatureType::Sketch: {
        features::SketchParams p;
        p.planeId = params.getString("planeId", "XY");
        p.originX = params.getNumber("originX");
        p.originY = params.getNumber("originY");
        p.originZ = params.getNumber("originZ");
        p.xDirX = params.getNumber("xDirX", 1.0);
        p.xDirY = params.getNumber("xDirY");
        p.xDirZ = params.getNumber("xDirZ");
        p.yDirX = params.getNumber("yDirX");
        p.yDirY = params.getNumber("yDirY", 1.0);
        p.yDirZ = params.getNumber("yDirZ");

        auto feat = std::make_shared<features::SketchFeature>(id, std::move(p));

        // Populate sketch geometry if present
        if (sketchObj) {
            deserializeSketch(*sketchObj, feat->sketch());
        }

        return feat;
    }
    case features::FeatureType::Hole: {
        features::HoleParams p;
        p.targetBodyId      = params.getString("targetBodyId");
        p.holeType          = holeTypeFromStr(params.getString("holeType", "Simple"));
        p.posX              = params.getNumber("posX");
        p.posY              = params.getNumber("posY");
        p.posZ              = params.getNumber("posZ");
        p.dirX              = params.getNumber("dirX");
        p.dirY              = params.getNumber("dirY");
        p.dirZ              = params.getNumber("dirZ", -1.0);
        p.diameterExpr      = params.getString("diameterExpr", "10 mm");
        p.depthExpr         = params.getString("depthExpr", "0");
        p.tipAngleDeg       = params.getNumber("tipAngleDeg", 118.0);
        p.cboreDiameterExpr = params.getString("cboreDiameterExpr", "16 mm");
        p.cboreDepthExpr    = params.getString("cboreDepthExpr", "5 mm");
        p.csinkDiameterExpr = params.getString("csinkDiameterExpr", "20 mm");
        p.csinkAngleDeg     = params.getNumber("csinkAngleDeg", 90.0);
        p.tapType           = holeTapTypeFromStr(params.getString("tapType", "Simple"));
        return std::make_shared<features::HoleFeature>(id, std::move(p));
    }
    case features::FeatureType::Combine: {
        features::CombineParams p;
        p.targetBodyId = params.getString("targetBodyId");
        p.toolBodyId   = params.getString("toolBodyId");
        p.operation    = combineOpFromStr(params.getString("operation", "Join"));
        p.keepToolBody = params.getBool("keepToolBody");
        return std::make_shared<features::CombineFeature>(id, std::move(p));
    }
    case features::FeatureType::SplitBody: {
        features::SplitBodyParams p;
        p.targetBodyId   = params.getString("targetBodyId");
        p.splittingToolId = params.getString("splittingToolId");
        p.planeOx = params.getNumber("planeOx");
        p.planeOy = params.getNumber("planeOy");
        p.planeOz = params.getNumber("planeOz");
        p.planeNx = params.getNumber("planeNx");
        p.planeNy = params.getNumber("planeNy");
        p.planeNz = params.getNumber("planeNz", 1.0);
        p.usePlane = params.getBool("usePlane", true);
        return std::make_shared<features::SplitBodyFeature>(id, std::move(p));
    }
    case features::FeatureType::OffsetFaces: {
        features::OffsetFacesParams p;
        p.targetBodyId = params.getString("targetBodyId");
        p.faceIndices  = readIntArray(params, "faceIndices");
        p.faceSignatures = readFaceSignatures(params, "faceSignatures");
        p.distance     = params.getNumber("distance", 2.0);
        return std::make_shared<features::OffsetFacesFeature>(id, std::move(p));
    }
    case features::FeatureType::Move: {
        features::MoveParams p;
        p.targetBodyId = params.getString("targetBodyId");
        p.mode         = moveModeFromStr(params.getString("mode", "TranslateXYZ"));
        p.dx           = params.getNumber("dx");
        p.dy           = params.getNumber("dy");
        p.dz           = params.getNumber("dz");
        p.axisOx       = params.getNumber("axisOx");
        p.axisOy       = params.getNumber("axisOy");
        p.axisOz       = params.getNumber("axisOz");
        p.axisDx       = params.getNumber("axisDx");
        p.axisDy       = params.getNumber("axisDy");
        p.axisDz       = params.getNumber("axisDz", 1.0);
        p.angleDeg     = params.getNumber("angleDeg");
        // Read 4x4 matrix
        {
            const auto* matArr = params.getArray("matrix");
            if (matArr && matArr->arrayVal.size() == 16) {
                for (int i = 0; i < 16; ++i) {
                    const auto& elem = matArr->arrayVal[i];
                    if (elem && elem->type == JsonValue::Type::Object)
                        p.matrix[i] = elem->getNumber("v");
                }
            }
        }
        p.createCopy = params.getBool("createCopy");
        return std::make_shared<features::MoveFeature>(id, std::move(p));
    }
    case features::FeatureType::Draft: {
        features::DraftParams p;
        p.targetBodyId = params.getString("targetBodyId");
        p.faceIndices  = readIntArray(params, "faceIndices");
        p.faceSignatures = readFaceSignatures(params, "faceSignatures");
        p.angleExpr    = params.getString("angleExpr", "3 deg");
        p.pullDirX     = params.getNumber("pullDirX");
        p.pullDirY     = params.getNumber("pullDirY");
        p.pullDirZ     = params.getNumber("pullDirZ", 1.0);
        return std::make_shared<features::DraftFeature>(id, std::move(p));
    }
    case features::FeatureType::Thicken: {
        features::ThickenParams p;
        p.targetBodyId  = params.getString("targetBodyId");
        p.thicknessExpr = params.getString("thicknessExpr", "2 mm");
        p.isSymmetric   = params.getBool("isSymmetric");
        return std::make_shared<features::ThickenFeature>(id, std::move(p));
    }
    case features::FeatureType::Thread: {
        features::ThreadParams p;
        p.targetBodyId        = params.getString("targetBodyId");
        p.cylindricalFaceIndex = params.getInt("cylindricalFaceIndex", -1);
        p.faceSignatures      = readFaceSignatures(params, "faceSignatures");
        p.threadType          = threadTypeFromStr(params.getString("threadType", "MetricCoarse"));
        p.pitch               = params.getNumber("pitch", 1.0);
        p.depth               = params.getNumber("depth", 0.3);
        p.isInternal          = params.getBool("isInternal", true);
        p.isRightHanded       = params.getBool("isRightHanded", true);
        p.isModeled           = params.getBool("isModeled");
        p.isFullLength        = params.getBool("isFullLength", true);
        p.threadLength        = params.getNumber("threadLength");
        p.threadOffset        = params.getNumber("threadOffset");
        return std::make_shared<features::ThreadFeature>(id, std::move(p));
    }
    case features::FeatureType::Scale: {
        features::ScaleParams p;
        p.targetBodyId = params.getString("targetBodyId");
        p.scaleType    = scaleTypeFromStr(params.getString("scaleType", "Uniform"));
        p.factor       = params.getNumber("factor", 2.0);
        p.factorX      = params.getNumber("factorX", 1.0);
        p.factorY      = params.getNumber("factorY", 1.0);
        p.factorZ      = params.getNumber("factorZ", 1.0);
        p.centerX      = params.getNumber("centerX");
        p.centerY      = params.getNumber("centerY");
        p.centerZ      = params.getNumber("centerZ");
        return std::make_shared<features::ScaleFeature>(id, std::move(p));
    }
    case features::FeatureType::ConstructionPlane: {
        features::ConstructionPlaneParams p;
        p.definitionType = planeDefTypeFromStr(params.getString("definitionType", "Standard"));
        p.standardPlane  = params.getString("standardPlane");
        p.parentPlaneId  = params.getString("parentPlaneId");
        p.offsetDistance  = params.getNumber("offsetDistance");
        p.angleDeg       = params.getNumber("angleDeg");
        p.originX        = params.getNumber("originX");
        p.originY        = params.getNumber("originY");
        p.originZ        = params.getNumber("originZ");
        p.normalX        = params.getNumber("normalX");
        p.normalY        = params.getNumber("normalY");
        p.normalZ        = params.getNumber("normalZ", 1.0);
        p.xDirX          = params.getNumber("xDirX", 1.0);
        p.xDirY          = params.getNumber("xDirY");
        p.xDirZ          = params.getNumber("xDirZ");
        return std::make_shared<features::ConstructionPlane>(id, std::move(p));
    }
    case features::FeatureType::ConstructionAxis: {
        features::ConstructionAxisParams p;
        p.definitionType = axisDefTypeFromStr(params.getString("definitionType", "Standard"));
        p.standardAxis   = params.getString("standardAxis");
        p.originX        = params.getNumber("originX");
        p.originY        = params.getNumber("originY");
        p.originZ        = params.getNumber("originZ");
        p.dirX           = params.getNumber("dirX");
        p.dirY           = params.getNumber("dirY");
        p.dirZ           = params.getNumber("dirZ", 1.0);
        p.point1Id       = params.getString("point1Id");
        p.point2Id       = params.getString("point2Id");
        p.plane1Id       = params.getString("plane1Id");
        p.plane2Id       = params.getString("plane2Id");
        return std::make_shared<features::ConstructionAxis>(id, std::move(p));
    }
    case features::FeatureType::ConstructionPoint: {
        features::ConstructionPointParams p;
        p.definitionType = pointDefTypeFromStr(params.getString("definitionType", "Standard"));
        p.x              = params.getNumber("x");
        p.y              = params.getNumber("y");
        p.z              = params.getNumber("z");
        p.lineId         = params.getString("lineId");
        p.planeId        = params.getString("planeId");
        p.circleId       = params.getString("circleId");
        return std::make_shared<features::ConstructionPoint>(id, std::move(p));
    }
    case features::FeatureType::PathPattern: {
        features::PathPatternParams p;
        p.targetBodyId = params.getString("targetBodyId");
        p.pathBodyId = params.getString("pathBodyId");
        p.count = params.getInt("count", 5);
        p.startOffset = params.getNumber("startOffset");
        p.endOffset = params.getNumber("endOffset", 1.0);
        p.operation = featureOpFromStr(params.getString("operation", "Join"));
        return std::make_shared<features::PathPatternFeature>(id, std::move(p));
    }
    case features::FeatureType::Coil: {
        features::CoilParams p;
        p.profileBodyId = params.getString("profileBodyId");
        p.axisOx = params.getNumber("axisOx");
        p.axisOy = params.getNumber("axisOy");
        p.axisOz = params.getNumber("axisOz");
        p.axisDx = params.getNumber("axisDx");
        p.axisDy = params.getNumber("axisDy");
        p.axisDz = params.getNumber("axisDz", 1.0);
        p.radius = params.getNumber("radius", 10.0);
        p.pitch = params.getNumber("pitch", 5.0);
        p.turns = params.getNumber("turns", 5.0);
        p.taperAngleDeg = params.getNumber("taperAngleDeg");
        return std::make_shared<features::CoilFeature>(id, std::move(p));
    }
    case features::FeatureType::DeleteFace: {
        features::DeleteFaceParams p;
        p.targetBodyId = params.getString("targetBodyId");
        const auto* faceArr = params.getArray("faceIndices");
        if (faceArr) {
            for (const auto& v : faceArr->arrayVal) {
                if (v && v->type == JsonValue::Type::Object)
                    p.faceIndices.push_back(v->getInt("v", 0));
            }
        }
        return std::make_shared<features::DeleteFaceFeature>(id, std::move(p));
    }
    case features::FeatureType::ReplaceFace: {
        features::ReplaceFaceParams p;
        p.targetBodyId = params.getString("targetBodyId");
        p.faceIndex = params.getInt("faceIndex", 0);
        p.replacementBodyId = params.getString("replacementBodyId");
        return std::make_shared<features::ReplaceFaceFeature>(id, std::move(p));
    }
    case features::FeatureType::ReverseNormal: {
        features::ReverseNormalParams p;
        p.targetBodyId = params.getString("targetBodyId");
        const auto* faceArr = params.getArray("faceIndices");
        if (faceArr) {
            for (const auto& v : faceArr->arrayVal) {
                if (v && v->type == JsonValue::Type::Object)
                    p.faceIndices.push_back(v->getInt("v", 0));
            }
        }
        return std::make_shared<features::ReverseNormalFeature>(id, std::move(p));
    }
    case features::FeatureType::Joint: {
        features::JointParams p;
        p.occurrenceOneId = params.getString("occurrenceOneId");
        p.occurrenceTwoId = params.getString("occurrenceTwoId");
        p.jointType       = jointTypeFromStr(params.getString("jointType", "Rigid"));
        // Geometry one
        p.geometryOne.originX       = params.getNumber("g1OriginX");
        p.geometryOne.originY       = params.getNumber("g1OriginY");
        p.geometryOne.originZ       = params.getNumber("g1OriginZ");
        p.geometryOne.primaryAxisX  = params.getNumber("g1PrimaryAxisX");
        p.geometryOne.primaryAxisY  = params.getNumber("g1PrimaryAxisY");
        p.geometryOne.primaryAxisZ  = params.getNumber("g1PrimaryAxisZ", 1.0);
        p.geometryOne.secondaryAxisX = params.getNumber("g1SecondaryAxisX", 1.0);
        p.geometryOne.secondaryAxisY = params.getNumber("g1SecondaryAxisY");
        p.geometryOne.secondaryAxisZ = params.getNumber("g1SecondaryAxisZ");
        // Geometry two
        p.geometryTwo.originX       = params.getNumber("g2OriginX");
        p.geometryTwo.originY       = params.getNumber("g2OriginY");
        p.geometryTwo.originZ       = params.getNumber("g2OriginZ");
        p.geometryTwo.primaryAxisX  = params.getNumber("g2PrimaryAxisX");
        p.geometryTwo.primaryAxisY  = params.getNumber("g2PrimaryAxisY");
        p.geometryTwo.primaryAxisZ  = params.getNumber("g2PrimaryAxisZ", 1.0);
        p.geometryTwo.secondaryAxisX = params.getNumber("g2SecondaryAxisX", 1.0);
        p.geometryTwo.secondaryAxisY = params.getNumber("g2SecondaryAxisY");
        p.geometryTwo.secondaryAxisZ = params.getNumber("g2SecondaryAxisZ");
        // Motion values
        p.rotationValue     = params.getNumber("rotationValue");
        p.translationValue  = params.getNumber("translationValue");
        p.rotation2Value    = params.getNumber("rotation2Value");
        p.translation2Value = params.getNumber("translation2Value");
        p.rotation3Value    = params.getNumber("rotation3Value");
        // Rotation limits
        p.rotationLimits.hasMin   = params.getBool("rotLimHasMin");
        p.rotationLimits.hasMax   = params.getBool("rotLimHasMax");
        p.rotationLimits.minValue = params.getNumber("rotLimMin");
        p.rotationLimits.maxValue = params.getNumber("rotLimMax");
        p.rotationLimits.restValue = params.getNumber("rotLimRest");
        // Translation limits
        p.translationLimits.hasMin   = params.getBool("transLimHasMin");
        p.translationLimits.hasMax   = params.getBool("transLimHasMax");
        p.translationLimits.minValue = params.getNumber("transLimMin");
        p.translationLimits.maxValue = params.getNumber("transLimMax");
        p.translationLimits.restValue = params.getNumber("transLimRest");
        // Flags
        p.isFlipped    = params.getBool("isFlipped");
        p.isLocked     = params.getBool("isLocked");
        p.isSuppressed = params.getBool("isSuppressed");
        return std::make_shared<features::Joint>(id, std::move(p));
    }
    default:
        return nullptr;
    }
}

// ── load ────────────────────────────────────────────────────────────────────

bool Serializer::load(Document& doc, const std::string& path)
{
    // Read file contents
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;

    std::ostringstream ss;
    ss << in.rdbuf();
    std::string json = ss.str();

    // Parse JSON
    auto root = JsonReader::parse(json);
    if (!root || root->type != JsonValue::Type::Object)
        return false;

    // Version check
    int version = root->getInt("version", 0);
    if (version < 1)
        return false;

    // Reset document
    doc.newDocument();

    // Name
    std::string name = root->getString("name", "Untitled");
    doc.setName(name);

    // Parameters
    const auto* paramsObj = root->getObject("parameters");
    if (paramsObj) {
        for (const auto& key : paramsObj->objectKeys) {
            const auto* param = paramsObj->getObject(key);
            if (param) {
                std::string expr = param->getString("expression");
                std::string unit = param->getString("unit", "mm");
                doc.parameters().set(key, expr, unit);
            }
        }
    }

    // Timeline
    const auto* timelineArr = root->getArray("timeline");
    if (timelineArr) {
        for (const auto& entryVal : timelineArr->arrayVal) {
            if (!entryVal || entryVal->type != JsonValue::Type::Object)
                continue;

            std::string entryId = entryVal->getString("id");
            std::string typeStr = entryVal->getString("type");
            bool isSuppressed   = entryVal->getBool("isSuppressed");

            auto ft = featureTypeFromStr(typeStr);

            const auto* paramsNode = entryVal->getObject("params");
            if (!paramsNode)
                continue;

            // For sketch features, get the embedded sketch object
            const auto* sketchNode = entryVal->getObject("sketch");

            auto feature = reconstructFeature(entryId, ft, *paramsNode, sketchNode);
            if (!feature)
                continue;

            // Append to timeline with auto-generated numbered name
            doc.appendFeatureToTimeline(feature);

            // Set suppressed state and custom name if needed
            size_t idx = doc.timeline().count() - 1;
            doc.timeline().entry(idx).isSuppressed = isSuppressed;
            std::string customName = entryVal->getString("customName", "");
            if (!customName.empty())
                doc.timeline().entry(idx).customName = customName;
        }
    }

    // Marker position
    int markerPos = root->getInt("markerPosition",
                                  static_cast<int>(doc.timeline().count()));
    doc.timeline().setMarker(static_cast<size_t>(markerPos));

    // Timeline groups
    const auto* groupsArr = root->getArray("groups");
    if (groupsArr) {
        for (const auto& gVal : groupsArr->arrayVal) {
            if (!gVal || gVal->type != JsonValue::Type::Object)
                continue;

            std::string gName  = gVal->getString("name");
            int startIdx       = gVal->getInt("startIndex", 0);
            int endIdx         = gVal->getInt("endIndex", 0);
            bool isCollapsed   = gVal->getBool("isCollapsed");
            bool isSuppressed  = gVal->getBool("isSuppressed");

            if (startIdx >= 0 && endIdx >= startIdx &&
                static_cast<size_t>(endIdx) < doc.timeline().count()) {
                try {
                    std::string gid = doc.timeline().createGroup(
                        gName, static_cast<size_t>(startIdx), static_cast<size_t>(endIdx));
                    doc.timeline().setGroupCollapsed(gid, isCollapsed);
                    doc.timeline().setGroupSuppressed(gid, isSuppressed);
                } catch (...) {
                    // Skip invalid groups silently
                }
            }
        }
    }

    // Material / appearance assignments (body-level)
    const auto* materialsArr = root->getArray("materials");
    if (materialsArr) {
        for (const auto& mVal : materialsArr->arrayVal) {
            if (!mVal || mVal->type != JsonValue::Type::Object)
                continue;
            std::string bodyId = mVal->getString("bodyId");
            kernel::Material mat;
            mat.name      = mVal->getString("name", "Default");
            mat.baseR     = static_cast<float>(mVal->getNumber("baseR", 0.6));
            mat.baseG     = static_cast<float>(mVal->getNumber("baseG", 0.65));
            mat.baseB     = static_cast<float>(mVal->getNumber("baseB", 0.7));
            mat.opacity   = static_cast<float>(mVal->getNumber("opacity", 1.0));
            mat.metallic  = static_cast<float>(mVal->getNumber("metallic", 0.0));
            mat.roughness = static_cast<float>(mVal->getNumber("roughness", 0.5));
            mat.density   = mVal->getNumber("density", 0.00785);
            if (!bodyId.empty())
                doc.appearances().setBodyMaterial(bodyId, mat);
        }
    }

    // Face-level material overrides
    const auto* faceMatsArr = root->getArray("faceMaterials");
    if (faceMatsArr) {
        for (const auto& fmVal : faceMatsArr->arrayVal) {
            if (!fmVal || fmVal->type != JsonValue::Type::Object)
                continue;
            std::string bodyId = fmVal->getString("bodyId");
            int faceIdx        = fmVal->getInt("faceIndex", -1);
            kernel::Material mat;
            mat.name      = fmVal->getString("name", "Default");
            mat.baseR     = static_cast<float>(fmVal->getNumber("baseR", 0.6));
            mat.baseG     = static_cast<float>(fmVal->getNumber("baseG", 0.65));
            mat.baseB     = static_cast<float>(fmVal->getNumber("baseB", 0.7));
            mat.opacity   = static_cast<float>(fmVal->getNumber("opacity", 1.0));
            mat.metallic  = static_cast<float>(fmVal->getNumber("metallic", 0.0));
            mat.roughness = static_cast<float>(fmVal->getNumber("roughness", 0.5));
            mat.density   = fmVal->getNumber("density", 0.00785);
            if (!bodyId.empty() && faceIdx >= 0)
                doc.appearances().setFaceMaterial(bodyId, faceIdx, mat);
        }
    }

    // Rebuild geometry from the loaded timeline
    doc.recompute();

    doc.setModified(false);
    return true;
}

} // namespace document
