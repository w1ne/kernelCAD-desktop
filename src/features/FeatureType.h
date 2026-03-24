#pragma once

namespace features {

enum class FeatureType {
    Extrude, Revolve, Fillet, Chamfer, Shell,
    Loft, Sweep, Hole, Thread,
    RectangularPattern, CircularPattern, Mirror,
    Draft, Scale, SplitBody, Combine, OffsetFaces, Move, Thicken,
    Sketch, ConstructionPlane, ConstructionAxis, ConstructionPoint,
    PathPattern, Coil,
    DeleteFace, ReplaceFace, ReverseNormal,
    Joint,
    Stitch, Unstitch, SplitFace, Patch, Rib, Web, PressPull,
    BaseFeature
};

enum class HealthState { Healthy, Warning, Error, Suppressed, RolledBack, Unknown };

} // namespace features
