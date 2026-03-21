#include "Commands.h"
#include "Document.h"
#include "../kernel/BRepModel.h"
#include <TopoDS_Shape.hxx>
#include <sstream>

namespace document {

// ── AddExtrudeCommand ─────────────────────────────────────────────────────────

AddExtrudeCommand::AddExtrudeCommand(features::ExtrudeParams params)
    : m_params(std::move(params))
{}

void AddExtrudeCommand::execute(Document& doc)
{
    m_bodyId = doc.addExtrude(m_params);

    // Capture the feature ID (the last entry appended to the timeline)
    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddExtrudeCommand::undo(Document& doc)
{
    // Remove the feature from the timeline and the body from BRepModel
    doc.timeline().remove(m_featureId);
    doc.brepModel().removeBody(m_bodyId);
    doc.recompute();
    doc.setModified(true);
}

// ── AddRevolveCommand ─────────────────────────────────────────────────────────

AddRevolveCommand::AddRevolveCommand(features::RevolveParams params)
    : m_params(std::move(params))
{}

void AddRevolveCommand::execute(Document& doc)
{
    m_bodyId = doc.addRevolve(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddRevolveCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    doc.brepModel().removeBody(m_bodyId);
    doc.recompute();
    doc.setModified(true);
}

// ── AddSketchCommand ──────────────────────────────────────────────────────────

AddSketchCommand::AddSketchCommand(features::SketchParams params,
                                   PostSetupFn postSetup)
    : m_params(std::move(params))
    , m_postSetup(std::move(postSetup))
{}

void AddSketchCommand::execute(Document& doc)
{
    m_featureId = doc.addSketch(m_params);

    // Run the optional post-setup callback (e.g. add geometry to the sketch)
    if (m_postSetup) {
        auto* sketchFeat = doc.findSketch(m_featureId);
        if (sketchFeat)
            m_postSetup(*sketchFeat);
    }
}

void AddSketchCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    doc.recompute();
    doc.setModified(true);
}

// ── DeleteFeatureCommand ──────────────────────────────────────────────────────

DeleteFeatureCommand::DeleteFeatureCommand(std::string featureId)
    : m_featureId(std::move(featureId))
{}

void DeleteFeatureCommand::execute(Document& doc)
{
    // Find and save the entry + its index before removing
    auto& tl = doc.timeline();
    for (size_t i = 0; i < tl.count(); ++i) {
        if (tl.entry(i).id == m_featureId) {
            m_savedIndex = i;
            // Copy the entry (shared_ptr to feature is ref-counted)
            m_savedEntry.id          = tl.entry(i).id;
            m_savedEntry.name        = tl.entry(i).name;
            m_savedEntry.feature     = tl.entry(i).feature;
            m_savedEntry.isSuppressed = tl.entry(i).isSuppressed;
            m_savedEntry.isRolledBack = tl.entry(i).isRolledBack;
            break;
        }
    }
    tl.remove(m_featureId);
    doc.recompute();
    doc.setModified(true);
}

void DeleteFeatureCommand::undo(Document& doc)
{
    // Re-insert the saved entry at its original position
    doc.timeline().insert(m_savedIndex, m_savedEntry);
    doc.recompute();
    doc.setModified(true);
}

// ── SuppressFeatureCommand ────────────────────────────────────────────────────

SuppressFeatureCommand::SuppressFeatureCommand(std::string featureId)
    : m_featureId(std::move(featureId))
{}

void SuppressFeatureCommand::execute(Document& doc)
{
    auto& tl = doc.timeline();
    for (size_t i = 0; i < tl.count(); ++i) {
        if (tl.entry(i).id == m_featureId) {
            tl.entry(i).isSuppressed = !tl.entry(i).isSuppressed;
            break;
        }
    }
    doc.recompute();
    doc.setModified(true);
}

void SuppressFeatureCommand::undo(Document& doc)
{
    // Toggle suppress is its own inverse
    execute(doc);
}

// ── MoveTimelineMarkerCommand ─────────────────────────────────────────────────

MoveTimelineMarkerCommand::MoveTimelineMarkerCommand(size_t newPosition)
    : m_newPosition(newPosition)
{}

void MoveTimelineMarkerCommand::execute(Document& doc)
{
    m_oldPosition = doc.timeline().markerPosition();
    doc.timeline().setMarker(m_newPosition);
    doc.recompute();
}

void MoveTimelineMarkerCommand::undo(Document& doc)
{
    doc.timeline().setMarker(m_oldPosition);
    doc.recompute();
}

// ── AddSphereCommand ──────────────────────────────────────────────────────────

AddSphereCommand::AddSphereCommand(double radius)
    : m_radius(radius)
{}

void AddSphereCommand::execute(Document& doc)
{
    auto& tl = doc.timeline();
    int counter = static_cast<int>(tl.count()) + 1;
    std::ostringstream bodyIdStream;
    bodyIdStream << "body_" << counter;
    m_bodyId = bodyIdStream.str();

    TopoDS_Shape sphere = doc.kernel().makeSphere(m_radius);
    doc.brepModel().addBody(m_bodyId, sphere);
    doc.setModified(true);
}

void AddSphereCommand::undo(Document& doc)
{
    doc.brepModel().removeBody(m_bodyId);
    doc.setModified(true);
}

// ── AddFilletCommand ──────────────────────────────────────────────────────────

AddFilletCommand::AddFilletCommand(features::FilletParams params)
    : m_params(std::move(params))
{}

void AddFilletCommand::execute(Document& doc)
{
    m_bodyId = doc.addFillet(m_params);

    // Capture the feature ID (the last entry appended to the timeline)
    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddFilletCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    // Fillet modifies an existing body in-place, so recompute to restore it
    doc.recompute();
    doc.setModified(true);
}

// ── AddChamferCommand ─────────────────────────────────────────────────────────

AddChamferCommand::AddChamferCommand(features::ChamferParams params)
    : m_params(std::move(params))
{}

void AddChamferCommand::execute(Document& doc)
{
    m_bodyId = doc.addChamfer(m_params);

    // Capture the feature ID (the last entry appended to the timeline)
    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddChamferCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    // Chamfer modifies an existing body in-place, so recompute to restore it
    doc.recompute();
    doc.setModified(true);
}

// ── AddSweepCommand ──────────────────────────────────────────────────────────

AddSweepCommand::AddSweepCommand(features::SweepParams params)
    : m_params(std::move(params))
{}

void AddSweepCommand::execute(Document& doc)
{
    m_bodyId = doc.addSweep(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddSweepCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    doc.brepModel().removeBody(m_bodyId);
    doc.recompute();
    doc.setModified(true);
}

// ── AddLoftCommand ───────────────────────────────────────────────────────────

AddLoftCommand::AddLoftCommand(features::LoftParams params)
    : m_params(std::move(params))
{}

void AddLoftCommand::execute(Document& doc)
{
    m_bodyId = doc.addLoft(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddLoftCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    doc.brepModel().removeBody(m_bodyId);
    doc.recompute();
    doc.setModified(true);
}

// ── AddHoleCommand ───────────────────────────────────────────────────────────

AddHoleCommand::AddHoleCommand(features::HoleParams params)
    : m_params(std::move(params))
{}

void AddHoleCommand::execute(Document& doc)
{
    m_bodyId = doc.addHole(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddHoleCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    // Hole modifies an existing body in-place, so recompute to restore it
    doc.recompute();
    doc.setModified(true);
}

// ── AddShellCommand ──────────────────────────────────────────────────────────

AddShellCommand::AddShellCommand(features::ShellParams params)
    : m_params(std::move(params))
{}

void AddShellCommand::execute(Document& doc)
{
    m_bodyId = doc.addShell(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddShellCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    // Shell modifies an existing body in-place, so recompute to restore it
    doc.recompute();
    doc.setModified(true);
}

// ── AddMirrorCommand ─────────────────────────────────────────────────────────

AddMirrorCommand::AddMirrorCommand(features::MirrorParams params)
    : m_params(std::move(params))
{}

void AddMirrorCommand::execute(Document& doc)
{
    m_bodyId = doc.addMirror(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddMirrorCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    // Mirror modifies/replaces an existing body, so recompute to restore it
    doc.recompute();
    doc.setModified(true);
}

// ── AddRectangularPatternCommand ─────────────────────────────────────────────

AddRectangularPatternCommand::AddRectangularPatternCommand(features::RectangularPatternParams params)
    : m_params(std::move(params))
{}

void AddRectangularPatternCommand::execute(Document& doc)
{
    m_bodyId = doc.addRectangularPattern(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddRectangularPatternCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    // Pattern modifies/replaces an existing body, so recompute to restore it
    doc.recompute();
    doc.setModified(true);
}

// ── AddCircularPatternCommand ────────────────────────────────────────────────

AddCircularPatternCommand::AddCircularPatternCommand(features::CircularPatternParams params)
    : m_params(std::move(params))
{}

void AddCircularPatternCommand::execute(Document& doc)
{
    m_bodyId = doc.addCircularPattern(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddCircularPatternCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    // Pattern modifies/replaces an existing body, so recompute to restore it
    doc.recompute();
    doc.setModified(true);
}

// ── AddCombineCommand ────────────────────────────────────────────────────────

AddCombineCommand::AddCombineCommand(features::CombineParams params)
    : m_params(std::move(params))
{}

void AddCombineCommand::execute(Document& doc)
{
    m_bodyId = doc.addCombine(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddCombineCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    // Combine modifies/replaces bodies, so recompute to restore them
    doc.recompute();
    doc.setModified(true);
}

// ── AddSplitBodyCommand ──────────────────────────────────────────────────────

AddSplitBodyCommand::AddSplitBodyCommand(features::SplitBodyParams params)
    : m_params(std::move(params))
{}

void AddSplitBodyCommand::execute(Document& doc)
{
    m_bodyId = doc.addSplitBody(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddSplitBodyCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    // SplitBody modifies/replaces an existing body, so recompute to restore it
    doc.recompute();
    doc.setModified(true);
}

// ── AddOffsetFacesCommand ────────────────────────────────────────────────────

AddOffsetFacesCommand::AddOffsetFacesCommand(features::OffsetFacesParams params)
    : m_params(std::move(params))
{}

void AddOffsetFacesCommand::execute(Document& doc)
{
    m_bodyId = doc.addOffsetFaces(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddOffsetFacesCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    // OffsetFaces modifies an existing body in-place, so recompute to restore it
    doc.recompute();
    doc.setModified(true);
}

// ── AddMoveCommand ───────────────────────────────────────────────────────────

AddMoveCommand::AddMoveCommand(features::MoveParams params)
    : m_params(std::move(params))
{}

void AddMoveCommand::execute(Document& doc)
{
    m_bodyId = doc.addMove(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddMoveCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    // Move modifies an existing body in-place, so recompute to restore it
    doc.recompute();
    doc.setModified(true);
}

// ── AddDraftCommand ──────────────────────────────────────────────────────────

AddDraftCommand::AddDraftCommand(features::DraftParams params)
    : m_params(std::move(params))
{}

void AddDraftCommand::execute(Document& doc)
{
    m_bodyId = doc.addDraft(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddDraftCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    // Draft modifies an existing body in-place, so recompute to restore it
    doc.recompute();
    doc.setModified(true);
}

// ── AddThickenCommand ────────────────────────────────────────────────────────

AddThickenCommand::AddThickenCommand(features::ThickenParams params)
    : m_params(std::move(params))
{}

void AddThickenCommand::execute(Document& doc)
{
    m_bodyId = doc.addThicken(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddThickenCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    // Thicken modifies an existing body in-place, so recompute to restore it
    doc.recompute();
    doc.setModified(true);
}

// ── AddThreadCommand ─────────────────────────────────────────────────────────

AddThreadCommand::AddThreadCommand(features::ThreadParams params)
    : m_params(std::move(params))
{}

void AddThreadCommand::execute(Document& doc)
{
    m_bodyId = doc.addThread(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddThreadCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    // Thread modifies an existing body in-place, so recompute to restore it
    doc.recompute();
    doc.setModified(true);
}

// ── AddScaleCommand ──────────────────────────────────────────────────────────

AddScaleCommand::AddScaleCommand(features::ScaleParams params)
    : m_params(std::move(params))
{}

void AddScaleCommand::execute(Document& doc)
{
    m_bodyId = doc.addScale(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddScaleCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    // Scale modifies an existing body in-place, so recompute to restore it
    doc.recompute();
    doc.setModified(true);
}

// ── AddPathPatternCommand ────────────────────────────────────────────────────

AddPathPatternCommand::AddPathPatternCommand(features::PathPatternParams params)
    : m_params(std::move(params))
{}

void AddPathPatternCommand::execute(Document& doc)
{
    m_bodyId = doc.addPathPattern(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddPathPatternCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    doc.recompute();
    doc.setModified(true);
}

// ── AddCoilCommand ──────────────────────────────────────────────────────────

AddCoilCommand::AddCoilCommand(features::CoilParams params)
    : m_params(std::move(params))
{}

void AddCoilCommand::execute(Document& doc)
{
    m_bodyId = doc.addCoil(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddCoilCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    doc.brepModel().removeBody(m_bodyId);
    doc.recompute();
    doc.setModified(true);
}

// ── AddDeleteFaceCommand ────────────────────────────────────────────────────

AddDeleteFaceCommand::AddDeleteFaceCommand(features::DeleteFaceParams params)
    : m_params(std::move(params))
{}

void AddDeleteFaceCommand::execute(Document& doc)
{
    m_bodyId = doc.addDeleteFace(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddDeleteFaceCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    doc.recompute();
    doc.setModified(true);
}

// ── AddReplaceFaceCommand ───────────────────────────────────────────────────

AddReplaceFaceCommand::AddReplaceFaceCommand(features::ReplaceFaceParams params)
    : m_params(std::move(params))
{}

void AddReplaceFaceCommand::execute(Document& doc)
{
    m_bodyId = doc.addReplaceFace(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddReplaceFaceCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    doc.recompute();
    doc.setModified(true);
}

// ── AddReverseNormalCommand ─────────────────────────────────────────────────

AddReverseNormalCommand::AddReverseNormalCommand(features::ReverseNormalParams params)
    : m_params(std::move(params))
{}

void AddReverseNormalCommand::execute(Document& doc)
{
    m_bodyId = doc.addReverseNormal(m_params);

    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_featureId = tl.entry(tl.count() - 1).id;
}

void AddReverseNormalCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    doc.recompute();
    doc.setModified(true);
}

// ── AddJointCommand ──────────────────────────────────────────────────────────

AddJointCommand::AddJointCommand(features::JointParams params)
    : m_params(std::move(params))
{}

void AddJointCommand::execute(Document& doc)
{
    m_featureId = doc.addJoint(m_params);
}

void AddJointCommand::undo(Document& doc)
{
    doc.timeline().remove(m_featureId);
    doc.recompute();
    doc.setModified(true);
}

} // namespace document
