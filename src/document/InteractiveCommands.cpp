#include "InteractiveCommands.h"
#include "Document.h"
#include <sstream>

namespace document {

// ── InteractiveCommand base undo ─────────────────────────────────────────────

void InteractiveCommand::undo(Document& doc)
{
    if (!m_createdFeatureId.empty()) {
        doc.timeline().remove(m_createdFeatureId);
        doc.recompute();
    }
}

// ── Extrude ──────────────────────────────────────────────────────────────────

std::vector<CommandInputDef> ExtrudeInteractiveCommand::inputDefinitions() const
{
    return {
        {"distance", "Distance", CommandInputType::Distance, 10.0, 0.01, 10000},
        {"operation", "Operation", CommandInputType::DropDown, 0, 0, 0, "",
         {"New Body", "Join", "Cut", "Intersect"}, 0},
        {"taper", "Taper Angle", CommandInputType::Angle, 0, -89, 89},
        {"symmetric", "Symmetric", CommandInputType::CheckBox}
    };
}

void ExtrudeInteractiveCommand::executeWithInputs(Document& doc, const CommandInputValues& inputs)
{
    features::ExtrudeParams params;
    std::ostringstream oss;
    oss << inputs.getNumeric("distance", 10) << " mm";
    params.distanceExpr = oss.str();
    params.taperAngleDeg = inputs.getNumeric("taper", 0);
    params.isSymmetric = inputs.getBool("symmetric", false);
    int opIdx = inputs.getInt("operation", 0);
    params.operation = static_cast<features::FeatureOperation>(opIdx);

    doc.addExtrude(std::move(params));
    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_createdFeatureId = tl.entry(tl.count() - 1).id;
}

// ── Fillet ───────────────────────────────────────────────────────────────────

std::vector<CommandInputDef> FilletInteractiveCommand::inputDefinitions() const
{
    return {
        {"radius", "Radius", CommandInputType::Distance, 2.0, 0.01, 1000},
        {"g2", "G2 Continuity", CommandInputType::CheckBox},
        {"tangentChain", "Tangent Chain", CommandInputType::CheckBox, 0, 0, 0, "", {}, 0, true}
    };
}

void FilletInteractiveCommand::executeWithInputs(Document& doc, const CommandInputValues& inputs)
{
    auto ids = doc.brepModel().bodyIds();
    if (ids.empty()) return;

    features::FilletParams params;
    params.targetBodyId = ids.back();
    std::ostringstream oss;
    oss << inputs.getNumeric("radius", 2) << " mm";
    params.radiusExpr = oss.str();
    params.isG2 = inputs.getBool("g2", false);
    params.isTangentChain = inputs.getBool("tangentChain", true);

    doc.addFillet(std::move(params));
    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_createdFeatureId = tl.entry(tl.count() - 1).id;
}

// ── Hole ─────────────────────────────────────────────────────────────────────

std::vector<CommandInputDef> HoleInteractiveCommand::inputDefinitions() const
{
    return {
        {"diameter", "Diameter", CommandInputType::Distance, 10.0, 0.1, 1000},
        {"depth", "Depth (0=through)", CommandInputType::Distance, 20.0, 0, 10000},
        {"holeType", "Type", CommandInputType::DropDown, 0, 0, 0, "",
         {"Simple", "Counterbore", "Countersink"}, 0}
    };
}

void HoleInteractiveCommand::executeWithInputs(Document& doc, const CommandInputValues& inputs)
{
    auto ids = doc.brepModel().bodyIds();
    if (ids.empty()) return;

    features::HoleParams params;
    params.targetBodyId = ids.back();
    std::ostringstream diaOss, depOss;
    diaOss << inputs.getNumeric("diameter", 10) << " mm";
    depOss << inputs.getNumeric("depth", 20) << " mm";
    params.diameterExpr = diaOss.str();
    params.depthExpr = depOss.str();
    params.holeType = static_cast<features::HoleType>(inputs.getInt("holeType", 0));
    params.dirX = 0; params.dirY = 0; params.dirZ = -1;

    doc.addHole(std::move(params));
    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_createdFeatureId = tl.entry(tl.count() - 1).id;
}

// ── Chamfer ──────────────────────────────────────────────────────────────────

std::vector<CommandInputDef> ChamferInteractiveCommand::inputDefinitions() const
{
    return {
        {"distance", "Distance", CommandInputType::Distance, 1.0, 0.01, 1000},
        {"chamferType", "Type", CommandInputType::DropDown, 0, 0, 0, "",
         {"Equal Distance", "Two Distances", "Distance + Angle"}, 0}
    };
}

void ChamferInteractiveCommand::executeWithInputs(Document& doc, const CommandInputValues& inputs)
{
    auto ids = doc.brepModel().bodyIds();
    if (ids.empty()) return;

    features::ChamferParams params;
    params.targetBodyId = ids.back();
    std::ostringstream oss;
    oss << inputs.getNumeric("distance", 1) << " mm";
    params.distanceExpr = oss.str();
    params.chamferType = static_cast<features::ChamferType>(inputs.getInt("chamferType", 0));

    doc.addChamfer(std::move(params));
    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_createdFeatureId = tl.entry(tl.count() - 1).id;
}

// ── Shell ────────────────────────────────────────────────────────────────────

std::vector<CommandInputDef> ShellInteractiveCommand::inputDefinitions() const
{
    return {
        {"thickness", "Wall Thickness", CommandInputType::Distance, 2.0, 0.01, 100}
    };
}

void ShellInteractiveCommand::executeWithInputs(Document& doc, const CommandInputValues& inputs)
{
    auto ids = doc.brepModel().bodyIds();
    if (ids.empty()) return;

    features::ShellParams params;
    params.targetBodyId = ids.back();
    params.thicknessExpr = inputs.getNumeric("thickness", 2);

    doc.addShell(std::move(params));
    auto& tl = doc.timeline();
    if (tl.count() > 0)
        m_createdFeatureId = tl.entry(tl.count() - 1).id;
}

} // namespace document
