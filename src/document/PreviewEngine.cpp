#include "PreviewEngine.h"
#include "Document.h"
#include "Timeline.h"
#include "../features/ExtrudeFeature.h"
#include "../features/RevolveFeature.h"
#include "../features/FilletFeature.h"
#include "../features/ChamferFeature.h"
#include "../features/ShellFeature.h"
#include "../features/SketchFeature.h"
#include "../kernel/OCCTKernel.h"
#include "../kernel/BRepModel.h"
#include <TopoDS_Shape.hxx>

namespace document {

PreviewEngine::PreviewEngine(Document& doc)
    : m_doc(doc)
{
}

PreviewEngine::~PreviewEngine() = default;

bool PreviewEngine::isActive() const
{
    return m_active;
}

const std::string& PreviewEngine::activeFeatureId() const
{
    return m_featureId;
}

void PreviewEngine::setMeshCallback(MeshCallback cb)
{
    m_meshCallback = std::move(cb);
}

void PreviewEngine::setClearCallback(ClearCallback cb)
{
    m_clearCallback = std::move(cb);
}

// ---------------------------------------------------------------------------
// beginPreview
// ---------------------------------------------------------------------------

void PreviewEngine::beginPreview(const std::string& featureId)
{
    if (m_active && m_featureId == featureId)
        return; // already previewing this feature

    // If we were previewing a different feature, cancel the old one first
    if (m_active)
        cancelPreview();

    m_featureId = featureId;
    m_active = true;

    // Find the feature and save its current params
    auto& tl = m_doc.timeline();
    for (size_t i = 0; i < tl.count(); ++i) {
        auto& entry = tl.entry(i);
        if (entry.feature && entry.feature->id() == featureId) {
            saveFeatureParams(entry.feature.get());
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// updatePreview
// ---------------------------------------------------------------------------

bool PreviewEngine::updatePreview()
{
    if (!m_active)
        return false;

    // Find the feature in the timeline
    auto& tl = m_doc.timeline();
    features::Feature* feat = nullptr;
    for (size_t i = 0; i < tl.count(); ++i) {
        auto& entry = tl.entry(i);
        if (entry.feature && entry.feature->id() == m_featureId) {
            feat = entry.feature.get();
            break;
        }
    }
    if (!feat)
        return false;

    auto& kernel = m_doc.kernel();
    auto& brep = m_doc.brepModel();

    try {
        TopoDS_Shape previewShape;

        using FT = features::FeatureType;
        switch (feat->type()) {
        case FT::Extrude: {
            auto* ef = static_cast<features::ExtrudeFeature*>(feat);
            const sketch::Sketch* sketchPtr = nullptr;
            if (!ef->params().sketchId.empty()) {
                auto* sketchFeat = m_doc.findSketch(ef->params().sketchId);
                if (sketchFeat)
                    sketchPtr = &sketchFeat->sketch();
            }
            previewShape = ef->execute(kernel, sketchPtr);
            break;
        }
        case FT::Revolve: {
            auto* rf = static_cast<features::RevolveFeature*>(feat);
            const sketch::Sketch* sketchPtr = nullptr;
            if (!rf->params().sketchId.empty()) {
                auto* sketchFeat = m_doc.findSketch(rf->params().sketchId);
                if (sketchFeat)
                    sketchPtr = &sketchFeat->sketch();
            }
            previewShape = rf->execute(kernel, sketchPtr);
            break;
        }
        case FT::Fillet: {
            auto* ff = static_cast<features::FilletFeature*>(feat);
            std::string targetId = ff->params().targetBodyId;
            if (!brep.hasBody(targetId))
                return false;
            previewShape = ff->execute(kernel, brep.getShape(targetId));
            break;
        }
        case FT::Chamfer: {
            auto* cf = static_cast<features::ChamferFeature*>(feat);
            std::string targetId = cf->params().targetBodyId;
            if (!brep.hasBody(targetId))
                return false;
            previewShape = cf->execute(kernel, brep.getShape(targetId));
            break;
        }
        case FT::Shell: {
            auto* sf = static_cast<features::ShellFeature*>(feat);
            std::string targetId = sf->params().targetBodyId;
            if (!brep.hasBody(targetId))
                return false;
            previewShape = sf->execute(kernel, brep.getShape(targetId));
            break;
        }
        default:
            // For unsupported types, fall back to a full recompute approach:
            // just commit immediately (no preview).
            return false;
        }

        if (previewShape.IsNull())
            return false;

        // Tessellate the preview shape
        auto mesh = kernel.tessellate(previewShape, 0.1);
        if (mesh.vertices.empty())
            return false;

        // Send mesh data to viewport via callback
        if (m_meshCallback)
            m_meshCallback(mesh.vertices, mesh.normals, mesh.indices);

        return true;

    } catch (const std::exception&) {
        // If execution fails (e.g., invalid params), clear the preview
        if (m_clearCallback)
            m_clearCallback();
        return false;
    }
}

// ---------------------------------------------------------------------------
// commitPreview
// ---------------------------------------------------------------------------

void PreviewEngine::commitPreview()
{
    if (!m_active)
        return;

    // Clear preview overlay from viewport
    if (m_clearCallback)
        m_clearCallback();

    // The feature params have already been updated by the PropertiesPanel.
    // Do a full recompute so downstream features pick up the change.
    m_doc.recompute();
    m_doc.setModified(true);

    m_active = false;
    m_featureId.clear();
    m_savedParams.clear();
}

// ---------------------------------------------------------------------------
// cancelPreview
// ---------------------------------------------------------------------------

void PreviewEngine::cancelPreview()
{
    if (!m_active)
        return;

    // Clear preview overlay from viewport
    if (m_clearCallback)
        m_clearCallback();

    // Restore original params
    auto& tl = m_doc.timeline();
    for (size_t i = 0; i < tl.count(); ++i) {
        auto& entry = tl.entry(i);
        if (entry.feature && entry.feature->id() == m_featureId) {
            restoreFeatureParams(entry.feature.get());
            break;
        }
    }

    m_active = false;
    m_featureId.clear();
    m_savedParams.clear();
}

// ---------------------------------------------------------------------------
// saveFeatureParams / restoreFeatureParams
// ---------------------------------------------------------------------------

void PreviewEngine::saveFeatureParams(features::Feature* feat)
{
    m_savedParams.clear();

    using FT = features::FeatureType;
    switch (feat->type()) {
    case FT::Extrude: {
        auto* ef = static_cast<features::ExtrudeFeature*>(feat);
        const auto& p = ef->params();
        m_savedParams["distanceExpr"] = p.distanceExpr;
        m_savedParams["extentType"] = std::to_string(static_cast<int>(p.extentType));
        m_savedParams["direction"] = std::to_string(static_cast<int>(p.direction));
        m_savedParams["operation"] = std::to_string(static_cast<int>(p.operation));
        m_savedParams["isSymmetric"] = p.isSymmetric ? "1" : "0";
        m_savedParams["taperAngleDeg"] = std::to_string(p.taperAngleDeg);
        m_savedParams["distance2Expr"] = p.distance2Expr;
        m_savedParams["taperAngle2Deg"] = std::to_string(p.taperAngle2Deg);
        m_savedParams["isThinExtrude"] = p.isThinExtrude ? "1" : "0";
        m_savedParams["wallThickness"] = std::to_string(p.wallThickness);
        break;
    }
    case FT::Revolve: {
        auto* rf = static_cast<features::RevolveFeature*>(feat);
        const auto& p = rf->params();
        m_savedParams["angleExpr"] = p.angleExpr;
        m_savedParams["axisType"] = std::to_string(static_cast<int>(p.axisType));
        m_savedParams["isFullRevolution"] = p.isFullRevolution ? "1" : "0";
        m_savedParams["isProjectAxis"] = p.isProjectAxis ? "1" : "0";
        m_savedParams["operation"] = std::to_string(static_cast<int>(p.operation));
        m_savedParams["angle2Expr"] = p.angle2Expr;
        break;
    }
    case FT::Fillet: {
        auto* ff = static_cast<features::FilletFeature*>(feat);
        const auto& p = ff->params();
        m_savedParams["radiusExpr"] = p.radiusExpr;
        m_savedParams["isVariableRadius"] = p.isVariableRadius ? "1" : "0";
        m_savedParams["isG2"] = p.isG2 ? "1" : "0";
        m_savedParams["isTangentChain"] = p.isTangentChain ? "1" : "0";
        m_savedParams["isRollingBallCorner"] = p.isRollingBallCorner ? "1" : "0";
        break;
    }
    case FT::Chamfer: {
        auto* cf = static_cast<features::ChamferFeature*>(feat);
        const auto& p = cf->params();
        m_savedParams["distanceExpr"] = p.distanceExpr;
        m_savedParams["distance2Expr"] = p.distance2Expr;
        m_savedParams["chamferType"] = std::to_string(static_cast<int>(p.chamferType));
        m_savedParams["angleDeg"] = std::to_string(p.angleDeg);
        m_savedParams["isTangentChain"] = p.isTangentChain ? "1" : "0";
        break;
    }
    case FT::Shell: {
        auto* sf = static_cast<features::ShellFeature*>(feat);
        const auto& p = sf->params();
        m_savedParams["thicknessExpr"] = std::to_string(p.thicknessExpr);
        break;
    }
    default:
        break;
    }
}

void PreviewEngine::restoreFeatureParams(features::Feature* feat)
{
    if (m_savedParams.empty())
        return;

    using FT = features::FeatureType;
    switch (feat->type()) {
    case FT::Extrude: {
        auto* ef = static_cast<features::ExtrudeFeature*>(feat);
        auto& p = ef->params();
        if (auto it = m_savedParams.find("distanceExpr"); it != m_savedParams.end())
            p.distanceExpr = it->second;
        if (auto it = m_savedParams.find("extentType"); it != m_savedParams.end())
            p.extentType = static_cast<features::ExtentType>(std::stoi(it->second));
        if (auto it = m_savedParams.find("direction"); it != m_savedParams.end())
            p.direction = static_cast<features::ExtentDirection>(std::stoi(it->second));
        if (auto it = m_savedParams.find("operation"); it != m_savedParams.end())
            p.operation = static_cast<features::FeatureOperation>(std::stoi(it->second));
        if (auto it = m_savedParams.find("isSymmetric"); it != m_savedParams.end())
            p.isSymmetric = (it->second == "1");
        if (auto it = m_savedParams.find("taperAngleDeg"); it != m_savedParams.end())
            p.taperAngleDeg = std::stod(it->second);
        if (auto it = m_savedParams.find("distance2Expr"); it != m_savedParams.end())
            p.distance2Expr = it->second;
        if (auto it = m_savedParams.find("taperAngle2Deg"); it != m_savedParams.end())
            p.taperAngle2Deg = std::stod(it->second);
        if (auto it = m_savedParams.find("isThinExtrude"); it != m_savedParams.end())
            p.isThinExtrude = (it->second == "1");
        if (auto it = m_savedParams.find("wallThickness"); it != m_savedParams.end())
            p.wallThickness = std::stod(it->second);
        break;
    }
    case FT::Revolve: {
        auto* rf = static_cast<features::RevolveFeature*>(feat);
        auto& p = rf->params();
        if (auto it = m_savedParams.find("angleExpr"); it != m_savedParams.end())
            p.angleExpr = it->second;
        if (auto it = m_savedParams.find("axisType"); it != m_savedParams.end())
            p.axisType = static_cast<features::AxisType>(std::stoi(it->second));
        if (auto it = m_savedParams.find("isFullRevolution"); it != m_savedParams.end())
            p.isFullRevolution = (it->second == "1");
        if (auto it = m_savedParams.find("isProjectAxis"); it != m_savedParams.end())
            p.isProjectAxis = (it->second == "1");
        if (auto it = m_savedParams.find("operation"); it != m_savedParams.end())
            p.operation = static_cast<features::FeatureOperation>(std::stoi(it->second));
        if (auto it = m_savedParams.find("angle2Expr"); it != m_savedParams.end())
            p.angle2Expr = it->second;
        break;
    }
    case FT::Fillet: {
        auto* ff = static_cast<features::FilletFeature*>(feat);
        auto& p = ff->params();
        if (auto it = m_savedParams.find("radiusExpr"); it != m_savedParams.end())
            p.radiusExpr = it->second;
        if (auto it = m_savedParams.find("isVariableRadius"); it != m_savedParams.end())
            p.isVariableRadius = (it->second == "1");
        if (auto it = m_savedParams.find("isG2"); it != m_savedParams.end())
            p.isG2 = (it->second == "1");
        if (auto it = m_savedParams.find("isTangentChain"); it != m_savedParams.end())
            p.isTangentChain = (it->second == "1");
        if (auto it = m_savedParams.find("isRollingBallCorner"); it != m_savedParams.end())
            p.isRollingBallCorner = (it->second == "1");
        break;
    }
    case FT::Chamfer: {
        auto* cf = static_cast<features::ChamferFeature*>(feat);
        auto& p = cf->params();
        if (auto it = m_savedParams.find("distanceExpr"); it != m_savedParams.end())
            p.distanceExpr = it->second;
        if (auto it = m_savedParams.find("distance2Expr"); it != m_savedParams.end())
            p.distance2Expr = it->second;
        if (auto it = m_savedParams.find("chamferType"); it != m_savedParams.end())
            p.chamferType = static_cast<features::ChamferType>(std::stoi(it->second));
        if (auto it = m_savedParams.find("angleDeg"); it != m_savedParams.end())
            p.angleDeg = std::stod(it->second);
        if (auto it = m_savedParams.find("isTangentChain"); it != m_savedParams.end())
            p.isTangentChain = (it->second == "1");
        break;
    }
    case FT::Shell: {
        auto* sf = static_cast<features::ShellFeature*>(feat);
        auto& p = sf->params();
        if (auto it = m_savedParams.find("thicknessExpr"); it != m_savedParams.end())
            p.thicknessExpr = std::stod(it->second);
        break;
    }
    default:
        break;
    }
}

} // namespace document
