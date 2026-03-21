#pragma once
#include "Command.h"
#include "Timeline.h"
#include "../features/ExtrudeFeature.h"
#include "../features/RevolveFeature.h"
#include "../features/SketchFeature.h"
#include "../features/FilletFeature.h"
#include "../features/ChamferFeature.h"
#include "../features/SweepFeature.h"
#include "../features/LoftFeature.h"
#include "../features/HoleFeature.h"
#include "../features/ShellFeature.h"
#include "../features/MirrorFeature.h"
#include "../features/RectangularPatternFeature.h"
#include "../features/CircularPatternFeature.h"
#include "../features/CombineFeature.h"
#include "../features/SplitBodyFeature.h"
#include "../features/OffsetFacesFeature.h"
#include "../features/MoveFeature.h"
#include "../features/DraftFeature.h"
#include "../features/ThickenFeature.h"
#include "../features/ThreadFeature.h"
#include "../features/ScaleFeature.h"
#include "../features/PathPatternFeature.h"
#include "../features/CoilFeature.h"
#include "../features/DeleteFaceFeature.h"
#include "../features/ReplaceFaceFeature.h"
#include "../features/ReverseNormalFeature.h"
#include "../features/Joint.h"
#include <string>
#include <memory>
#include <functional>

namespace document {

// ── AddExtrudeCommand ─────────────────────────────────────────────────────────

class AddExtrudeCommand : public Command {
public:
    explicit AddExtrudeCommand(features::ExtrudeParams params);

    std::string description() const override { return "Add Extrude"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::ExtrudeParams m_params;
    std::string m_bodyId;       // stored after first execute, for undo
    std::string m_featureId;    // stored after first execute, for undo
};

// ── AddRevolveCommand ─────────────────────────────────────────────────────────

class AddRevolveCommand : public Command {
public:
    explicit AddRevolveCommand(features::RevolveParams params);

    std::string description() const override { return "Add Revolve"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::RevolveParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddSketchCommand ──────────────────────────────────────────────────────────

/// Command that adds a sketch and optionally runs a post-setup callback
/// (e.g. to add geometry to the sketch after creation).
class AddSketchCommand : public Command {
public:
    using PostSetupFn = std::function<void(features::SketchFeature&)>;

    explicit AddSketchCommand(features::SketchParams params,
                              PostSetupFn postSetup = nullptr);

    std::string description() const override { return "Add Sketch"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::SketchParams m_params;
    PostSetupFn            m_postSetup;
    std::string            m_featureId;   // stored after execute
};

// ── DeleteFeatureCommand ──────────────────────────────────────────────────────

class DeleteFeatureCommand : public Command {
public:
    explicit DeleteFeatureCommand(std::string featureId);

    std::string description() const override { return "Delete Feature"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    std::string   m_featureId;
    TimelineEntry m_savedEntry;   // saved on execute, restored on undo
    size_t        m_savedIndex = 0;
};

// ── SuppressFeatureCommand ────────────────────────────────────────────────────

class SuppressFeatureCommand : public Command {
public:
    explicit SuppressFeatureCommand(std::string featureId);

    std::string description() const override { return "Toggle Suppress"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    std::string m_featureId;
};

// ── MoveTimelineMarkerCommand ─────────────────────────────────────────────────

class MoveTimelineMarkerCommand : public Command {
public:
    explicit MoveTimelineMarkerCommand(size_t newPosition);

    std::string description() const override { return "Move Timeline Marker"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    size_t m_newPosition;
    size_t m_oldPosition = 0;  // saved on execute
};

// ── AddSphereCommand ──────────────────────────────────────────────────────────
/// Special command for the sphere primitive, which bypasses the feature system
/// and creates a body directly through the kernel.

class AddSphereCommand : public Command {
public:
    explicit AddSphereCommand(double radius);

    std::string description() const override { return "Add Sphere"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    double      m_radius;
    std::string m_bodyId;  // stored after execute
};

// ── AddFilletCommand ──────────────────────────────────────────────────────────

class AddFilletCommand : public Command {
public:
    explicit AddFilletCommand(features::FilletParams params);

    std::string description() const override { return "Add Fillet"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::FilletParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddChamferCommand ─────────────────────────────────────────────────────────

class AddChamferCommand : public Command {
public:
    explicit AddChamferCommand(features::ChamferParams params);

    std::string description() const override { return "Add Chamfer"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::ChamferParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddSweepCommand ──────────────────────────────────────────────────────────

class AddSweepCommand : public Command {
public:
    explicit AddSweepCommand(features::SweepParams params);

    std::string description() const override { return "Add Sweep"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::SweepParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddLoftCommand ───────────────────────────────────────────────────────────

class AddLoftCommand : public Command {
public:
    explicit AddLoftCommand(features::LoftParams params);

    std::string description() const override { return "Add Loft"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::LoftParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddHoleCommand ───────────────────────────────────────────────────────────

class AddHoleCommand : public Command {
public:
    explicit AddHoleCommand(features::HoleParams params);

    std::string description() const override { return "Add Hole"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::HoleParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddShellCommand ──────────────────────────────────────────────────────────

class AddShellCommand : public Command {
public:
    explicit AddShellCommand(features::ShellParams params);

    std::string description() const override { return "Add Shell"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::ShellParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddMirrorCommand ─────────────────────────────────────────────────────────

class AddMirrorCommand : public Command {
public:
    explicit AddMirrorCommand(features::MirrorParams params);

    std::string description() const override { return "Add Mirror"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::MirrorParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddRectangularPatternCommand ─────────────────────────────────────────────

class AddRectangularPatternCommand : public Command {
public:
    explicit AddRectangularPatternCommand(features::RectangularPatternParams params);

    std::string description() const override { return "Add Rectangular Pattern"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::RectangularPatternParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddCircularPatternCommand ────────────────────────────────────────────────

class AddCircularPatternCommand : public Command {
public:
    explicit AddCircularPatternCommand(features::CircularPatternParams params);

    std::string description() const override { return "Add Circular Pattern"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::CircularPatternParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddCombineCommand ────────────────────────────────────────────────────────

class AddCombineCommand : public Command {
public:
    explicit AddCombineCommand(features::CombineParams params);

    std::string description() const override { return "Add Combine"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::CombineParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddSplitBodyCommand ──────────────────────────────────────────────────────

class AddSplitBodyCommand : public Command {
public:
    explicit AddSplitBodyCommand(features::SplitBodyParams params);

    std::string description() const override { return "Add Split Body"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::SplitBodyParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddOffsetFacesCommand ────────────────────────────────────────────────────

class AddOffsetFacesCommand : public Command {
public:
    explicit AddOffsetFacesCommand(features::OffsetFacesParams params);

    std::string description() const override { return "Add Offset Faces"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::OffsetFacesParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddMoveCommand ───────────────────────────────────────────────────────────

class AddMoveCommand : public Command {
public:
    explicit AddMoveCommand(features::MoveParams params);

    std::string description() const override { return "Add Move"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::MoveParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddDraftCommand ──────────────────────────────────────────────────────────

class AddDraftCommand : public Command {
public:
    explicit AddDraftCommand(features::DraftParams params);

    std::string description() const override { return "Add Draft"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::DraftParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddThickenCommand ────────────────────────────────────────────────────────

class AddThickenCommand : public Command {
public:
    explicit AddThickenCommand(features::ThickenParams params);

    std::string description() const override { return "Add Thicken"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::ThickenParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddThreadCommand ─────────────────────────────────────────────────────────

class AddThreadCommand : public Command {
public:
    explicit AddThreadCommand(features::ThreadParams params);

    std::string description() const override { return "Add Thread"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::ThreadParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddScaleCommand ──────────────────────────────────────────────────────────

class AddScaleCommand : public Command {
public:
    explicit AddScaleCommand(features::ScaleParams params);

    std::string description() const override { return "Add Scale"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::ScaleParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddPathPatternCommand ────────────────────────────────────────────────────

class AddPathPatternCommand : public Command {
public:
    explicit AddPathPatternCommand(features::PathPatternParams params);

    std::string description() const override { return "Add Path Pattern"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::PathPatternParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddCoilCommand ──────────────────────────────────────────────────────────

class AddCoilCommand : public Command {
public:
    explicit AddCoilCommand(features::CoilParams params);

    std::string description() const override { return "Add Coil"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::CoilParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddDeleteFaceCommand ────────────────────────────────────────────────────

class AddDeleteFaceCommand : public Command {
public:
    explicit AddDeleteFaceCommand(features::DeleteFaceParams params);

    std::string description() const override { return "Add Delete Face"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::DeleteFaceParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddReplaceFaceCommand ───────────────────────────────────────────────────

class AddReplaceFaceCommand : public Command {
public:
    explicit AddReplaceFaceCommand(features::ReplaceFaceParams params);

    std::string description() const override { return "Add Replace Face"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::ReplaceFaceParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddReverseNormalCommand ─────────────────────────────────────────────────

class AddReverseNormalCommand : public Command {
public:
    explicit AddReverseNormalCommand(features::ReverseNormalParams params);

    std::string description() const override { return "Add Reverse Normal"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::ReverseNormalParams m_params;
    std::string m_bodyId;
    std::string m_featureId;
};

// ── AddJointCommand ──────────────────────────────────────────────────────────

class AddJointCommand : public Command {
public:
    explicit AddJointCommand(features::JointParams params);

    std::string description() const override { return "Add Joint"; }
    void execute(Document& doc) override;
    void undo(Document& doc) override;

private:
    features::JointParams m_params;
    std::string m_featureId;
};

} // namespace document
