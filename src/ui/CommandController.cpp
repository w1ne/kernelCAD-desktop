#include "CommandController.h"
#include "MainWindow.h"
#include "Viewport3D.h"
#include "SketchEditor.h"
#include "FeatureTree.h"
#include "PropertiesPanel.h"
#include "SelectionManager.h"
#include "MeasureTool.h"
#include "JointCreator.h"
#include "CommandDialog.h"
#include "FeatureDialog.h"
#include "ViewportManipulator.h"
#include "../document/Document.h"
#include "../document/InteractiveCommands.h"
#include "../document/Timeline.h"
#include "../document/Commands.h"
#include "../document/Component.h"
#include "../document/PreviewEngine.h"
#include "../features/ExtrudeFeature.h"
#include "../features/RevolveFeature.h"
#include "../features/SketchFeature.h"
#include "../features/ShellFeature.h"
#include "../features/FilletFeature.h"
#include "../features/ChamferFeature.h"
#include "../features/DraftFeature.h"
#include "../features/MirrorFeature.h"
#include "../features/CircularPatternFeature.h"
#include "../features/RectangularPatternFeature.h"
#include "../features/HoleFeature.h"
#include "../features/SweepFeature.h"
#include "../features/LoftFeature.h"
#include "../features/OffsetFacesFeature.h"
#include "../features/ConstructionPlane.h"
#include "../features/Joint.h"
#include "../kernel/BRepModel.h"
#include "../kernel/BRepQuery.h"
#include "../kernel/StableReference.h"
#include <TopoDS_Shape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Compound.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <BRep_Tool.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Pnt.hxx>
#include <Poly_Triangulation.hxx>
#include <TopLoc_Location.hxx>
#include <sstream>
#include <algorithm>

#include <QAction>
#include <QStatusBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>

CommandController::CommandController(MainWindow* mainWindow,
                                     document::Document* doc,
                                     SelectionManager* selMgr,
                                     Viewport3D* viewport,
                                     FeatureDialog* featureDialog,
                                     PropertiesPanel* properties,
                                     QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_document(doc)
    , m_selectionMgr(selMgr)
    , m_viewport(viewport)
    , m_featureDialog(featureDialog)
    , m_properties(properties)
{
}

// =============================================================================
// Primitive creation
// =============================================================================

void CommandController::onCreateBox()
{
    m_lastCommandName = tr("Create Box");
    m_lastCommandCallback = [this]() { onCreateBox(); };
    features::ExtrudeParams params;
    params.profileId    = "";           // empty = base-feature shortcut (makeBox)
    params.distanceExpr = "50 mm";
    params.extentType   = features::ExtentType::Distance;
    params.operation    = features::FeatureOperation::NewBody;

    m_document->executeCommand(
        std::make_unique<document::AddExtrudeCommand>(std::move(params)));
    m_mainWindow->statusBar()->showMessage(tr("Created box"));
    m_mainWindow->refreshAllPanels();

    // Select the newly created feature in the properties panel
    auto& tl = m_document->timeline();
    if (tl.count() > 0) {
        auto& lastEntry = tl.entry(tl.count() - 1);
        m_properties->showFeature(QString::fromStdString(lastEntry.id));
    }
}

void CommandController::onCreateCylinder()
{
    m_lastCommandName = tr("Create Cylinder");
    m_lastCommandCallback = [this]() { onCreateCylinder(); };
    features::RevolveParams params;
    params.profileId       = "";          // empty = base-feature shortcut (makeCylinder)
    params.angleExpr       = "360 deg";
    params.isFullRevolution = true;
    params.operation       = features::FeatureOperation::NewBody;

    m_document->executeCommand(
        std::make_unique<document::AddRevolveCommand>(std::move(params)));
    m_mainWindow->statusBar()->showMessage(tr("Created cylinder"));
    m_mainWindow->refreshAllPanels();

    // Select the newly created feature in the properties panel
    auto& tl = m_document->timeline();
    if (tl.count() > 0) {
        auto& lastEntry = tl.entry(tl.count() - 1);
        m_properties->showFeature(QString::fromStdString(lastEntry.id));
    }
}

void CommandController::onCreateSphere()
{
    m_lastCommandName = tr("Create Sphere");
    m_lastCommandCallback = [this]() { onCreateSphere(); };
    m_document->executeCommand(
        std::make_unique<document::AddSphereCommand>(25.0));
    m_mainWindow->statusBar()->showMessage(tr("Created sphere"));
    m_mainWindow->refreshAllPanels();

    // Select the newly created feature in the properties panel
    auto& tl = m_document->timeline();
    if (tl.count() > 0) {
        auto& lastEntry = tl.entry(tl.count() - 1);
        m_properties->showFeature(QString::fromStdString(lastEntry.id));
    }
}

void CommandController::onCreateTorus()
{
    m_lastCommandName = tr("Create Torus");
    m_lastCommandCallback = [this]() { onCreateTorus(); };
    m_document->executeCommand(
        std::make_unique<document::AddTorusCommand>(20.0, 5.0));
    m_mainWindow->statusBar()->showMessage(tr("Created torus"));
    m_mainWindow->refreshAllPanels();

    auto& tl = m_document->timeline();
    if (tl.count() > 0) {
        auto& lastEntry = tl.entry(tl.count() - 1);
        m_properties->showFeature(QString::fromStdString(lastEntry.id));
    }
}

void CommandController::onCreatePipe()
{
    m_lastCommandName = tr("Create Pipe");
    m_lastCommandCallback = [this]() { onCreatePipe(); };
    m_document->executeCommand(
        std::make_unique<document::AddPipeCommand>(15.0, 12.0, 30.0));
    m_mainWindow->statusBar()->showMessage(tr("Created pipe"));
    m_mainWindow->refreshAllPanels();

    auto& tl = m_document->timeline();
    if (tl.count() > 0) {
        auto& lastEntry = tl.entry(tl.count() - 1);
        m_properties->showFeature(QString::fromStdString(lastEntry.id));
    }
}

// =============================================================================
// Sketch
// =============================================================================

void CommandController::onCreateSketch()
{
    m_lastCommandName = tr("Create Sketch");
    m_lastCommandCallback = [this]() { onCreateSketch(); };
    // If already editing a sketch, finish first
    if (m_mainWindow->sketchEditor()->isEditing())
        m_mainWindow->sketchEditor()->finishEditing();

    // Enter "pick a plane" mode -- wait for user to click an origin plane or planar face
    m_pendingSketchPlane = true;
    m_pendingCommand = PendingCommand::SketchPlane;
    m_selectionMgr->setFilter(SelectionFilter::Faces);
    if (m_mainWindow->selectFacesAction()) m_mainWindow->selectFacesAction()->setChecked(true);
    m_mainWindow->statusBar()->showMessage(tr("Select a plane or planar face to sketch on..."));
    m_mainWindow->showConfirmBar(tr("Sketch: Pick Plane"));
}

void CommandController::onEditSketch()
{
    // If already editing, finish first
    if (m_mainWindow->sketchEditor()->isEditing())
        m_mainWindow->sketchEditor()->finishEditing();

    // Find the last (or selected) sketch feature in the timeline
    auto& tl = m_document->timeline();
    features::SketchFeature* targetSketch = nullptr;

    for (size_t i = tl.count(); i > 0; --i) {
        auto& entry = tl.entry(i - 1);
        if (entry.feature &&
            entry.feature->type() == features::FeatureType::Sketch &&
            !entry.isSuppressed && !entry.isRolledBack) {
            targetSketch = static_cast<features::SketchFeature*>(entry.feature.get());
            break;
        }
    }

    if (!targetSketch) {
        m_mainWindow->statusBar()->showMessage(tr("No sketch found. Create a sketch first."), 3000);
        return;
    }

    m_mainWindow->beginSketchEditing(targetSketch);
}

void CommandController::handleSketchPlaneSelection(const SelectionHit& hit)
{
    features::SketchParams skParams;

    // Check if the hit is on a planar face of a body
    if (!hit.bodyId.empty() && hit.faceIndex >= 0) {
        auto& brep = m_document->brepModel();
        if (brep.hasBody(hit.bodyId)) {
            try {
                kernel::BRepQuery bq = brep.query(hit.bodyId);
                if (hit.faceIndex < bq.faceCount()) {
                    kernel::FaceInfo fi = bq.faceInfo(hit.faceIndex);
                    if (fi.surfaceType == kernel::SurfaceType::Plane) {
                        // Use the face centroid as sketch origin and face normal for orientation
                        skParams.planeId = hit.bodyId + ":face:" + std::to_string(hit.faceIndex);
                        skParams.originX = fi.centroidX;
                        skParams.originY = fi.centroidY;
                        skParams.originZ = fi.centroidZ;

                        // Build a coordinate frame from the face normal
                        double nx = fi.normalX, ny = fi.normalY, nz = fi.normalZ;
                        // Choose an up vector that is not parallel to normal
                        double ux = 0, uy = 1, uz = 0;
                        if (std::abs(ny) > 0.9) { ux = 1; uy = 0; uz = 0; }
                        // xDir = cross(up, normal), then yDir = cross(normal, xDir)
                        skParams.xDirX = uy * nz - uz * ny;
                        skParams.xDirY = uz * nx - ux * nz;
                        skParams.xDirZ = ux * ny - uy * nx;
                        double xLen = std::sqrt(skParams.xDirX * skParams.xDirX +
                                                skParams.xDirY * skParams.xDirY +
                                                skParams.xDirZ * skParams.xDirZ);
                        if (xLen > 1e-9) {
                            skParams.xDirX /= xLen;
                            skParams.xDirY /= xLen;
                            skParams.xDirZ /= xLen;
                        }
                        skParams.yDirX = ny * skParams.xDirZ - nz * skParams.xDirY;
                        skParams.yDirY = nz * skParams.xDirX - nx * skParams.xDirZ;
                        skParams.yDirZ = nx * skParams.xDirY - ny * skParams.xDirX;
                    } else {
                        m_mainWindow->statusBar()->showMessage(tr("Selected face is not planar. Pick a flat face or origin plane."), 3000);
                        return;
                    }
                }
            } catch (...) {
                m_mainWindow->statusBar()->showMessage(tr("Could not query face geometry."), 3000);
                return;
            }
        }
    } else {
        // No body face hit -- check origin planes using the world hit position
        // Approximate: use the hit world coords to determine which origin plane
        m_mainWindow->statusBar()->showMessage(tr("No planar face selected. Click an origin plane or a flat face."), 3000);
        return;
    }

    // Clean up pending state
    clearPendingState();

    // Create the sketch with the selected plane params
    m_document->executeCommand(
        std::make_unique<document::AddSketchCommand>(std::move(skParams)));
    m_mainWindow->refreshAllPanels();

    // Find the newly created sketch feature and enter editing mode
    auto& tl = m_document->timeline();
    for (size_t i = tl.count(); i > 0; --i) {
        auto& entry = tl.entry(i - 1);
        if (entry.feature &&
            entry.feature->type() == features::FeatureType::Sketch &&
            !entry.isSuppressed && !entry.isRolledBack) {
            auto* skFeat = static_cast<features::SketchFeature*>(entry.feature.get());
            m_mainWindow->beginSketchEditing(skFeat);
            break;
        }
    }
}

// =============================================================================
// Form (extrude / revolve / sweep / loft)
// =============================================================================

void CommandController::onExtrudeSketch()
{
    m_lastCommandName = tr("Extrude");
    m_lastCommandCallback = [this]() { onExtrudeSketch(); };

    // Collect all sketches with closed profiles
    auto& tl = m_document->timeline();
    std::vector<std::pair<features::SketchFeature*, std::string>> sketchesWithProfiles;

    for (size_t i = 0; i < tl.count(); ++i) {
        auto& entry = tl.entry(i);
        if (entry.feature &&
            entry.feature->type() == features::FeatureType::Sketch &&
            !entry.isSuppressed && !entry.isRolledBack) {
            auto* sk = static_cast<features::SketchFeature*>(entry.feature.get());
            auto profiles = sk->sketch().detectProfiles();
            if (!profiles.empty())
                sketchesWithProfiles.push_back({sk, entry.feature->id()});
        }
    }

    if (sketchesWithProfiles.empty()) {
        m_mainWindow->statusBar()->showMessage(tr("No sketch with closed profiles found. Draw a closed shape first (S \u2192 R for rectangle)."), 5000);
        return;
    }

    // If exactly one sketch with profiles, use it directly
    // If multiple, use the last one (most recent)
    auto* targetSketch = sketchesWithProfiles.back().first;
    std::string targetSketchId = sketchesWithProfiles.back().second;

    // If a sketch is selected in the feature tree, prefer that one
    for (auto& [sk, skId] : sketchesWithProfiles) {
        // Check if this sketch is highlighted/selected
        if (!m_selectionMgr->selection().empty()) {
            std::string selectedBody = m_selectionMgr->selection().front().bodyId;
            std::string featureForSel = m_document->featureForBody(selectedBody);
            if (featureForSel == skId) {
                targetSketch = sk;
                targetSketchId = skId;
                break;
            }
        }
    }

    auto profiles = targetSketch->sketch().detectProfiles();

    // Build a profileId string from the first detected profile
    std::string profileId;
    for (size_t i = 0; i < profiles[0].size(); ++i) {
        if (i > 0) profileId += ",";
        profileId += profiles[0][i];
    }

    // Build default params and show the floating feature dialog
    features::ExtrudeParams params;
    params.profileId    = profileId;
    params.sketchId     = targetSketchId;
    params.distanceExpr = "10 mm";
    params.extentType   = features::ExtentType::Distance;
    params.operation    = features::FeatureOperation::NewBody;

    // Highlight the sketch being extruded
    m_viewport->setHighlightedSketch(&targetSketch->sketch());

    m_featureDialog->showExtrude(params);
    m_mainWindow->showConfirmBar(tr("Extrude \u2014 press Enter to apply, Escape to cancel"));
    m_mainWindow->statusBar()->showMessage(
        tr("Extruding sketch '%1' (%2 profiles found). Adjust distance and press Enter.")
            .arg(QString::fromStdString(targetSketch->name()))
            .arg(profiles.size()));

    connect(m_featureDialog, &FeatureDialog::extrudeAccepted, this,
            [this](features::ExtrudeParams p) {
        m_document->executeCommand(
            std::make_unique<document::AddExtrudeCommand>(std::move(p)));
        m_viewport->setHighlightedSketch(nullptr);
        m_mainWindow->statusBar()->showMessage(tr("Extruded sketch"));
        m_mainWindow->hideConfirmBar();
        m_mainWindow->refreshAllPanels();
    }, Qt::SingleShotConnection);

    connect(m_featureDialog, &FeatureDialog::cancelled, this,
            [this]() {
        m_viewport->setHighlightedSketch(nullptr);
        m_mainWindow->hideConfirmBar();
        m_mainWindow->statusBar()->showMessage(tr("Extrude cancelled"));
    }, Qt::SingleShotConnection);
}

void CommandController::onRevolveSketch()
{
    m_lastCommandName = tr("Revolve");
    m_lastCommandCallback = [this]() { onRevolveSketch(); };
    // Find the last non-suppressed sketch in the timeline
    auto& tl = m_document->timeline();
    features::SketchFeature* lastSketch = nullptr;
    std::string lastSketchId;

    for (size_t i = 0; i < tl.count(); ++i) {
        auto& entry = tl.entry(i);
        if (entry.feature &&
            entry.feature->type() == features::FeatureType::Sketch &&
            !entry.isSuppressed && !entry.isRolledBack) {
            lastSketch = static_cast<features::SketchFeature*>(entry.feature.get());
            lastSketchId = entry.feature->id();
        }
    }

    if (!lastSketch) {
        m_mainWindow->statusBar()->showMessage(tr("No sketch found. Create a sketch first."), 3000);
        return;
    }

    // Detect profiles in the sketch
    auto profiles = lastSketch->sketch().detectProfiles();
    if (profiles.empty()) {
        m_mainWindow->statusBar()->showMessage(tr("No closed profiles found in the sketch."), 3000);
        return;
    }

    // Build a profileId string from the first detected profile
    std::string profileId;
    for (size_t i = 0; i < profiles[0].size(); ++i) {
        if (i > 0) profileId += ",";
        profileId += profiles[0][i];
    }

    // Build default params and show the floating feature dialog
    features::RevolveParams params;
    params.profileId        = profileId;
    params.sketchId         = lastSketchId;
    params.axisType         = features::AxisType::ZAxis;
    params.angleExpr        = "360 deg";
    params.isFullRevolution = true;
    params.operation        = features::FeatureOperation::NewBody;

    m_featureDialog->showRevolve(params);
    m_mainWindow->showConfirmBar(tr("Revolve"));

    connect(m_featureDialog, &FeatureDialog::revolveAccepted, this,
            [this](features::RevolveParams p) {
        m_document->executeCommand(
            std::make_unique<document::AddRevolveCommand>(std::move(p)));
        m_mainWindow->statusBar()->showMessage(tr("Revolved sketch"));
        m_mainWindow->hideConfirmBar();
        m_mainWindow->refreshAllPanels();
    }, Qt::SingleShotConnection);

    connect(m_featureDialog, &FeatureDialog::cancelled, this,
            [this]() {
        m_mainWindow->hideConfirmBar();
        m_mainWindow->statusBar()->showMessage(tr("Revolve cancelled"));
    }, Qt::SingleShotConnection);
}

void CommandController::onSweepTest()
{
    features::SweepParams params;
    // Empty profileId/pathId triggers the test helix sweep
    params.operation = features::FeatureOperation::NewBody;

    try {
        m_document->executeCommand(
            std::make_unique<document::AddSweepCommand>(std::move(params)));
        m_mainWindow->statusBar()->showMessage(tr("Created sweep (test helix)"));
    } catch (const std::exception& e) {
        QMessageBox::warning(m_mainWindow, tr("Sweep Failed"),
            tr("Could not create sweep: %1").arg(e.what()));
    }
    m_mainWindow->refreshAllPanels();
}

void CommandController::onLoftTest()
{
    m_lastCommandName = tr("Loft");
    m_lastCommandCallback = [this]() { onLoftTest(); };
    features::LoftParams params;
    // Empty sectionIds triggers the test circle-to-square loft
    params.operation = features::FeatureOperation::NewBody;

    try {
        m_document->executeCommand(
            std::make_unique<document::AddLoftCommand>(std::move(params)));
        m_mainWindow->statusBar()->showMessage(tr("Created loft (test sections)"));
    } catch (const std::exception& e) {
        QMessageBox::warning(m_mainWindow, tr("Loft Failed"),
            tr("Could not create loft: %1").arg(e.what()));
    }
    m_mainWindow->refreshAllPanels();
}

void CommandController::onSweepSketch()
{
    m_lastCommandName = tr("Sweep");
    m_lastCommandCallback = [this]() { onSweepSketch(); };
    // Find the last two sketch features: first = profile, second = path
    auto& tl = m_document->timeline();
    std::vector<std::pair<std::string, features::SketchFeature*>> sketches;

    for (size_t i = 0; i < tl.count(); ++i) {
        auto& entry = tl.entry(i);
        if (entry.feature &&
            entry.feature->type() == features::FeatureType::Sketch &&
            !entry.isSuppressed && !entry.isRolledBack) {
            auto* sf = static_cast<features::SketchFeature*>(entry.feature.get());
            sketches.push_back({entry.feature->id(), sf});
        }
    }

    if (sketches.size() < 2) {
        m_mainWindow->statusBar()->showMessage(tr("Need at least two sketches (profile + path). Create them first."), 3000);
        return;
    }

    // Use second-to-last as profile, last as path
    auto& [profileSketchId, profileSketch] = sketches[sketches.size() - 2];
    auto& [pathSketchId, pathSketch] = sketches[sketches.size() - 1];

    // Detect profiles in the profile sketch
    auto profiles = profileSketch->sketch().detectProfiles();
    if (profiles.empty()) {
        m_mainWindow->statusBar()->showMessage(tr("No closed profiles found in the profile sketch."), 3000);
        return;
    }

    // Build profileId from first detected profile
    std::string profileId;
    for (size_t i = 0; i < profiles[0].size(); ++i) {
        if (i > 0) profileId += ",";
        profileId += profiles[0][i];
    }

    // Detect profiles in the path sketch to get path curves
    auto pathProfiles = pathSketch->sketch().detectProfiles();
    if (pathProfiles.empty()) {
        m_mainWindow->statusBar()->showMessage(tr("No profiles found in the path sketch."), 3000);
        return;
    }

    std::string pathId;
    for (size_t i = 0; i < pathProfiles[0].size(); ++i) {
        if (i > 0) pathId += ",";
        pathId += pathProfiles[0][i];
    }

    features::SweepParams params;
    params.profileId   = profileId;
    params.sketchId    = profileSketchId;
    params.pathId      = pathId;
    params.pathSketchId = pathSketchId;
    params.operation   = features::FeatureOperation::NewBody;

    try {
        m_document->executeCommand(
            std::make_unique<document::AddSweepCommand>(std::move(params)));
        m_mainWindow->statusBar()->showMessage(tr("Swept sketch along path"));
    } catch (const std::exception& e) {
        QMessageBox::warning(m_mainWindow, tr("Sweep Failed"),
            tr("Could not sweep sketch: %1").arg(e.what()));
    }
    m_mainWindow->refreshAllPanels();
}

// =============================================================================
// Modify
// =============================================================================

void CommandController::onFillet()
{
    m_lastCommandName = tr("Fillet");
    m_lastCommandCallback = [this]() { onFillet(); };
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        m_mainWindow->statusBar()->showMessage(tr("No bodies found. Create a body first."), 3000);
        return;
    }

    const auto& sel = m_selectionMgr->selection();

    // If no selection at all, enter pending mode so user can pick edges
    if (sel.empty()) {
        m_pendingCommand = PendingCommand::Fillet;
        m_selectionMgr->setFilter(SelectionFilter::Edges);
        if (m_mainWindow->selectEdgesAction())
            m_mainWindow->selectEdgesAction()->setChecked(true);
        m_mainWindow->statusBar()->showMessage(
            tr("Select edges to fillet, then press Enter to confirm (Esc to cancel)"));
        return;
    }

    // Collect edges from selection
    std::string bodyId;
    std::vector<int> edgeIndices = collectSelectedEdges(bodyId);

    // If selection has no body, fall back to last body
    if (bodyId.empty())
        bodyId = ids.back();

    features::FilletParams params;
    params.targetBodyId = bodyId;
    params.edgeIds      = edgeIndices;  // empty = fillet all edges (fallback)
    params.radiusExpr   = "3 mm";

    // Capture stable edge signatures for topology persistence
    if (!edgeIndices.empty()) {
        try {
            const TopoDS_Shape& shape = brep.getShape(bodyId);
            for (int idx : edgeIndices)
                params.edgeSignatures.push_back(
                    kernel::StableReference::computeEdgeSignature(shape, idx));
        } catch (...) {
            // If signature capture fails, proceed without them
        }
    }

    // Show the floating feature dialog for parameter input
    m_featureDialog->showFillet(params);
    m_mainWindow->showConfirmBar(tr("Fillet"));

    connect(m_featureDialog, &FeatureDialog::filletAccepted, this,
            [this](features::FilletParams p) {
        try {
            m_document->executeCommand(
                std::make_unique<document::AddFilletCommand>(std::move(p)));
            m_mainWindow->statusBar()->showMessage(tr("Fillet applied"));
        } catch (const std::exception& e) {
            QMessageBox::warning(m_mainWindow, tr("Fillet Failed"),
                tr("Could not fillet: %1").arg(e.what()));
        }
        m_mainWindow->hideConfirmBar();
        m_pendingCommand = PendingCommand::None;
        m_selectionMgr->clearSelection();
        m_mainWindow->refreshAllPanels();
    }, Qt::SingleShotConnection);

    connect(m_featureDialog, &FeatureDialog::cancelled, this,
            [this]() {
        m_mainWindow->hideConfirmBar();
        m_pendingCommand = PendingCommand::None;
        m_selectionMgr->clearSelection();
        m_mainWindow->statusBar()->showMessage(tr("Fillet cancelled"));
    }, Qt::SingleShotConnection);
}

void CommandController::onChamfer()
{
    m_lastCommandName = tr("Chamfer");
    m_lastCommandCallback = [this]() { onChamfer(); };
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        m_mainWindow->statusBar()->showMessage(tr("No bodies found. Create a body first."), 3000);
        return;
    }

    const auto& sel = m_selectionMgr->selection();

    if (sel.empty()) {
        m_pendingCommand = PendingCommand::Chamfer;
        m_selectionMgr->setFilter(SelectionFilter::Edges);
        if (m_mainWindow->selectEdgesAction())
            m_mainWindow->selectEdgesAction()->setChecked(true);
        m_mainWindow->statusBar()->showMessage(
            tr("Select edges to chamfer, then press Enter to confirm (Esc to cancel)"));
        return;
    }

    std::string bodyId;
    std::vector<int> edgeIndices = collectSelectedEdges(bodyId);

    if (bodyId.empty())
        bodyId = ids.back();

    features::ChamferParams params;
    params.targetBodyId = bodyId;
    params.edgeIds      = edgeIndices;  // empty = chamfer all edges
    params.distanceExpr = "2 mm";

    if (!edgeIndices.empty()) {
        try {
            const TopoDS_Shape& shape = brep.getShape(bodyId);
            for (int idx : edgeIndices)
                params.edgeSignatures.push_back(
                    kernel::StableReference::computeEdgeSignature(shape, idx));
        } catch (...) {}
    }

    // Show the floating feature dialog for parameter input
    m_featureDialog->showChamfer(params);
    m_mainWindow->showConfirmBar(tr("Chamfer"));

    connect(m_featureDialog, &FeatureDialog::chamferAccepted, this,
            [this](features::ChamferParams p) {
        try {
            m_document->executeCommand(
                std::make_unique<document::AddChamferCommand>(std::move(p)));
            m_mainWindow->statusBar()->showMessage(tr("Chamfer applied"));
        } catch (const std::exception& e) {
            QMessageBox::warning(m_mainWindow, tr("Chamfer Failed"),
                tr("Could not chamfer: %1").arg(e.what()));
        }
        m_mainWindow->hideConfirmBar();
        m_pendingCommand = PendingCommand::None;
        m_selectionMgr->clearSelection();
        m_mainWindow->refreshAllPanels();
    }, Qt::SingleShotConnection);

    connect(m_featureDialog, &FeatureDialog::cancelled, this,
            [this]() {
        m_mainWindow->hideConfirmBar();
        m_pendingCommand = PendingCommand::None;
        m_selectionMgr->clearSelection();
        m_mainWindow->statusBar()->showMessage(tr("Chamfer cancelled"));
    }, Qt::SingleShotConnection);
}

void CommandController::onShell()
{
    m_lastCommandName = tr("Shell");
    m_lastCommandCallback = [this]() { onShell(); };
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        m_mainWindow->statusBar()->showMessage(tr("No bodies found. Create a body first."), 3000);
        return;
    }

    const auto& sel = m_selectionMgr->selection();

    if (sel.empty()) {
        m_pendingCommand = PendingCommand::Shell;
        m_selectionMgr->setFilter(SelectionFilter::Faces);
        if (m_mainWindow->selectFacesAction())
            m_mainWindow->selectFacesAction()->setChecked(true);
        m_mainWindow->statusBar()->showMessage(
            tr("Select faces to remove for shell, then press Enter (Esc to cancel)"));
        return;
    }

    std::string bodyId;
    std::vector<int> faceIndices = collectSelectedFaces(bodyId);

    if (bodyId.empty())
        bodyId = ids.back();

    features::ShellParams params;
    params.targetBodyId   = bodyId;
    params.thicknessExpr  = 2.0;
    params.removedFaceIds = faceIndices;  // empty = auto-detect top face

    // Capture stable face signatures
    if (!faceIndices.empty()) {
        try {
            const TopoDS_Shape& shape = brep.getShape(bodyId);
            for (int idx : faceIndices)
                params.faceSignatures.push_back(
                    kernel::StableReference::computeFaceSignature(shape, idx));
        } catch (...) {}
    }

    // Show the floating feature dialog for parameter input
    m_featureDialog->showShell(params);
    m_mainWindow->showConfirmBar(tr("Shell"));

    connect(m_featureDialog, &FeatureDialog::shellAccepted, this,
            [this](features::ShellParams p) {
        try {
            m_document->executeCommand(
                std::make_unique<document::AddShellCommand>(std::move(p)));
            m_mainWindow->statusBar()->showMessage(tr("Shell applied"));
        } catch (const std::exception& e) {
            QMessageBox::warning(m_mainWindow, tr("Shell Failed"),
                tr("Could not shell body: %1").arg(e.what()));
        }
        m_mainWindow->hideConfirmBar();
        m_pendingCommand = PendingCommand::None;
        m_selectionMgr->clearSelection();
        m_mainWindow->refreshAllPanels();
    }, Qt::SingleShotConnection);

    connect(m_featureDialog, &FeatureDialog::cancelled, this,
            [this]() {
        m_mainWindow->hideConfirmBar();
        m_pendingCommand = PendingCommand::None;
        m_selectionMgr->clearSelection();
        m_mainWindow->statusBar()->showMessage(tr("Shell cancelled"));
    }, Qt::SingleShotConnection);
}

void CommandController::onDraft()
{
    m_lastCommandName = tr("Draft");
    m_lastCommandCallback = [this]() { onDraft(); };
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        m_mainWindow->statusBar()->showMessage(tr("No bodies found. Create a body first."), 3000);
        return;
    }

    const auto& sel = m_selectionMgr->selection();

    if (sel.empty()) {
        m_pendingCommand = PendingCommand::Draft;
        m_selectionMgr->setFilter(SelectionFilter::Faces);
        if (m_mainWindow->selectFacesAction())
            m_mainWindow->selectFacesAction()->setChecked(true);
        m_mainWindow->statusBar()->showMessage(
            tr("Select faces to draft, then press Enter (Esc to cancel)"));
        return;
    }

    std::string bodyId;
    std::vector<int> faceIndices = collectSelectedFaces(bodyId);

    if (bodyId.empty())
        bodyId = ids.back();

    if (faceIndices.empty()) {
        m_mainWindow->statusBar()->showMessage(tr("Please select at least one face to draft."), 3000);
        return;
    }

    features::DraftParams params;
    params.targetBodyId = bodyId;
    params.faceIndices  = faceIndices;
    params.angleExpr    = "3 deg";

    // Capture stable face signatures
    try {
        const TopoDS_Shape& shape = brep.getShape(bodyId);
        for (int idx : faceIndices)
            params.faceSignatures.push_back(
                kernel::StableReference::computeFaceSignature(shape, idx));
    } catch (...) {}

    try {
        m_document->executeCommand(
            std::make_unique<document::AddDraftCommand>(std::move(params)));
        m_mainWindow->statusBar()->showMessage(
            tr("Drafted %1 face(s) on %2 (3 deg)")
                .arg(faceIndices.size())
                .arg(QString::fromStdString(bodyId)));
    } catch (const std::exception& e) {
        QMessageBox::warning(m_mainWindow, tr("Draft Failed"),
            tr("Could not draft: %1").arg(e.what()));
    }

    m_pendingCommand = PendingCommand::None;
    m_selectionMgr->clearSelection();
    m_mainWindow->refreshAllPanels();
}

void CommandController::onPressPull()
{
    m_lastCommandName = tr("Press/Pull");
    m_lastCommandCallback = [this]() { onPressPull(); };
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        m_mainWindow->statusBar()->showMessage(tr("No bodies found. Create a body first."), 3000);
        return;
    }

    const auto& sel = m_selectionMgr->selection();

    // If no selection, enter pending mode for face selection
    if (sel.empty()) {
        m_pendingCommand = PendingCommand::PressPull;
        m_selectionMgr->setFilter(SelectionFilter::Faces);
        if (m_mainWindow->selectFacesAction())
            m_mainWindow->selectFacesAction()->setChecked(true);
        m_mainWindow->statusBar()->showMessage(
            tr("Select face(s) to push/pull, then press Enter to confirm (Esc to cancel)"));
        return;
    }

    // Collect selected face indices
    std::string bodyId;
    std::vector<int> faceIndices = collectSelectedFaces(bodyId);

    if (bodyId.empty())
        bodyId = ids.back();

    if (faceIndices.empty()) {
        m_mainWindow->statusBar()->showMessage(tr("No faces selected. Select at least one face."), 3000);
        return;
    }

    features::OffsetFacesParams params;
    params.targetBodyId = bodyId;
    params.faceIndices = faceIndices;
    params.distance = 5.0;

    // Capture stable face signatures
    try {
        const TopoDS_Shape& shape = brep.getShape(bodyId);
        for (int idx : faceIndices)
            params.faceSignatures.push_back(
                kernel::StableReference::computeFaceSignature(shape, idx));
    } catch (...) {}

    // Show the floating feature dialog for parameter input
    m_featureDialog->showPressPull(params);
    m_mainWindow->showConfirmBar(tr("Press/Pull"));

    connect(m_featureDialog, &FeatureDialog::pressPullAccepted, this,
            [this](features::OffsetFacesParams p) {
        try {
            m_document->executeCommand(
                std::make_unique<document::AddOffsetFacesCommand>(std::move(p)));
            m_mainWindow->statusBar()->showMessage(tr("Press/Pull applied"));
        } catch (const std::exception& e) {
            QMessageBox::warning(m_mainWindow, tr("Press/Pull Failed"),
                tr("Could not apply press/pull: %1").arg(e.what()));
        }
        m_mainWindow->hideConfirmBar();
        m_pendingCommand = PendingCommand::None;
        m_selectionMgr->clearSelection();
        m_mainWindow->refreshAllPanels();
    }, Qt::SingleShotConnection);

    connect(m_featureDialog, &FeatureDialog::cancelled, this,
            [this]() {
        m_mainWindow->hideConfirmBar();
        m_pendingCommand = PendingCommand::None;
        m_selectionMgr->clearSelection();
        m_mainWindow->statusBar()->showMessage(tr("Press/Pull cancelled"));
    }, Qt::SingleShotConnection);
}

void CommandController::onAddHole()
{
    m_lastCommandName = tr("Hole");
    m_lastCommandCallback = [this]() { onAddHole(); };
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        m_mainWindow->statusBar()->showMessage(tr("No bodies found. Create a body first."), 3000);
        return;
    }

    const auto& sel = m_selectionMgr->selection();

    // Selection-driven: if no selection, enter pending mode for face selection
    if (sel.empty()) {
        m_pendingCommand = PendingCommand::Hole;
        m_selectionMgr->setFilter(SelectionFilter::Faces);
        if (m_mainWindow->selectFacesAction())
            m_mainWindow->selectFacesAction()->setChecked(true);
        m_mainWindow->statusBar()->showMessage(
            tr("Select a face for the hole position, then press Enter (Esc to cancel)"));
        return;
    }

    // If a face is selected, use its center as the hole position
    std::string lastBodyId;
    double holePosX = 0, holePosY = 0, holePosZ = 0;

    if (!sel.empty() && sel[0].faceIndex >= 0) {
        lastBodyId = sel[0].bodyId.empty() ? ids.back() : sel[0].bodyId;
        // Use the 3D hit point from the selection as hole position
        holePosX = sel[0].worldX;
        holePosY = sel[0].worldY;
        holePosZ = sel[0].worldZ;
    } else {
        lastBodyId = ids.back();
    }

    // Add a 10mm diameter, 20mm deep hole.
    // If no face was selected, determine the top face center by finding
    // the face with the highest average Z.
    const TopoDS_Shape& shape = brep.getShape(lastBodyId);

    // If no face was pre-selected, find the top face (highest average Z)
    if (!sel.empty() && sel[0].faceIndex < 0) {
        double bestAvgZ = -1e30;
        TopExp_Explorer faceEx(shape, TopAbs_FACE);
        for (; faceEx.More(); faceEx.Next()) {
            const TopoDS_Face& face = TopoDS::Face(faceEx.Current());
            double sumX = 0, sumY = 0, sumZ = 0;
            int count = 0;
            TopExp_Explorer vertEx(face, TopAbs_VERTEX);
            for (; vertEx.More(); vertEx.Next()) {
                gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(vertEx.Current()));
                sumX += p.X();
                sumY += p.Y();
                sumZ += p.Z();
                count++;
            }
            if (count > 0) {
                double avgZ = sumZ / count;
                if (avgZ > bestAvgZ) {
                    bestAvgZ = avgZ;
                    holePosX = sumX / count;
                    holePosY = sumY / count;
                    holePosZ = sumZ / count;
                }
            }
        }
    } else if (sel.empty()) {
        // Fallback: top face search when no selection (shouldn't reach here
        // due to pending mode above, but kept for robustness)
        double bestAvgZ = -1e30;
        TopExp_Explorer faceEx(shape, TopAbs_FACE);
        for (; faceEx.More(); faceEx.Next()) {
            const TopoDS_Face& face = TopoDS::Face(faceEx.Current());
            double sumX = 0, sumY = 0, sumZ = 0;
            int count = 0;
            TopExp_Explorer vertEx(face, TopAbs_VERTEX);
            for (; vertEx.More(); vertEx.Next()) {
                gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(vertEx.Current()));
                sumX += p.X();
                sumY += p.Y();
                sumZ += p.Z();
                count++;
            }
            if (count > 0) {
                double avgZ = sumZ / count;
                if (avgZ > bestAvgZ) {
                    bestAvgZ = avgZ;
                    holePosX = sumX / count;
                    holePosY = sumY / count;
                    holePosZ = sumZ / count;
                }
            }
        }
    }

    // Show the Hole dialog so user can set diameter, depth, type
    auto cmd = std::make_unique<document::HoleInteractiveCommand>();
    CommandDialog dlg(cmd.get(), m_document, m_mainWindow);
    dlg.setWindowTitle(tr("Hole"));
    if (dlg.exec() != QDialog::Accepted) {
        m_mainWindow->statusBar()->showMessage(tr("Hole cancelled"), 2000);
        return;
    }

    auto inputs = dlg.values();
    features::HoleParams params;
    params.targetBodyId = lastBodyId;
    params.holeType     = static_cast<features::HoleType>(inputs.getInt("holeType", 0));
    params.posX = holePosX;
    params.posY = holePosY;
    params.posZ = holePosZ;
    params.dirX = 0;
    params.dirY = 0;
    params.dirZ = -1;

    std::ostringstream diaOss, depOss;
    diaOss << inputs.getNumeric("diameter", 10) << " mm";
    depOss << inputs.getNumeric("depth", 20) << " mm";
    params.diameterExpr = diaOss.str();
    params.depthExpr    = depOss.str();

    try {
        m_document->executeCommand(
            std::make_unique<document::AddHoleCommand>(std::move(params)));
        m_mainWindow->statusBar()->showMessage(tr("Added hole (%1 dia, %2 deep)")
            .arg(QString::fromStdString(params.diameterExpr),
                 QString::fromStdString(params.depthExpr)));
    } catch (const std::exception& e) {
        QMessageBox::warning(m_mainWindow, tr("Hole Failed"),
            tr("Could not create hole: %1").arg(e.what()));
    }

    m_pendingCommand = PendingCommand::None;
    m_selectionMgr->clearSelection();
    m_mainWindow->refreshAllPanels();
}

// =============================================================================
// Helpers: collect selected edges/faces
// =============================================================================

std::vector<int> CommandController::collectSelectedEdges(std::string& bodyIdOut) const
{
    const auto& sel = m_selectionMgr->selection();
    std::vector<int> edges;
    bodyIdOut.clear();

    for (const auto& hit : sel) {
        if (hit.bodyId.empty())
            continue;
        if (bodyIdOut.empty())
            bodyIdOut = hit.bodyId;
        if (hit.bodyId == bodyIdOut && hit.edgeIndex >= 0)
            edges.push_back(hit.edgeIndex);
    }

    // If no edges found but we have a body from a face hit, use that body id
    if (bodyIdOut.empty() && !sel.empty())
        bodyIdOut = sel.front().bodyId;

    return edges;
}

std::vector<int> CommandController::collectSelectedFaces(std::string& bodyIdOut) const
{
    const auto& sel = m_selectionMgr->selection();
    std::vector<int> faces;
    bodyIdOut.clear();

    for (const auto& hit : sel) {
        if (hit.bodyId.empty())
            continue;
        if (bodyIdOut.empty())
            bodyIdOut = hit.bodyId;
        if (hit.bodyId == bodyIdOut && hit.faceIndex >= 0)
            faces.push_back(hit.faceIndex);
    }

    if (bodyIdOut.empty() && !sel.empty())
        bodyIdOut = sel.front().bodyId;

    return faces;
}

// =============================================================================
// Pending command: commit / cancel
// =============================================================================

void CommandController::onCommitPendingCommand()
{
    PendingCommand cmd = m_pendingCommand;
    m_pendingCommand = PendingCommand::None;

    switch (cmd) {
    case PendingCommand::Fillet:      onFillet();  break;
    case PendingCommand::Chamfer:     onChamfer(); break;
    case PendingCommand::Shell:       onShell();   break;
    case PendingCommand::Draft:       onDraft();   break;
    case PendingCommand::Hole:        onAddHole(); break;
    case PendingCommand::PressPull:   onPressPull(); break;
    case PendingCommand::SketchPlane: break; // no commit for plane pick
    case PendingCommand::None:        break;
    case PendingCommand::Extrude:     break;
    }
}

void CommandController::onCancelPendingCommand()
{
    // Cancel joint creator if active
    if (m_mainWindow->jointCreator() && m_mainWindow->jointCreator()->state() != JointCreator::State::Idle) {
        m_mainWindow->jointCreator()->cancel();
    }

    // Dismiss the feature dialog if active
    if (m_featureDialog && m_featureDialog->isActive()) {
        m_featureDialog->dismiss();
    }

    if (m_pendingCommand != PendingCommand::None) {
        m_pendingSketchPlane = false;
        m_pendingCommand = PendingCommand::None;
        m_selectionMgr->clearSelection();
        m_selectionMgr->setFilter(SelectionFilter::All);
        if (m_mainWindow->selectAllAction())
            m_mainWindow->selectAllAction()->setChecked(true);
        m_mainWindow->hideConfirmBar();
        m_mainWindow->statusBar()->showMessage(tr("Command cancelled"));
    }
}

// =============================================================================
// Pattern
// =============================================================================

void CommandController::onMirrorLastBody()
{
    m_lastCommandName = tr("Mirror");
    m_lastCommandCallback = [this]() { onMirrorLastBody(); };
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        m_mainWindow->statusBar()->showMessage(tr("No bodies found. Create a body first."), 3000);
        return;
    }

    // Mirror the most recent body about the YZ plane (X=0)
    std::string lastBodyId = ids.back();

    features::MirrorParams params;
    params.targetBodyId = lastBodyId;
    params.planeOx = 0; params.planeOy = 0; params.planeOz = 0;
    params.planeNx = 1; params.planeNy = 0; params.planeNz = 0;  // YZ plane
    params.isCombine = true;

    try {
        m_document->executeCommand(
            std::make_unique<document::AddMirrorCommand>(std::move(params)));
        m_mainWindow->statusBar()->showMessage(
            tr("Mirrored body: %1 about YZ plane").arg(QString::fromStdString(lastBodyId)));
    } catch (const std::exception& e) {
        QMessageBox::warning(m_mainWindow, tr("Mirror Failed"),
            tr("Could not mirror body: %1").arg(e.what()));
    }

    m_mainWindow->refreshAllPanels();
}

void CommandController::onCircularPattern()
{
    m_lastCommandName = tr("Circular Pattern");
    m_lastCommandCallback = [this]() { onCircularPattern(); };
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        m_mainWindow->statusBar()->showMessage(tr("No bodies found. Create a body first."), 3000);
        return;
    }

    // Create 6 copies of the most recent body around the Z axis (full 360 degrees)
    std::string lastBodyId = ids.back();

    features::CircularPatternParams params;
    params.targetBodyId = lastBodyId;
    params.axisOx = 0; params.axisOy = 0; params.axisOz = 0;
    params.axisDx = 0; params.axisDy = 0; params.axisDz = 1;  // Z axis
    params.count = 6;
    params.totalAngleDeg = 360.0;
    params.operation = features::FeatureOperation::Join;

    try {
        m_document->executeCommand(
            std::make_unique<document::AddCircularPatternCommand>(std::move(params)));
        m_mainWindow->statusBar()->showMessage(
            tr("Circular pattern: %1 (6 copies around Z)").arg(QString::fromStdString(lastBodyId)));
    } catch (const std::exception& e) {
        QMessageBox::warning(m_mainWindow, tr("Circular Pattern Failed"),
            tr("Could not create circular pattern: %1").arg(e.what()));
    }

    m_mainWindow->refreshAllPanels();
}

void CommandController::onRectangularPattern()
{
    m_lastCommandName = tr("Rectangular Pattern");
    m_lastCommandCallback = [this]() { onRectangularPattern(); };
    auto& brep = m_document->brepModel();
    auto ids = brep.bodyIds();
    if (ids.empty()) {
        m_mainWindow->statusBar()->showMessage(tr("No bodies found. Create a body first."), 3000);
        return;
    }

    std::string lastBodyId = ids.back();

    features::RectangularPatternParams params;
    params.targetBodyId = lastBodyId;
    params.dir1X = 1; params.dir1Y = 0; params.dir1Z = 0;  // X direction
    params.spacing1Expr = "30 mm";
    params.count1 = 3;
    params.dir2X = 0; params.dir2Y = 1; params.dir2Z = 0;  // Y direction
    params.spacing2Expr = "30 mm";
    params.count2 = 2;
    params.operation = features::FeatureOperation::Join;

    try {
        m_document->executeCommand(
            std::make_unique<document::AddRectangularPatternCommand>(std::move(params)));
        m_mainWindow->statusBar()->showMessage(
            tr("Rectangular pattern: %1 (3x2, 30 mm spacing)").arg(QString::fromStdString(lastBodyId)));
    } catch (const std::exception& e) {
        QMessageBox::warning(m_mainWindow, tr("Rectangular Pattern Failed"),
            tr("Could not create rectangular pattern: %1").arg(e.what()));
    }

    m_mainWindow->refreshAllPanels();
}

// =============================================================================
// Construction geometry
// =============================================================================

void CommandController::onConstructPlane()
{
    m_lastCommandName = tr("Construction Plane");
    m_lastCommandCallback = [this]() { onConstructPlane(); };

    features::ConstructionPlaneParams params;
    params.definitionType = features::PlaneDefinitionType::OffsetFromPlane;
    params.parentPlaneId = "XY";
    params.offsetDistance = 10.0;
    params.originX = 0; params.originY = 0; params.originZ = 10.0;
    params.normalX = 0; params.normalY = 0; params.normalZ = 1;
    params.xDirX = 1; params.xDirY = 0; params.xDirZ = 0;

    // Show the floating feature dialog
    m_featureDialog->showConstructionPlane(params);
    m_mainWindow->showConfirmBar(tr("Construction Plane"));

    connect(m_featureDialog, &FeatureDialog::constructionPlaneAccepted, this,
            [this](features::ConstructionPlaneParams p) {
        try {
            m_document->executeCommand(
                std::make_unique<document::AddConstructionPlaneCommand>(std::move(p)));
            m_mainWindow->statusBar()->showMessage(tr("Construction plane created"));
        } catch (const std::exception& e) {
            QMessageBox::warning(m_mainWindow, tr("Construction Plane Failed"),
                tr("Could not create plane: %1").arg(e.what()));
        }
        m_mainWindow->hideConfirmBar();
        m_mainWindow->refreshAllPanels();
    }, Qt::SingleShotConnection);

    connect(m_featureDialog, &FeatureDialog::cancelled, this,
            [this]() {
        m_mainWindow->hideConfirmBar();
        m_mainWindow->statusBar()->showMessage(tr("Construction plane cancelled"));
    }, Qt::SingleShotConnection);
}

void CommandController::onConstructAxis()
{
    bool ok = false;
    double x = QInputDialog::getDouble(m_mainWindow, tr("Construction Axis"),
        tr("Direction X:"), 0.0, -1.0, 1.0, 3, &ok);
    if (!ok) return;
    double y = QInputDialog::getDouble(m_mainWindow, tr("Construction Axis"),
        tr("Direction Y:"), 0.0, -1.0, 1.0, 3, &ok);
    if (!ok) return;
    double z = QInputDialog::getDouble(m_mainWindow, tr("Construction Axis"),
        tr("Direction Z:"), 1.0, -1.0, 1.0, 3, &ok);
    if (!ok) return;

    m_mainWindow->statusBar()->showMessage(
        tr("Construction axis created along (%1, %2, %3)").arg(x).arg(y).arg(z));
}

void CommandController::onConstructPoint()
{
    bool ok = false;
    double x = QInputDialog::getDouble(m_mainWindow, tr("Construction Point"),
        tr("X coordinate (mm):"), 0.0, -10000, 10000, 2, &ok);
    if (!ok) return;
    double y = QInputDialog::getDouble(m_mainWindow, tr("Construction Point"),
        tr("Y coordinate (mm):"), 0.0, -10000, 10000, 2, &ok);
    if (!ok) return;
    double z = QInputDialog::getDouble(m_mainWindow, tr("Construction Point"),
        tr("Z coordinate (mm):"), 0.0, -10000, 10000, 2, &ok);
    if (!ok) return;

    m_mainWindow->statusBar()->showMessage(
        tr("Construction point created at (%1, %2, %3)").arg(x).arg(y).arg(z));
}

// =============================================================================
// Assembly
// =============================================================================

void CommandController::onNewComponent()
{
    auto& reg = m_document->components();

    // Find the currently active component (or root)
    document::Component* active = nullptr;
    for (const auto& id : reg.componentIds()) {
        auto* c = reg.findComponent(id);
        if (c && c->isActive()) {
            active = c;
            break;
        }
    }
    if (!active)
        active = &reg.rootComponent();

    // Create a new child component and add an occurrence to the active component
    std::string newId = reg.createComponent("Component");
    std::string occId = active->addOccurrence(newId);
    m_document->setModified(true);

    m_mainWindow->statusBar()->showMessage(
        tr("New component created under %1").arg(QString::fromStdString(active->name())));
    m_mainWindow->refreshAllPanels();
}

void CommandController::onInsertComponent()
{
    QString filePath = QFileDialog::getOpenFileName(m_mainWindow,
        tr("Insert Component from .kcd"),
        QString(),
        tr("kernelCAD files (*.kcd);;All files (*)"));

    if (filePath.isEmpty())
        return;

    try {
        std::string occId = m_document->insertComponentFromFile(filePath.toStdString());
        m_mainWindow->statusBar()->showMessage(
            tr("Inserted component from %1 (occurrence %2)")
                .arg(QFileInfo(filePath).fileName())
                .arg(QString::fromStdString(occId)));
    } catch (const std::exception& e) {
        QMessageBox::warning(m_mainWindow, tr("Insert Failed"),
            tr("Could not insert component: %1").arg(e.what()));
    }

    m_mainWindow->refreshAllPanels();
}

void CommandController::onAddJoint()
{
    // If the joint creator is already active, cancel it
    if (m_mainWindow->jointCreator()->state() != JointCreator::State::Idle) {
        m_mainWindow->jointCreator()->cancel();
        return;
    }

    // Start interactive face-to-face joint creation (default: Rigid)
    m_mainWindow->jointCreator()->begin(features::JointType::Rigid);
}

// =============================================================================
// Delete selected feature
// =============================================================================

void CommandController::onDeleteSelectedFeature()
{
    // If something is selected in the viewport, try to find the corresponding feature.
    // For now, delete the last feature in the timeline (most common workflow).
    if (!m_selectionMgr->hasSelection()) {
        m_mainWindow->statusBar()->showMessage(tr("Nothing selected to delete"), 3000);
        return;
    }

    // Try to find a body-level feature from the selection
    const auto& hit = m_selectionMgr->selection().front();
    if (hit.bodyId.empty()) {
        m_mainWindow->statusBar()->showMessage(tr("No feature associated with selection"), 3000);
        return;
    }

    // The bodyId often corresponds to a feature id in the timeline
    QString featureId = QString::fromStdString(hit.bodyId);
    m_document->executeCommand(
        std::make_unique<document::DeleteFeatureCommand>(featureId.toStdString()));
    m_selectionMgr->clearSelection();
    m_properties->clear();
    m_mainWindow->refreshAllPanels();
    m_mainWindow->statusBar()->showMessage(tr("Feature deleted"));
}

// =============================================================================
// Measure tool
// =============================================================================

void CommandController::onMeasure()
{
    m_lastCommandName = tr("Measure");
    m_lastCommandCallback = [this]() { onMeasure(); };
    if (m_mainWindow->measureActive()) {
        // Toggle off
        m_mainWindow->setMeasureActive(false);
        m_mainWindow->measureTool()->reset();
        m_mainWindow->statusBar()->showMessage(tr("Measure tool deactivated"));
        return;
    }

    m_mainWindow->setMeasureActive(true);
    m_mainWindow->measureTool()->reset();
    m_mainWindow->measureTool()->setMode(MeasureTool::MeasureMode::PointToPoint);

    // Wire up selection clicks to the measure tool while active
    m_selectionMgr->setOnSelectionChanged([this](const std::vector<SelectionHit>& sel) {
        if (!m_mainWindow->measureActive() || sel.empty()) {
            onSelectionChanged();
            return;
        }

        const auto& hit = sel.front();

        if (!m_mainWindow->measureTool()->hasFirstEntity()) {
            m_mainWindow->measureTool()->setFirstEntity(hit);
            m_mainWindow->statusBar()->showMessage(tr("Measure: pick second point (M to cancel)"));
        } else {
            m_mainWindow->measureTool()->setSecondEntity(hit);
            // Result is emitted via measurementReady signal
            // Reset for next measurement
            m_mainWindow->measureTool()->reset();
            m_mainWindow->statusBar()->showMessage(tr("Measure: pick first point (M to cancel)"));
        }
    });

    m_mainWindow->statusBar()->showMessage(tr("Measure: pick first point (M to cancel)"));
}

// =============================================================================
// Property / selection changed
// =============================================================================

void CommandController::onPropertyChanged(const QString& featureId,
                                          const QString& propertyName,
                                          const QVariant& newValue)
{
    auto& tl = m_document->timeline();
    features::Feature* feat = nullptr;
    for (size_t i = 0; i < tl.count(); ++i) {
        auto& entry = tl.entry(i);
        if (entry.id == featureId.toStdString() ||
            (entry.feature && entry.feature->id() == featureId.toStdString()))
        {
            feat = entry.feature.get();
            break;
        }
    }
    if (!feat)
        return;

    using FT = features::FeatureType;
    switch (feat->type()) {
    case FT::Extrude: {
        auto* ef = static_cast<features::ExtrudeFeature*>(feat);
        auto& p = ef->params();
        if (propertyName == QLatin1String("Distance")) {
            std::ostringstream oss;
            oss << newValue.toDouble() << " mm";
            p.distanceExpr = oss.str();
        } else if (propertyName == QLatin1String("distanceExpr")) {
            p.distanceExpr = newValue.toString().toStdString();
        } else if (propertyName == QLatin1String("ExtentType")) {
            QString txt = newValue.toString();
            if (txt == QLatin1String("Distance"))    p.extentType = features::ExtentType::Distance;
            else if (txt == QLatin1String("ThroughAll"))  p.extentType = features::ExtentType::ThroughAll;
            else if (txt == QLatin1String("ToEntity"))    p.extentType = features::ExtentType::ToEntity;
            else if (txt == QLatin1String("Symmetric"))   p.extentType = features::ExtentType::Symmetric;
        } else if (propertyName == QLatin1String("Operation")) {
            QString txt = newValue.toString();
            if (txt == QLatin1String("NewBody"))      p.operation = features::FeatureOperation::NewBody;
            else if (txt == QLatin1String("Join"))    p.operation = features::FeatureOperation::Join;
            else if (txt == QLatin1String("Cut"))     p.operation = features::FeatureOperation::Cut;
            else if (txt == QLatin1String("Intersect")) p.operation = features::FeatureOperation::Intersect;
        } else if (propertyName == QLatin1String("TaperAngle")) {
            p.taperAngleDeg = newValue.toDouble();
        } else if (propertyName == QLatin1String("Symmetric")) {
            p.isSymmetric = newValue.toBool();
        }
        break;
    }
    case FT::Revolve: {
        auto* rf = static_cast<features::RevolveFeature*>(feat);
        auto& p = rf->params();
        if (propertyName == QLatin1String("Angle")) {
            std::ostringstream oss;
            oss << newValue.toDouble() << " deg";
            p.angleExpr = oss.str();
        } else if (propertyName == QLatin1String("angleExpr")) {
            p.angleExpr = newValue.toString().toStdString();
        } else if (propertyName == QLatin1String("AxisType")) {
            QString txt = newValue.toString();
            if (txt == QLatin1String("XAxis"))       p.axisType = features::AxisType::XAxis;
            else if (txt == QLatin1String("YAxis"))  p.axisType = features::AxisType::YAxis;
            else if (txt == QLatin1String("ZAxis"))  p.axisType = features::AxisType::ZAxis;
            else if (txt == QLatin1String("Custom")) p.axisType = features::AxisType::Custom;
        } else if (propertyName == QLatin1String("FullRevolution")) {
            p.isFullRevolution = newValue.toBool();
        }
        break;
    }
    case FT::Fillet: {
        auto* ff = static_cast<features::FilletFeature*>(feat);
        auto& p = ff->params();
        if (propertyName == QLatin1String("Radius")) {
            std::ostringstream oss;
            oss << newValue.toDouble() << " mm";
            p.radiusExpr = oss.str();
        } else if (propertyName == QLatin1String("radiusExpr")) {
            p.radiusExpr = newValue.toString().toStdString();
        }
        break;
    }
    case FT::Chamfer: {
        auto* cf = static_cast<features::ChamferFeature*>(feat);
        auto& p = cf->params();
        if (propertyName == QLatin1String("Distance")) {
            std::ostringstream oss;
            oss << newValue.toDouble() << " mm";
            p.distanceExpr = oss.str();
        } else if (propertyName == QLatin1String("distanceExpr")) {
            p.distanceExpr = newValue.toString().toStdString();
        } else if (propertyName == QLatin1String("ChamferType")) {
            QString txt = newValue.toString();
            if (txt == QLatin1String("EqualDistance"))      p.chamferType = features::ChamferType::EqualDistance;
            else if (txt == QLatin1String("TwoDistances"))  p.chamferType = features::ChamferType::TwoDistances;
            else if (txt == QLatin1String("DistanceAndAngle")) p.chamferType = features::ChamferType::DistanceAndAngle;
        }
        break;
    }
    case FT::Hole: {
        auto* hf = static_cast<features::HoleFeature*>(feat);
        auto& p = hf->params();
        if (propertyName == QLatin1String("Diameter")) {
            std::ostringstream oss;
            oss << newValue.toDouble() << " mm";
            p.diameterExpr = oss.str();
        } else if (propertyName == QLatin1String("diameterExpr")) {
            p.diameterExpr = newValue.toString().toStdString();
        } else if (propertyName == QLatin1String("Depth")) {
            std::ostringstream oss;
            oss << newValue.toDouble() << " mm";
            p.depthExpr = oss.str();
        } else if (propertyName == QLatin1String("depthExpr")) {
            p.depthExpr = newValue.toString().toStdString();
        } else if (propertyName == QLatin1String("TipAngle")) {
            p.tipAngleDeg = newValue.toDouble();
        }
        break;
    }
    case FT::Shell: {
        auto* sf = static_cast<features::ShellFeature*>(feat);
        auto& p = sf->params();
        if (propertyName == QLatin1String("Thickness")) {
            p.thicknessExpr = newValue.toDouble();
        }
        break;
    }
    default:
        break;
    }

    std::string fid = featureId.toStdString();

    // Rebuild dependency edges from the edited feature's updated params
    // before recomputing, so the dirty set includes any newly-referenced bodies.
    m_document->updateDependenciesFromParams(fid);

    if (!m_mainWindow->editingFeatureId().isEmpty()) {
        // During edit mode, use incremental recompute from the edited feature
        // for better performance -- only downstream features are recalculated.
        m_document->recomputeFrom(fid);
        m_mainWindow->updateViewport();
    } else {
        // Use the preview engine for live geometry updates instead of full recompute.
        // beginPreview saves the original params; updatePreview re-executes just
        // this feature and pushes a semi-transparent mesh to the viewport.
        if (!m_mainWindow->previewEngine()->isActive() || m_mainWindow->previewEngine()->activeFeatureId() != fid)
            m_mainWindow->previewEngine()->beginPreview(fid);
        m_mainWindow->previewEngine()->updatePreview();
    }
}

void CommandController::onSelectionChanged()
{
    // Route selection to JointCreator if it's active
    if (m_mainWindow->jointCreator() && m_mainWindow->jointCreator()->state() != JointCreator::State::Idle
        && m_selectionMgr->hasSelection()) {
        const auto& hit = m_selectionMgr->selection().front();
        m_mainWindow->jointCreator()->onFaceSelected(hit);
        return;
    }

    // Handle pending sketch plane selection
    if (m_pendingSketchPlane && m_selectionMgr->hasSelection()) {
        const auto& hit = m_selectionMgr->selection().front();
        handleSketchPlaneSelection(hit);
        return;
    }

    if (!m_selectionMgr->hasSelection()) {
        m_properties->clear();
        // Clear single-click highlights from tree selection
        if (m_mainWindow->editingFeatureId().isEmpty()) {
            m_mainWindow->hideManipulator();
            m_viewport->setHighlightedSketch(nullptr);
            m_viewport->setHighlightedFaces({});
        }
        m_mainWindow->updateStatusBarInfo();
        m_mainWindow->statusBar()->showMessage(tr("Selection cleared"));
        return;
    }

    const auto& sel = m_selectionMgr->selection();
    const auto& hit = sel.front();

    // Body-level selection (no face or edge): show rich body properties panel
    if (!hit.bodyId.empty() && hit.faceIndex < 0 && hit.edgeIndex < 0) {
        m_properties->showBodyProperties(hit.bodyId);
        m_mainWindow->updateStatusBarInfo();
        m_mainWindow->statusBar()->showMessage(
            tr("Selected body %1").arg(QString::fromStdString(hit.bodyId)));
        return;
    }

    // Build a properties display for the selected entity
    std::vector<std::tuple<QString, QString, QVariant>> props;

    if (!hit.bodyId.empty()) {
        props.emplace_back(tr("Body ID"), QStringLiteral("string"),
                           QString::fromStdString(hit.bodyId));

        // Show body material
        const auto& bodyMat = m_document->appearances().bodyMaterial(hit.bodyId);
        props.emplace_back(tr("Body Material"), QStringLiteral("string"),
                           QString::fromStdString(bodyMat.name));
    }

    if (hit.faceIndex >= 0) {
        props.emplace_back(tr("Face Index"), QStringLiteral("int"),
                           hit.faceIndex);

        // Show face material (override or inherited)
        if (!hit.bodyId.empty()) {
            const auto& faceMat = m_document->appearances().faceMaterial(
                hit.bodyId, hit.faceIndex);
            bool hasOverride = m_document->appearances().hasFaceOverride(
                hit.bodyId, hit.faceIndex);
            QString matLabel = QString::fromStdString(faceMat.name);
            if (hasOverride)
                matLabel += QStringLiteral(" (override)");
            props.emplace_back(tr("Face Material"), QStringLiteral("string"),
                               matLabel);
        }
    }

    if (hit.edgeIndex >= 0) {
        props.emplace_back(tr("Edge Index"), QStringLiteral("int"),
                           hit.edgeIndex);
    }

    props.emplace_back(tr("World X"), QStringLiteral("double"),
                       static_cast<double>(hit.worldX));
    props.emplace_back(tr("World Y"), QStringLiteral("double"),
                       static_cast<double>(hit.worldY));
    props.emplace_back(tr("World Z"), QStringLiteral("double"),
                       static_cast<double>(hit.worldZ));
    props.emplace_back(tr("Depth"), QStringLiteral("double"),
                       static_cast<double>(hit.depth));

    // --- B-Rep topology info via BRepQuery ---
    if (!hit.bodyId.empty()) {
        auto& brep = m_document->brepModel();
        if (brep.hasBody(hit.bodyId)) {
            try {
                kernel::BRepQuery bq = brep.query(hit.bodyId);

                // Face-level topology info
                if (hit.faceIndex >= 0 && hit.faceIndex < bq.faceCount()) {
                    kernel::FaceInfo fi = bq.faceInfo(hit.faceIndex);

                    props.emplace_back(tr("--- Face Topology ---"), QStringLiteral("label"),
                                       QString());
                    props.emplace_back(tr("Surface Type"), QStringLiteral("string"),
                                       QString::fromLatin1(kernel::surfaceTypeName(fi.surfaceType)));
                    props.emplace_back(tr("Face Area"), QStringLiteral("string"),
                                       QString::number(fi.area, 'f', 4) + QStringLiteral(" mm2"));
                    props.emplace_back(tr("Normal"), QStringLiteral("string"),
                                       QStringLiteral("(%1, %2, %3)")
                                           .arg(fi.normalX, 0, 'f', 4)
                                           .arg(fi.normalY, 0, 'f', 4)
                                           .arg(fi.normalZ, 0, 'f', 4));
                    props.emplace_back(tr("Centroid"), QStringLiteral("string"),
                                       QStringLiteral("(%1, %2, %3)")
                                           .arg(fi.centroidX, 0, 'f', 4)
                                           .arg(fi.centroidY, 0, 'f', 4)
                                           .arg(fi.centroidZ, 0, 'f', 4));
                    props.emplace_back(tr("Edge Count"), QStringLiteral("int"),
                                       fi.edgeCount);
                    props.emplace_back(tr("Loop Count"), QStringLiteral("int"),
                                       fi.loopCount);
                    props.emplace_back(tr("Reversed"), QStringLiteral("string"),
                                       fi.isReversed ? QStringLiteral("Yes") : QStringLiteral("No"));

                    // Adjacent faces
                    auto adj = bq.adjacentFaces(hit.faceIndex);
                    if (!adj.empty()) {
                        QStringList adjList;
                        for (int idx : adj) adjList.append(QString::number(idx));
                        props.emplace_back(tr("Adjacent Faces"), QStringLiteral("string"),
                                           adjList.join(QStringLiteral(", ")));
                    }
                }

                // Edge-level topology info
                if (hit.edgeIndex >= 0 && hit.edgeIndex < bq.edgeCount()) {
                    kernel::EdgeInfo ei = bq.edgeInfo(hit.edgeIndex);

                    props.emplace_back(tr("--- Edge Topology ---"), QStringLiteral("label"),
                                       QString());
                    props.emplace_back(tr("Curve Type"), QStringLiteral("string"),
                                       QString::fromLatin1(kernel::curveTypeName(ei.curveType)));
                    props.emplace_back(tr("Edge Length"), QStringLiteral("string"),
                                       QString::number(ei.length, 'f', 4) + QStringLiteral(" mm"));
                    props.emplace_back(tr("Convexity"), QStringLiteral("string"),
                                       QString::fromLatin1(kernel::edgeConvexityName(ei.convexity)));
                    props.emplace_back(tr("Start"), QStringLiteral("string"),
                                       QStringLiteral("(%1, %2, %3)")
                                           .arg(ei.startX, 0, 'f', 4)
                                           .arg(ei.startY, 0, 'f', 4)
                                           .arg(ei.startZ, 0, 'f', 4));
                    props.emplace_back(tr("End"), QStringLiteral("string"),
                                       QStringLiteral("(%1, %2, %3)")
                                           .arg(ei.endX, 0, 'f', 4)
                                           .arg(ei.endY, 0, 'f', 4)
                                           .arg(ei.endZ, 0, 'f', 4));
                    if (ei.adjacentFace1 >= 0)
                        props.emplace_back(tr("Adj. Face 1"), QStringLiteral("int"),
                                           ei.adjacentFace1);
                    if (ei.adjacentFace2 >= 0)
                        props.emplace_back(tr("Adj. Face 2"), QStringLiteral("int"),
                                           ei.adjacentFace2);
                    if (ei.isSeam)
                        props.emplace_back(tr("Seam Edge"), QStringLiteral("string"),
                                           QStringLiteral("Yes"));
                    if (ei.isDegenerate)
                        props.emplace_back(tr("Degenerate"), QStringLiteral("string"),
                                           QStringLiteral("Yes"));
                }
            } catch (...) {
                // BRepQuery may throw on degenerate shapes; silently skip topology info
            }

            // Physical properties for the selected body
            auto phys = brep.getProperties(hit.bodyId);

            props.emplace_back(tr("--- Physical Properties ---"), QStringLiteral("label"),
                               QString());

            props.emplace_back(tr("Volume"), QStringLiteral("string"),
                               QString::number(phys.volume, 'f', 4) + QStringLiteral(" mm3"));
            props.emplace_back(tr("Surface Area"), QStringLiteral("string"),
                               QString::number(phys.surfaceArea, 'f', 4) + QStringLiteral(" mm2"));
            props.emplace_back(tr("Mass (steel)"), QStringLiteral("string"),
                               QString::number(phys.mass, 'f', 4) + QStringLiteral(" g"));
            props.emplace_back(tr("CoG X"), QStringLiteral("string"),
                               QString::number(phys.cogX, 'f', 4) + QStringLiteral(" mm"));
            props.emplace_back(tr("CoG Y"), QStringLiteral("string"),
                               QString::number(phys.cogY, 'f', 4) + QStringLiteral(" mm"));
            props.emplace_back(tr("CoG Z"), QStringLiteral("string"),
                               QString::number(phys.cogZ, 'f', 4) + QStringLiteral(" mm"));
            props.emplace_back(tr("Bbox Width (X)"), QStringLiteral("string"),
                               QString::number(phys.bboxMaxX - phys.bboxMinX, 'f', 4) + QStringLiteral(" mm"));
            props.emplace_back(tr("Bbox Height (Y)"), QStringLiteral("string"),
                               QString::number(phys.bboxMaxY - phys.bboxMinY, 'f', 4) + QStringLiteral(" mm"));
            props.emplace_back(tr("Bbox Depth (Z)"), QStringLiteral("string"),
                               QString::number(phys.bboxMaxZ - phys.bboxMinZ, 'f', 4) + QStringLiteral(" mm"));
        }
    }

    m_properties->setProperties(props);

    // Add material dropdown when a body is selected
    if (!hit.bodyId.empty()) {
        const auto& bodyMat = m_document->appearances().bodyMaterial(hit.bodyId);
        m_properties->addMaterialDropdown(
            QString::fromStdString(hit.bodyId),
            QString::fromStdString(bodyMat.name));
    }

    // Status bar
    QString msg;
    if (sel.size() == 1) {
        if (hit.edgeIndex >= 0) {
            msg = tr("Selected edge %1").arg(hit.edgeIndex);
        } else {
            msg = tr("Selected face %1").arg(hit.faceIndex);
        }
        if (!hit.bodyId.empty())
            msg += tr(" on body %1").arg(QString::fromStdString(hit.bodyId));
    } else {
        msg = tr("Selected %1 entities").arg(sel.size());
    }
    m_mainWindow->statusBar()->showMessage(msg);

    // Refresh status bar center (body count + selection count)
    m_mainWindow->updateStatusBarInfo();
}

void CommandController::clearPendingState()
{
    m_pendingSketchPlane = false;
    m_pendingCommand = PendingCommand::None;
    m_selectionMgr->clearSelection();
    m_selectionMgr->setFilter(SelectionFilter::All);
    if (m_mainWindow->selectAllAction()) m_mainWindow->selectAllAction()->setChecked(true);
    m_mainWindow->hideConfirmBar();
}
