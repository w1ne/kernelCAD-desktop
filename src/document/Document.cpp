#include "Document.h"
#include "Serializer.h"
#include "../kernel/StableReference.h"
#include <TopoDS_Shape.hxx>
#include <BRep_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <functional>

namespace document {

Document::Document()
    : m_timeline(std::make_unique<Timeline>())
    , m_paramStore(std::make_unique<ParameterStore>())
    , m_kernel(std::make_unique<kernel::OCCTKernel>())
    , m_brepModel(std::make_unique<kernel::BRepModel>())
{
    // Create the root component for the design
    m_components.createComponent("Root");
}

Document::~Document() = default;

void Document::newDocument()
{
    m_name = "Untitled";
    m_modified = false;
    m_timeline = std::make_unique<Timeline>();
    m_paramStore = std::make_unique<ParameterStore>();
    m_brepModel = std::make_unique<kernel::BRepModel>();
    m_nextBodyCounter = 1;
    m_history.clear();
    m_depGraph.clear();
    m_bodyToFeature.clear();
    m_lastGoodShapes.clear();
    m_erroredFeatureIds.clear();
    m_featureInputHashes.clear();
    m_components.clear();
    m_components.createComponent("Root");
    m_appearances.clear();
}

bool Document::save(const std::string& path)
{
    if (!Serializer::save(*this, path))
        return false;
    m_modified = false;
    return true;
}

bool Document::load(const std::string& path)
{
    // Serializer::load clears the doc, rebuilds features, and calls recompute()
    return Serializer::load(*this, path);
}

int Document::importFile(const std::string& path)
{
    // Determine format from extension
    std::string ext;
    auto dotPos = path.rfind('.');
    if (dotPos != std::string::npos)
        ext = path.substr(dotPos);
    // Lowercase
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::vector<TopoDS_Shape> shapes;
    if (ext == ".step" || ext == ".stp")
        shapes = m_kernel->importSTEP(path);
    else if (ext == ".igs" || ext == ".iges")
        shapes = m_kernel->importIGES(path);
    else
        throw std::runtime_error("Unsupported file format: " + ext);

    int imported = 0;
    for (const auto& shape : shapes) {
        std::string bodyId = "imported_" + std::to_string(m_nextBodyCounter++);
        m_brepModel->addBody(bodyId, shape);
        ++imported;
    }

    m_modified = true;
    return imported;
}

void Document::registerBodyFeature(const std::string& bodyId, const std::string& featureId)
{
    m_bodyToFeature[bodyId] = featureId;
}

std::string Document::featureForBody(const std::string& bodyId) const
{
    auto it = m_bodyToFeature.find(bodyId);
    if (it != m_bodyToFeature.end())
        return it->second;
    return {};
}

std::string Document::generateFeatureName(features::FeatureType type) const
{
    // Map FeatureType to its base display name
    static const std::unordered_map<int, const char*> nameMap = {
        {(int)features::FeatureType::Extrude,            "Extrude"},
        {(int)features::FeatureType::Revolve,            "Revolve"},
        {(int)features::FeatureType::Fillet,             "Fillet"},
        {(int)features::FeatureType::Chamfer,            "Chamfer"},
        {(int)features::FeatureType::Shell,              "Shell"},
        {(int)features::FeatureType::Loft,               "Loft"},
        {(int)features::FeatureType::Sweep,              "Sweep"},
        {(int)features::FeatureType::Hole,               "Hole"},
        {(int)features::FeatureType::Thread,             "Thread"},
        {(int)features::FeatureType::RectangularPattern, "Rectangular Pattern"},
        {(int)features::FeatureType::CircularPattern,    "Circular Pattern"},
        {(int)features::FeatureType::Mirror,             "Mirror"},
        {(int)features::FeatureType::Draft,              "Draft"},
        {(int)features::FeatureType::Scale,              "Scale"},
        {(int)features::FeatureType::SplitBody,          "Split Body"},
        {(int)features::FeatureType::Combine,            "Combine"},
        {(int)features::FeatureType::OffsetFaces,        "Offset Faces"},
        {(int)features::FeatureType::Move,               "Move"},
        {(int)features::FeatureType::Thicken,            "Thicken"},
        {(int)features::FeatureType::Sketch,             "Sketch"},
        {(int)features::FeatureType::ConstructionPlane,  "Construction Plane"},
        {(int)features::FeatureType::ConstructionAxis,   "Construction Axis"},
        {(int)features::FeatureType::ConstructionPoint,  "Construction Point"},
        {(int)features::FeatureType::PathPattern,        "Path Pattern"},
        {(int)features::FeatureType::Coil,               "Coil"},
        {(int)features::FeatureType::DeleteFace,         "Delete Face"},
        {(int)features::FeatureType::ReplaceFace,        "Replace Face"},
        {(int)features::FeatureType::ReverseNormal,      "Reverse Normal"},
        {(int)features::FeatureType::Joint,              "Joint"},
        {(int)features::FeatureType::BaseFeature,        "Feature"},
    };

    auto it = nameMap.find(static_cast<int>(type));
    std::string baseName = (it != nameMap.end()) ? it->second : "Feature";

    // Count existing features of this type in the timeline
    int count = 0;
    for (size_t i = 0; i < m_timeline->count(); ++i) {
        if (m_timeline->entry(i).feature &&
            m_timeline->entry(i).feature->type() == type)
            ++count;
    }

    // Number starts at 1 for the first instance
    return baseName + " " + std::to_string(count + 1);
}

void Document::appendFeatureToTimeline(std::shared_ptr<features::Feature> feature)
{
    std::string numberedName = generateFeatureName(feature->type());

    // If the timeline marker is not at the end (user has rolled back),
    // insert the new feature at the marker position — this is how Fusion
    // works: new features go where the marker is, not at the end.
    size_t markerPos = m_timeline->markerPosition();
    if (markerPos < m_timeline->count()) {
        document::TimelineEntry entry;
        entry.id = feature->id();
        entry.name = numberedName;
        entry.feature = std::move(feature);
        m_timeline->insert(markerPos, std::move(entry));
    } else {
        m_timeline->append(std::move(feature));
        m_timeline->entry(m_timeline->count() - 1).name = numberedName;
    }
}

std::string Document::addExtrude(features::ExtrudeParams params)
{
    // Generate unique IDs
    std::ostringstream featureIdStream;
    featureIdStream << "extrude_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();

    std::ostringstream bodyIdStream;
    bodyIdStream << "body_" << m_nextBodyCounter;
    std::string bodyId = bodyIdStream.str();

    m_nextBodyCounter++;

    // Create the feature and execute it
    auto feature = std::make_shared<features::ExtrudeFeature>(featureId, params);

    // Resolve sketch if sketchId is set
    const sketch::Sketch* sketchPtr = nullptr;
    features::SketchFeature* sketchFeat = nullptr;
    if (!params.sketchId.empty()) {
        sketchFeat = findSketch(params.sketchId);
        if (sketchFeat)
            sketchPtr = &sketchFeat->sketch();
    }

    TopoDS_Shape shape = feature->execute(*m_kernel, sketchPtr);

    // Store the resulting body in the BRepModel
    m_brepModel->addBody(bodyId, shape);

    // Build dependency graph edges
    m_depGraph.addNode(featureId);
    if (!params.sketchId.empty())
        m_depGraph.addEdge(params.sketchId, featureId);
    if (!params.targetBodyId.empty()) {
        std::string depFeat = featureForBody(params.targetBodyId);
        if (!depFeat.empty())
            m_depGraph.addEdge(depFeat, featureId);
    }

    // Register body -> feature mapping
    registerBodyFeature(bodyId, featureId);

    // Track body in root component
    m_components.rootComponent().addBodyRef(bodyId);

    // Append to the timeline
    appendFeatureToTimeline(feature);

    m_modified = true;
    return bodyId;
}

std::string Document::addRevolve(features::RevolveParams params)
{
    std::ostringstream featureIdStream;
    featureIdStream << "revolve_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();

    std::ostringstream bodyIdStream;
    bodyIdStream << "body_" << m_nextBodyCounter;
    std::string bodyId = bodyIdStream.str();

    m_nextBodyCounter++;

    auto feature = std::make_shared<features::RevolveFeature>(featureId, params);

    // Resolve sketch if sketchId is set
    const sketch::Sketch* sketchPtr = nullptr;
    features::SketchFeature* sketchFeat = nullptr;
    if (!params.sketchId.empty()) {
        sketchFeat = findSketch(params.sketchId);
        if (sketchFeat)
            sketchPtr = &sketchFeat->sketch();
    }

    TopoDS_Shape shape = feature->execute(*m_kernel, sketchPtr);

    m_brepModel->addBody(bodyId, shape);

    // Dependency graph
    m_depGraph.addNode(featureId);
    if (!feature->params().sketchId.empty())
        m_depGraph.addEdge(feature->params().sketchId, featureId);

    registerBodyFeature(bodyId, featureId);

    // Track body in root component
    m_components.rootComponent().addBodyRef(bodyId);

    appendFeatureToTimeline(feature);

    m_modified = true;
    return bodyId;
}

std::string Document::addFillet(features::FilletParams params)
{
    // Look up the target body shape
    std::string targetId = params.targetBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("Fillet: target body '" + targetId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

    // Capture stable edge signatures at creation time
    params.edgeSignatures.clear();
    for (int idx : params.edgeIds) {
        params.edgeSignatures.push_back(
            kernel::StableReference::computeEdgeSignature(targetShape, idx));
    }

    std::ostringstream featureIdStream;
    featureIdStream << "fillet_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();

    m_nextBodyCounter++;

    auto feature = std::make_shared<features::FilletFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, targetShape);

    // Replace the target body with the filleted shape
    m_brepModel->addBody(targetId, result);

    // Dependency graph: fillet depends on the feature that created the target body
    m_depGraph.addNode(featureId);
    std::string depFeat = featureForBody(targetId);
    if (!depFeat.empty())
        m_depGraph.addEdge(depFeat, featureId);

    // This feature now owns the body
    registerBodyFeature(targetId, featureId);

    appendFeatureToTimeline(feature);

    m_modified = true;
    return targetId;
}

std::string Document::addChamfer(features::ChamferParams params)
{
    // Look up the target body shape
    std::string targetId = params.targetBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("Chamfer: target body '" + targetId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

    // Capture stable edge signatures at creation time
    params.edgeSignatures.clear();
    for (int idx : params.edgeIds) {
        params.edgeSignatures.push_back(
            kernel::StableReference::computeEdgeSignature(targetShape, idx));
    }

    std::ostringstream featureIdStream;
    featureIdStream << "chamfer_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();

    m_nextBodyCounter++;

    auto feature = std::make_shared<features::ChamferFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, targetShape);

    // Replace the target body with the chamfered shape
    m_brepModel->addBody(targetId, result);

    // Dependency graph
    m_depGraph.addNode(featureId);
    std::string depFeat = featureForBody(targetId);
    if (!depFeat.empty())
        m_depGraph.addEdge(depFeat, featureId);

    registerBodyFeature(targetId, featureId);

    appendFeatureToTimeline(feature);

    m_modified = true;
    return targetId;
}

std::string Document::addSketch(features::SketchParams params)
{
    // Resolve standard plane IDs from the document origin
    if (params.planeId == "XY" || params.planeId == "XZ" || params.planeId == "YZ") {
        const features::ConstructionPlane* plane = nullptr;
        if (params.planeId == "XY")
            plane = &m_origin.xyPlane();
        else if (params.planeId == "XZ")
            plane = &m_origin.xzPlane();
        else if (params.planeId == "YZ")
            plane = &m_origin.yzPlane();

        if (plane) {
            plane->origin(params.originX, params.originY, params.originZ);
            plane->xDirection(params.xDirX, params.xDirY, params.xDirZ);
            double ydx, ydy, ydz;
            plane->yDirection(ydx, ydy, ydz);
            params.yDirX = ydx;
            params.yDirY = ydy;
            params.yDirZ = ydz;
        }
    }

    std::ostringstream featureIdStream;
    featureIdStream << "sketch_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();
    m_nextBodyCounter++;

    auto feature = std::make_shared<features::SketchFeature>(featureId, std::move(params));

    // Solve the sketch (resolves constraints on any pre-added geometry)
    feature->sketch().solve();

    // Dependency graph: sketch has no dependencies (root node)
    m_depGraph.addNode(featureId);

    appendFeatureToTimeline(feature);
    m_modified = true;
    return featureId;
}

std::string Document::addSweep(features::SweepParams params)
{
    std::ostringstream featureIdStream;
    featureIdStream << "sweep_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();

    std::ostringstream bodyIdStream;
    bodyIdStream << "body_" << m_nextBodyCounter;
    std::string bodyId = bodyIdStream.str();

    m_nextBodyCounter++;

    auto feature = std::make_shared<features::SweepFeature>(featureId, std::move(params));

    // Resolve profile and path sketches
    const sketch::Sketch* profileSketchPtr = nullptr;
    const sketch::Sketch* pathSketchPtr = nullptr;
    if (!feature->params().sketchId.empty()) {
        auto* sketchFeat = findSketch(feature->params().sketchId);
        if (sketchFeat)
            profileSketchPtr = &sketchFeat->sketch();
    }
    if (!feature->params().pathSketchId.empty()) {
        auto* sketchFeat = findSketch(feature->params().pathSketchId);
        if (sketchFeat)
            pathSketchPtr = &sketchFeat->sketch();
    }

    TopoDS_Shape shape = feature->execute(*m_kernel, profileSketchPtr, pathSketchPtr);

    m_brepModel->addBody(bodyId, shape);

    // Dependency graph
    m_depGraph.addNode(featureId);
    if (!feature->params().sketchId.empty())
        m_depGraph.addEdge(feature->params().sketchId, featureId);
    if (!feature->params().pathSketchId.empty())
        m_depGraph.addEdge(feature->params().pathSketchId, featureId);

    registerBodyFeature(bodyId, featureId);

    // Track body in root component
    m_components.rootComponent().addBodyRef(bodyId);

    appendFeatureToTimeline(feature);

    m_modified = true;
    return bodyId;
}

std::string Document::addLoft(features::LoftParams params)
{
    std::ostringstream featureIdStream;
    featureIdStream << "loft_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();

    std::ostringstream bodyIdStream;
    bodyIdStream << "body_" << m_nextBodyCounter;
    std::string bodyId = bodyIdStream.str();

    m_nextBodyCounter++;

    auto feature = std::make_shared<features::LoftFeature>(featureId, std::move(params));

    // Resolve section sketches
    std::vector<const sketch::Sketch*> sketches;
    for (const auto& sketchId : feature->params().sectionSketchIds) {
        auto* sketchFeat = findSketch(sketchId);
        sketches.push_back(sketchFeat ? &sketchFeat->sketch() : nullptr);
    }

    TopoDS_Shape shape = feature->execute(*m_kernel, sketches);

    m_brepModel->addBody(bodyId, shape);

    // Dependency graph
    m_depGraph.addNode(featureId);
    for (const auto& sketchId : feature->params().sectionSketchIds) {
        if (!sketchId.empty())
            m_depGraph.addEdge(sketchId, featureId);
    }

    registerBodyFeature(bodyId, featureId);

    // Track body in root component
    m_components.rootComponent().addBodyRef(bodyId);

    appendFeatureToTimeline(feature);

    m_modified = true;
    return bodyId;
}

std::string Document::addShell(features::ShellParams params)
{
    std::string targetId = params.targetBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("Shell: target body '" + targetId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

    // Capture stable face signatures for removed faces
    params.faceSignatures.clear();
    for (int idx : params.removedFaceIds) {
        params.faceSignatures.push_back(
            kernel::StableReference::computeFaceSignature(targetShape, idx));
    }

    std::ostringstream featureIdStream;
    featureIdStream << "shell_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();

    m_nextBodyCounter++;

    auto feature = std::make_shared<features::ShellFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, targetShape);

    // Replace the target body with the shelled shape
    m_brepModel->addBody(targetId, result);

    // Dependency graph
    m_depGraph.addNode(featureId);
    std::string depFeat = featureForBody(targetId);
    if (!depFeat.empty())
        m_depGraph.addEdge(depFeat, featureId);

    registerBodyFeature(targetId, featureId);

    appendFeatureToTimeline(feature);

    m_modified = true;
    return targetId;
}

std::string Document::addMirror(features::MirrorParams params)
{
    std::string targetId = params.targetBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("Mirror: target body '" + targetId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

    std::ostringstream featureIdStream;
    featureIdStream << "mirror_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();
    m_nextBodyCounter++;

    auto feature = std::make_shared<features::MirrorFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, targetShape);

    m_brepModel->addBody(targetId, result);

    // Dependency graph
    m_depGraph.addNode(featureId);
    std::string depFeat = featureForBody(targetId);
    if (!depFeat.empty())
        m_depGraph.addEdge(depFeat, featureId);

    registerBodyFeature(targetId, featureId);

    appendFeatureToTimeline(feature);

    m_modified = true;
    return targetId;
}

std::string Document::addRectangularPattern(features::RectangularPatternParams params)
{
    std::string targetId = params.targetBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("RectangularPattern: target body '" + targetId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

    std::ostringstream featureIdStream;
    featureIdStream << "rectpattern_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();
    m_nextBodyCounter++;

    auto feature = std::make_shared<features::RectangularPatternFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, targetShape);

    m_brepModel->addBody(targetId, result);

    // Dependency graph
    m_depGraph.addNode(featureId);
    std::string depFeat = featureForBody(targetId);
    if (!depFeat.empty())
        m_depGraph.addEdge(depFeat, featureId);

    registerBodyFeature(targetId, featureId);

    appendFeatureToTimeline(feature);

    m_modified = true;
    return targetId;
}

std::string Document::addCircularPattern(features::CircularPatternParams params)
{
    std::string targetId = params.targetBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("CircularPattern: target body '" + targetId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

    std::ostringstream featureIdStream;
    featureIdStream << "circpattern_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();
    m_nextBodyCounter++;

    auto feature = std::make_shared<features::CircularPatternFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, targetShape);

    m_brepModel->addBody(targetId, result);

    // Dependency graph
    m_depGraph.addNode(featureId);
    std::string depFeat = featureForBody(targetId);
    if (!depFeat.empty())
        m_depGraph.addEdge(depFeat, featureId);

    registerBodyFeature(targetId, featureId);

    appendFeatureToTimeline(feature);

    m_modified = true;
    return targetId;
}

std::string Document::addHole(features::HoleParams params)
{
    std::string targetId = params.targetBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("Hole: target body '" + targetId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

    std::ostringstream featureIdStream;
    featureIdStream << "hole_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();

    m_nextBodyCounter++;

    auto feature = std::make_shared<features::HoleFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, targetShape);

    // Replace the target body with the holed shape
    m_brepModel->addBody(targetId, result);

    // Dependency graph
    m_depGraph.addNode(featureId);
    std::string depFeat = featureForBody(targetId);
    if (!depFeat.empty())
        m_depGraph.addEdge(depFeat, featureId);

    registerBodyFeature(targetId, featureId);

    appendFeatureToTimeline(feature);

    m_modified = true;
    return targetId;
}

std::string Document::addCombine(features::CombineParams params)
{
    std::string targetId = params.targetBodyId;
    std::string toolId   = params.toolBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("Combine: target body '" + targetId + "' not found");
    if (!m_brepModel->hasBody(toolId))
        throw std::runtime_error("Combine: tool body '" + toolId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);
    TopoDS_Shape toolShape   = m_brepModel->getShape(toolId);

    std::ostringstream featureIdStream;
    featureIdStream << "combine_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();
    m_nextBodyCounter++;

    auto feature = std::make_shared<features::CombineFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, targetShape, toolShape);

    // Replace the target body with the combined result
    m_brepModel->addBody(targetId, result);

    // Remove the tool body unless keepToolBody is set
    if (!feature->params().keepToolBody)
        m_brepModel->removeBody(toolId);

    // Dependency graph
    m_depGraph.addNode(featureId);
    std::string depTarget = featureForBody(targetId);
    if (!depTarget.empty())
        m_depGraph.addEdge(depTarget, featureId);
    std::string depTool = featureForBody(toolId);
    if (!depTool.empty())
        m_depGraph.addEdge(depTool, featureId);

    registerBodyFeature(targetId, featureId);

    appendFeatureToTimeline(feature);

    m_modified = true;
    return targetId;
}

std::string Document::addSplitBody(features::SplitBodyParams params)
{
    std::string targetId = params.targetBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("SplitBody: target body '" + targetId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

    std::ostringstream featureIdStream;
    featureIdStream << "splitbody_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();
    m_nextBodyCounter++;

    auto feature = std::make_shared<features::SplitBodyFeature>(featureId, std::move(params));

    TopoDS_Shape result;
    if (feature->params().usePlane) {
        result = feature->execute(*m_kernel, targetShape);
    } else {
        // Use a tool body for splitting
        std::string toolId = feature->params().splittingToolId;
        if (!m_brepModel->hasBody(toolId))
            throw std::runtime_error("SplitBody: splitting tool body '" + toolId + "' not found");
        TopoDS_Shape toolShape = m_brepModel->getShape(toolId);
        result = feature->execute(*m_kernel, targetShape, toolShape);
    }

    // Replace the target body with the split result (compound)
    m_brepModel->addBody(targetId, result);

    // Dependency graph
    m_depGraph.addNode(featureId);
    std::string depFeat = featureForBody(targetId);
    if (!depFeat.empty())
        m_depGraph.addEdge(depFeat, featureId);
    if (!feature->params().usePlane && !feature->params().splittingToolId.empty()) {
        std::string depTool = featureForBody(feature->params().splittingToolId);
        if (!depTool.empty())
            m_depGraph.addEdge(depTool, featureId);
    }

    registerBodyFeature(targetId, featureId);

    appendFeatureToTimeline(feature);

    m_modified = true;
    return targetId;
}

std::string Document::addOffsetFaces(features::OffsetFacesParams params)
{
    std::string targetId = params.targetBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("OffsetFaces: target body '" + targetId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

    // Capture stable face signatures for offset faces
    params.faceSignatures.clear();
    for (int idx : params.faceIndices) {
        params.faceSignatures.push_back(
            kernel::StableReference::computeFaceSignature(targetShape, idx));
    }

    std::ostringstream featureIdStream;
    featureIdStream << "offsetfaces_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();
    m_nextBodyCounter++;

    auto feature = std::make_shared<features::OffsetFacesFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, targetShape);

    // Replace the target body with the offset result
    m_brepModel->addBody(targetId, result);

    // Dependency graph
    m_depGraph.addNode(featureId);
    std::string depFeat = featureForBody(targetId);
    if (!depFeat.empty())
        m_depGraph.addEdge(depFeat, featureId);

    registerBodyFeature(targetId, featureId);

    appendFeatureToTimeline(feature);

    m_modified = true;
    return targetId;
}

std::string Document::addMove(features::MoveParams params)
{
    std::string targetId = params.targetBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("Move: target body '" + targetId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

    std::ostringstream featureIdStream;
    featureIdStream << "move_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();
    m_nextBodyCounter++;

    auto feature = std::make_shared<features::MoveFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, targetShape);

    if (feature->params().createCopy) {
        // Keep the original, store the moved copy as a new body
        std::ostringstream bodyIdStream;
        bodyIdStream << "body_" << m_nextBodyCounter;
        std::string newBodyId = bodyIdStream.str();
        m_nextBodyCounter++;

        m_brepModel->addBody(newBodyId, result);

        // Dependency graph
        m_depGraph.addNode(featureId);
        std::string depFeat = featureForBody(targetId);
        if (!depFeat.empty())
            m_depGraph.addEdge(depFeat, featureId);

        registerBodyFeature(newBodyId, featureId);

        appendFeatureToTimeline(feature);

        m_modified = true;
        return newBodyId;
    } else {
        // Replace the target body with the moved shape
        m_brepModel->addBody(targetId, result);

        // Dependency graph
        m_depGraph.addNode(featureId);
        std::string depFeat = featureForBody(targetId);
        if (!depFeat.empty())
            m_depGraph.addEdge(depFeat, featureId);

        registerBodyFeature(targetId, featureId);

        appendFeatureToTimeline(feature);

        m_modified = true;
        return targetId;
    }
}

std::string Document::addDraft(features::DraftParams params)
{
    std::string targetId = params.targetBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("Draft: target body '" + targetId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

    // Capture stable face signatures for draft faces
    params.faceSignatures.clear();
    for (int idx : params.faceIndices) {
        params.faceSignatures.push_back(
            kernel::StableReference::computeFaceSignature(targetShape, idx));
    }

    std::ostringstream featureIdStream;
    featureIdStream << "draft_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();
    m_nextBodyCounter++;

    auto feature = std::make_shared<features::DraftFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, targetShape);

    // Replace the target body with the drafted shape
    m_brepModel->addBody(targetId, result);

    // Dependency graph
    m_depGraph.addNode(featureId);
    std::string depFeat = featureForBody(targetId);
    if (!depFeat.empty())
        m_depGraph.addEdge(depFeat, featureId);

    registerBodyFeature(targetId, featureId);

    appendFeatureToTimeline(feature);

    m_modified = true;
    return targetId;
}

std::string Document::addThicken(features::ThickenParams params)
{
    std::string targetId = params.targetBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("Thicken: target body '" + targetId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

    std::ostringstream featureIdStream;
    featureIdStream << "thicken_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();
    m_nextBodyCounter++;

    auto feature = std::make_shared<features::ThickenFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, targetShape);

    // Replace the target body with the thickened shape
    m_brepModel->addBody(targetId, result);

    // Dependency graph
    m_depGraph.addNode(featureId);
    std::string depFeat = featureForBody(targetId);
    if (!depFeat.empty())
        m_depGraph.addEdge(depFeat, featureId);

    registerBodyFeature(targetId, featureId);

    appendFeatureToTimeline(feature);

    m_modified = true;
    return targetId;
}

std::string Document::addThread(features::ThreadParams params)
{
    std::string targetId = params.targetBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("Thread: target body '" + targetId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

    // Capture stable face signature for the cylindrical face (if not auto-detect)
    params.faceSignatures.clear();
    if (params.cylindricalFaceIndex >= 0) {
        params.faceSignatures.push_back(
            kernel::StableReference::computeFaceSignature(targetShape,
                                                          params.cylindricalFaceIndex));
    }

    std::ostringstream featureIdStream;
    featureIdStream << "thread_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();
    m_nextBodyCounter++;

    auto feature = std::make_shared<features::ThreadFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, targetShape);

    // Replace the target body with the threaded shape
    m_brepModel->addBody(targetId, result);

    // Dependency graph
    m_depGraph.addNode(featureId);
    std::string depFeat = featureForBody(targetId);
    if (!depFeat.empty())
        m_depGraph.addEdge(depFeat, featureId);

    registerBodyFeature(targetId, featureId);

    appendFeatureToTimeline(feature);

    m_modified = true;
    return targetId;
}

std::string Document::addScale(features::ScaleParams params)
{
    std::string targetId = params.targetBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("Scale: target body '" + targetId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

    std::ostringstream featureIdStream;
    featureIdStream << "scale_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();
    m_nextBodyCounter++;

    auto feature = std::make_shared<features::ScaleFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, targetShape);

    // Replace the target body with the scaled shape
    m_brepModel->addBody(targetId, result);

    // Dependency graph
    m_depGraph.addNode(featureId);
    std::string depFeat = featureForBody(targetId);
    if (!depFeat.empty())
        m_depGraph.addEdge(depFeat, featureId);

    registerBodyFeature(targetId, featureId);

    appendFeatureToTimeline(feature);

    m_modified = true;
    return targetId;
}

std::string Document::addPathPattern(features::PathPatternParams params)
{
    std::string targetId = params.targetBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("PathPattern: target body '" + targetId + "' not found");

    std::string pathId = params.pathBodyId;
    if (!m_brepModel->hasBody(pathId))
        throw std::runtime_error("PathPattern: path body '" + pathId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);
    TopoDS_Shape pathShape = m_brepModel->getShape(pathId);

    std::ostringstream featureIdStream;
    featureIdStream << "pathpattern_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();
    m_nextBodyCounter++;

    auto feature = std::make_shared<features::PathPatternFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, targetShape, pathShape);

    m_brepModel->addBody(targetId, result);

    m_depGraph.addNode(featureId);
    std::string depFeat = featureForBody(targetId);
    if (!depFeat.empty())
        m_depGraph.addEdge(depFeat, featureId);
    std::string pathDepFeat = featureForBody(pathId);
    if (!pathDepFeat.empty())
        m_depGraph.addEdge(pathDepFeat, featureId);

    registerBodyFeature(targetId, featureId);
    appendFeatureToTimeline(feature);
    m_modified = true;
    return targetId;
}

std::string Document::addCoil(features::CoilParams params)
{
    std::string profileId = params.profileBodyId;
    TopoDS_Shape profileShape;

    if (!profileId.empty()) {
        if (!m_brepModel->hasBody(profileId))
            throw std::runtime_error("Coil: profile body '" + profileId + "' not found");
        profileShape = m_brepModel->getShape(profileId);
    }

    std::ostringstream featureIdStream;
    featureIdStream << "coil_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();

    std::ostringstream bodyIdStream;
    bodyIdStream << "body_" << m_nextBodyCounter;
    std::string bodyId = bodyIdStream.str();
    m_nextBodyCounter++;

    auto feature = std::make_shared<features::CoilFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, profileShape);

    m_brepModel->addBody(bodyId, result);

    m_depGraph.addNode(featureId);
    if (!profileId.empty()) {
        std::string depFeat = featureForBody(profileId);
        if (!depFeat.empty())
            m_depGraph.addEdge(depFeat, featureId);
    }

    registerBodyFeature(bodyId, featureId);
    appendFeatureToTimeline(feature);
    m_modified = true;
    return bodyId;
}

std::string Document::addDeleteFace(features::DeleteFaceParams params)
{
    std::string targetId = params.targetBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("DeleteFace: target body '" + targetId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

    params.faceSignatures.clear();
    for (int idx : params.faceIndices) {
        params.faceSignatures.push_back(
            kernel::StableReference::computeFaceSignature(targetShape, idx));
    }

    std::ostringstream featureIdStream;
    featureIdStream << "deleteface_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();
    m_nextBodyCounter++;

    auto feature = std::make_shared<features::DeleteFaceFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, targetShape);

    m_brepModel->addBody(targetId, result);

    m_depGraph.addNode(featureId);
    std::string depFeat = featureForBody(targetId);
    if (!depFeat.empty())
        m_depGraph.addEdge(depFeat, featureId);

    registerBodyFeature(targetId, featureId);
    appendFeatureToTimeline(feature);
    m_modified = true;
    return targetId;
}

std::string Document::addReplaceFace(features::ReplaceFaceParams params)
{
    std::string targetId = params.targetBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("ReplaceFace: target body '" + targetId + "' not found");

    std::string replId = params.replacementBodyId;
    if (!m_brepModel->hasBody(replId))
        throw std::runtime_error("ReplaceFace: replacement body '" + replId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);
    TopoDS_Shape replShape = m_brepModel->getShape(replId);

    params.faceSignature =
        kernel::StableReference::computeFaceSignature(targetShape, params.faceIndex);

    std::ostringstream featureIdStream;
    featureIdStream << "replaceface_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();
    m_nextBodyCounter++;

    auto feature = std::make_shared<features::ReplaceFaceFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, targetShape, replShape);

    m_brepModel->addBody(targetId, result);

    m_depGraph.addNode(featureId);
    std::string depFeat = featureForBody(targetId);
    if (!depFeat.empty())
        m_depGraph.addEdge(depFeat, featureId);
    std::string replDepFeat = featureForBody(replId);
    if (!replDepFeat.empty())
        m_depGraph.addEdge(replDepFeat, featureId);

    registerBodyFeature(targetId, featureId);
    appendFeatureToTimeline(feature);
    m_modified = true;
    return targetId;
}

std::string Document::addReverseNormal(features::ReverseNormalParams params)
{
    std::string targetId = params.targetBodyId;
    if (!m_brepModel->hasBody(targetId))
        throw std::runtime_error("ReverseNormal: target body '" + targetId + "' not found");

    TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

    params.faceSignatures.clear();
    for (int idx : params.faceIndices) {
        params.faceSignatures.push_back(
            kernel::StableReference::computeFaceSignature(targetShape, idx));
    }

    std::ostringstream featureIdStream;
    featureIdStream << "reversenormal_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();
    m_nextBodyCounter++;

    auto feature = std::make_shared<features::ReverseNormalFeature>(featureId, std::move(params));
    TopoDS_Shape result = feature->execute(*m_kernel, targetShape);

    m_brepModel->addBody(targetId, result);

    m_depGraph.addNode(featureId);
    std::string depFeat = featureForBody(targetId);
    if (!depFeat.empty())
        m_depGraph.addEdge(depFeat, featureId);

    registerBodyFeature(targetId, featureId);
    appendFeatureToTimeline(feature);
    m_modified = true;
    return targetId;
}

std::string Document::addJoint(features::JointParams params)
{
    std::ostringstream featureIdStream;
    featureIdStream << "joint_" << m_nextBodyCounter;
    std::string featureId = featureIdStream.str();
    m_nextBodyCounter++;

    auto feature = std::make_shared<features::Joint>(featureId, std::move(params));

    // Build dependency graph: joint depends on both occurrences' components
    m_depGraph.addNode(featureId);

    // Append to timeline
    appendFeatureToTimeline(feature);

    // Solve joints immediately (update occurrence transforms)
    std::vector<std::shared_ptr<features::Feature>> joints;
    for (size_t i = 0; i < m_timeline->count(); ++i) {
        const auto& entry = m_timeline->entry(i);
        if (entry.feature && entry.feature->type() == features::FeatureType::Joint
            && !entry.isSuppressed && !entry.isRolledBack) {
            joints.push_back(entry.feature);
        }
    }
    JointSolver::solve(m_components, joints);

    m_modified = true;
    return featureId;
}

std::string Document::insertComponentFromFile(const std::string& kcdPath)
{
    // 1. Create a temporary Document and load the .kcd file into it
    Document tempDoc;
    if (!tempDoc.load(kcdPath))
        throw std::runtime_error("Failed to load component file: " + kcdPath);

    // 2. Create a new Component in this document's registry
    // Extract a short name from the file path
    std::string compName = kcdPath;
    auto slashPos = compName.rfind('/');
    if (slashPos == std::string::npos)
        slashPos = compName.rfind('\\');
    if (slashPos != std::string::npos)
        compName = compName.substr(slashPos + 1);
    // Strip .kcd extension
    auto dotPos = compName.rfind('.');
    if (dotPos != std::string::npos)
        compName = compName.substr(0, dotPos);

    std::string compId = m_components.createComponent(compName);
    auto* comp = m_components.findComponent(compId);

    // 3. For each body in the loaded document, add body to BRepModel with prefixed IDs
    auto& tempBrep = tempDoc.brepModel();
    auto tempIds = tempBrep.bodyIds();
    int bodyIdx = 1;
    for (const auto& tempBodyId : tempIds) {
        std::string newBodyId = compId + "_body_" + std::to_string(bodyIdx++);
        const TopoDS_Shape& shape = tempBrep.getShape(tempBodyId);
        m_brepModel->addBody(newBodyId, shape);
        m_nextBodyCounter++;

        // 4. Add body ref to the new component
        comp->addBodyRef(newBodyId);
    }

    // 5. Create an Occurrence in the root component pointing to the new component
    auto& root = m_components.rootComponent();
    std::string occId = root.addOccurrence(compId, compName);

    m_modified = true;
    return occId;
}

std::string Document::findOccurrenceForBody(const std::string& bodyId) const
{
    // Search all components for one whose body refs contain bodyId,
    // then find the occurrence in the root that points to that component.
    for (const auto& compId : m_components.componentIds()) {
        const auto* comp = m_components.findComponent(compId);
        if (!comp) continue;
        const auto& refs = comp->bodyRefs();
        for (const auto& ref : refs) {
            if (ref == bodyId) {
                // Found the component; now find the occurrence in root
                const auto& root = m_components.rootComponent();
                for (const auto& occ : root.occurrences()) {
                    if (occ.componentId == compId)
                        return occ.id;
                }
                return {};
            }
        }
    }
    return {};
}

features::SketchFeature* Document::findSketch(const std::string& sketchId)
{
    for (size_t i = 0; i < m_timeline->count(); ++i) {
        auto& entry = m_timeline->entry(i);
        if (entry.feature && entry.feature->id() == sketchId &&
            entry.feature->type() == features::FeatureType::Sketch) {
            return static_cast<features::SketchFeature*>(entry.feature.get());
        }
    }
    return nullptr;
}

// Helper: execute a single feature, updating BRepModel and bodyCounter as needed.
void Document::executeFeature(features::Feature* feat, int& bodyCounter)
{
    std::ostringstream bodyIdStream;

    if (feat->type() == features::FeatureType::Joint) {
        // Joint features produce no body -- they are handled by JointSolver
        // after all features are executed.
        return;
    }
    else if (feat->type() == features::FeatureType::Sketch) {
        auto* sketchFeat = static_cast<features::SketchFeature*>(feat);
        sketchFeat->sketch().solve();
        // Sketch features produce no body -- they just need solving
    }
    else if (feat->type() == features::FeatureType::Extrude) {
        auto* extrude = static_cast<features::ExtrudeFeature*>(feat);

        const sketch::Sketch* sketchPtr = nullptr;
        if (!extrude->params().sketchId.empty()) {
            auto* sketchFeat = findSketch(extrude->params().sketchId);
            if (sketchFeat)
                sketchPtr = &sketchFeat->sketch();
        }

        TopoDS_Shape shape = extrude->execute(*m_kernel, sketchPtr);
        bodyIdStream << "body_" << bodyCounter;
        std::string bodyId = bodyIdStream.str();
        m_brepModel->addBody(bodyId, shape);
        autoTagFeatureFaces(feat, bodyId, shape);
        registerBodyFeature(bodyId, feat->id());
        bodyCounter++;
    }
    else if (feat->type() == features::FeatureType::Revolve) {
        auto* revolve = static_cast<features::RevolveFeature*>(feat);

        const sketch::Sketch* sketchPtr = nullptr;
        if (!revolve->params().sketchId.empty()) {
            auto* sketchFeat = findSketch(revolve->params().sketchId);
            if (sketchFeat)
                sketchPtr = &sketchFeat->sketch();
        }

        TopoDS_Shape shape = revolve->execute(*m_kernel, sketchPtr);
        bodyIdStream << "body_" << bodyCounter;
        std::string bodyId = bodyIdStream.str();
        m_brepModel->addBody(bodyId, shape);
        autoTagFeatureFaces(feat, bodyId, shape);
        registerBodyFeature(bodyId, feat->id());
        bodyCounter++;
    }
    else if (feat->type() == features::FeatureType::Fillet) {
        auto* fillet = static_cast<features::FilletFeature*>(feat);
        std::string targetId = fillet->params().targetBodyId;
        if (m_brepModel->hasBody(targetId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

            // Stable reference: remap edge indices using stored signatures
            auto& fp = fillet->params();
            if (!fp.edgeSignatures.empty() &&
                fp.edgeSignatures.size() == fp.edgeIds.size()) {
                auto remapped = kernel::StableReference::remapEdgesFromSignatures(
                    fp.edgeSignatures, targetShape);
                // Only apply remapping if all edges were matched
                bool allMatched = true;
                for (int idx : remapped) {
                    if (idx < 0) { allMatched = false; break; }
                }
                if (allMatched)
                    fp.edgeIds = remapped;
            }

            TopoDS_Shape result = fillet->execute(*m_kernel, targetShape);
            propagateAttributes(targetId, targetShape, result);
            m_brepModel->addBody(targetId, result);
            autoTagFeatureFaces(feat, targetId, result);
            registerBodyFeature(targetId, feat->id());
        }
    }
    else if (feat->type() == features::FeatureType::Chamfer) {
        auto* chamfer = static_cast<features::ChamferFeature*>(feat);
        std::string targetId = chamfer->params().targetBodyId;
        if (m_brepModel->hasBody(targetId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

            // Stable reference: remap edge indices using stored signatures
            auto& cp = chamfer->params();
            if (!cp.edgeSignatures.empty() &&
                cp.edgeSignatures.size() == cp.edgeIds.size()) {
                auto remapped = kernel::StableReference::remapEdgesFromSignatures(
                    cp.edgeSignatures, targetShape);
                bool allMatched = true;
                for (int idx : remapped) {
                    if (idx < 0) { allMatched = false; break; }
                }
                if (allMatched)
                    cp.edgeIds = remapped;
            }

            TopoDS_Shape result = chamfer->execute(*m_kernel, targetShape);
            propagateAttributes(targetId, targetShape, result);
            m_brepModel->addBody(targetId, result);
            autoTagFeatureFaces(feat, targetId, result);
            registerBodyFeature(targetId, feat->id());
        }
    }
    else if (feat->type() == features::FeatureType::Sweep) {
        auto* sweep = static_cast<features::SweepFeature*>(feat);

        const sketch::Sketch* profileSketchPtr = nullptr;
        const sketch::Sketch* pathSketchPtr = nullptr;
        if (!sweep->params().sketchId.empty()) {
            auto* sketchFeat = findSketch(sweep->params().sketchId);
            if (sketchFeat)
                profileSketchPtr = &sketchFeat->sketch();
        }
        if (!sweep->params().pathSketchId.empty()) {
            auto* sketchFeat = findSketch(sweep->params().pathSketchId);
            if (sketchFeat)
                pathSketchPtr = &sketchFeat->sketch();
        }

        TopoDS_Shape shape = sweep->execute(*m_kernel, profileSketchPtr, pathSketchPtr);
        bodyIdStream << "body_" << bodyCounter;
        std::string bodyId = bodyIdStream.str();
        m_brepModel->addBody(bodyId, shape);
        autoTagFeatureFaces(feat, bodyId, shape);
        registerBodyFeature(bodyId, feat->id());
        bodyCounter++;
    }
    else if (feat->type() == features::FeatureType::Loft) {
        auto* loft = static_cast<features::LoftFeature*>(feat);

        std::vector<const sketch::Sketch*> sketches;
        for (const auto& sketchId : loft->params().sectionSketchIds) {
            auto* sketchFeat = findSketch(sketchId);
            sketches.push_back(sketchFeat ? &sketchFeat->sketch() : nullptr);
        }

        TopoDS_Shape shape = loft->execute(*m_kernel, sketches);
        bodyIdStream << "body_" << bodyCounter;
        std::string bodyId = bodyIdStream.str();
        m_brepModel->addBody(bodyId, shape);
        autoTagFeatureFaces(feat, bodyId, shape);
        registerBodyFeature(bodyId, feat->id());
        bodyCounter++;
    }
    else if (feat->type() == features::FeatureType::Shell) {
        auto* shellFeat = static_cast<features::ShellFeature*>(feat);
        std::string targetId = shellFeat->params().targetBodyId;
        if (m_brepModel->hasBody(targetId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

            // Stable reference: remap removed-face indices
            auto& sp = shellFeat->params();
            if (!sp.faceSignatures.empty() &&
                sp.faceSignatures.size() == sp.removedFaceIds.size()) {
                auto remapped = kernel::StableReference::remapFacesFromSignatures(
                    sp.faceSignatures, targetShape);
                bool allMatched = true;
                for (int idx : remapped) {
                    if (idx < 0) { allMatched = false; break; }
                }
                if (allMatched)
                    sp.removedFaceIds = remapped;
            }

            TopoDS_Shape result = shellFeat->execute(*m_kernel, targetShape);
            propagateAttributes(targetId, targetShape, result);
            m_brepModel->addBody(targetId, result);
            registerBodyFeature(targetId, feat->id());
        }
    }
    else if (feat->type() == features::FeatureType::Mirror) {
        auto* mirror = static_cast<features::MirrorFeature*>(feat);
        std::string targetId = mirror->params().targetBodyId;
        if (m_brepModel->hasBody(targetId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);
            TopoDS_Shape result = mirror->execute(*m_kernel, targetShape);
            propagateAttributes(targetId, targetShape, result);
            m_brepModel->addBody(targetId, result);
            registerBodyFeature(targetId, feat->id());
        }
    }
    else if (feat->type() == features::FeatureType::RectangularPattern) {
        auto* pattern = static_cast<features::RectangularPatternFeature*>(feat);
        std::string targetId = pattern->params().targetBodyId;
        if (m_brepModel->hasBody(targetId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);
            TopoDS_Shape result = pattern->execute(*m_kernel, targetShape);
            propagateAttributes(targetId, targetShape, result);
            m_brepModel->addBody(targetId, result);
            registerBodyFeature(targetId, feat->id());
        }
    }
    else if (feat->type() == features::FeatureType::CircularPattern) {
        auto* pattern = static_cast<features::CircularPatternFeature*>(feat);
        std::string targetId = pattern->params().targetBodyId;
        if (m_brepModel->hasBody(targetId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);
            TopoDS_Shape result = pattern->execute(*m_kernel, targetShape);
            propagateAttributes(targetId, targetShape, result);
            m_brepModel->addBody(targetId, result);
            registerBodyFeature(targetId, feat->id());
        }
    }
    else if (feat->type() == features::FeatureType::Hole) {
        auto* holeFeat = static_cast<features::HoleFeature*>(feat);
        std::string targetId = holeFeat->params().targetBodyId;
        if (m_brepModel->hasBody(targetId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);
            TopoDS_Shape result = holeFeat->execute(*m_kernel, targetShape);
            propagateAttributes(targetId, targetShape, result);
            m_brepModel->addBody(targetId, result);
            autoTagFeatureFaces(feat, targetId, result);
            registerBodyFeature(targetId, feat->id());
        }
    }
    else if (feat->type() == features::FeatureType::Combine) {
        auto* combineFeat = static_cast<features::CombineFeature*>(feat);
        std::string targetId = combineFeat->params().targetBodyId;
        std::string toolId   = combineFeat->params().toolBodyId;
        if (m_brepModel->hasBody(targetId) && m_brepModel->hasBody(toolId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);
            TopoDS_Shape toolShape   = m_brepModel->getShape(toolId);
            TopoDS_Shape result = combineFeat->execute(*m_kernel, targetShape, toolShape);
            propagateAttributes(targetId, targetShape, result);
            m_brepModel->addBody(targetId, result);
            if (!combineFeat->params().keepToolBody)
                m_brepModel->removeBody(toolId);
            registerBodyFeature(targetId, feat->id());
        }
    }
    else if (feat->type() == features::FeatureType::SplitBody) {
        auto* splitFeat = static_cast<features::SplitBodyFeature*>(feat);
        std::string targetId = splitFeat->params().targetBodyId;
        if (m_brepModel->hasBody(targetId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);
            TopoDS_Shape result;
            if (splitFeat->params().usePlane) {
                result = splitFeat->execute(*m_kernel, targetShape);
            } else {
                std::string toolId = splitFeat->params().splittingToolId;
                if (m_brepModel->hasBody(toolId)) {
                    TopoDS_Shape toolShape = m_brepModel->getShape(toolId);
                    result = splitFeat->execute(*m_kernel, targetShape, toolShape);
                } else {
                    result = targetShape;
                }
            }
            propagateAttributes(targetId, targetShape, result);
            m_brepModel->addBody(targetId, result);
            registerBodyFeature(targetId, feat->id());
        }
    }
    else if (feat->type() == features::FeatureType::OffsetFaces) {
        auto* offsetFeat = static_cast<features::OffsetFacesFeature*>(feat);
        std::string targetId = offsetFeat->params().targetBodyId;
        if (m_brepModel->hasBody(targetId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

            // Stable reference: remap face indices
            auto& ofp = offsetFeat->params();
            if (!ofp.faceSignatures.empty() &&
                ofp.faceSignatures.size() == ofp.faceIndices.size()) {
                auto remapped = kernel::StableReference::remapFacesFromSignatures(
                    ofp.faceSignatures, targetShape);
                bool allMatched = true;
                for (int idx : remapped) {
                    if (idx < 0) { allMatched = false; break; }
                }
                if (allMatched)
                    ofp.faceIndices = remapped;
            }

            TopoDS_Shape result = offsetFeat->execute(*m_kernel, targetShape);
            propagateAttributes(targetId, targetShape, result);
            m_brepModel->addBody(targetId, result);
            registerBodyFeature(targetId, feat->id());
        }
    }
    else if (feat->type() == features::FeatureType::Move) {
        auto* moveFeat = static_cast<features::MoveFeature*>(feat);
        std::string targetId = moveFeat->params().targetBodyId;
        if (m_brepModel->hasBody(targetId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);
            TopoDS_Shape result = moveFeat->execute(*m_kernel, targetShape);
            if (moveFeat->params().createCopy) {
                // Store the moved copy as a new body
                bodyIdStream << "body_" << bodyCounter;
                std::string newBodyId = bodyIdStream.str();
                m_brepModel->addBody(newBodyId, result);
                registerBodyFeature(newBodyId, feat->id());
                bodyCounter++;
            } else {
                propagateAttributes(targetId, targetShape, result);
                m_brepModel->addBody(targetId, result);
                registerBodyFeature(targetId, feat->id());
            }
        }
    }
    else if (feat->type() == features::FeatureType::Draft) {
        auto* draftFeat = static_cast<features::DraftFeature*>(feat);
        std::string targetId = draftFeat->params().targetBodyId;
        if (m_brepModel->hasBody(targetId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

            // Stable reference: remap face indices
            auto& dp = draftFeat->params();
            if (!dp.faceSignatures.empty() &&
                dp.faceSignatures.size() == dp.faceIndices.size()) {
                auto remapped = kernel::StableReference::remapFacesFromSignatures(
                    dp.faceSignatures, targetShape);
                bool allMatched = true;
                for (int idx : remapped) {
                    if (idx < 0) { allMatched = false; break; }
                }
                if (allMatched)
                    dp.faceIndices = remapped;
            }

            TopoDS_Shape result = draftFeat->execute(*m_kernel, targetShape);
            propagateAttributes(targetId, targetShape, result);
            m_brepModel->addBody(targetId, result);
            autoTagFeatureFaces(feat, targetId, result);
            registerBodyFeature(targetId, feat->id());
        }
    }
    else if (feat->type() == features::FeatureType::Thicken) {
        auto* thickenFeat = static_cast<features::ThickenFeature*>(feat);
        std::string targetId = thickenFeat->params().targetBodyId;
        if (m_brepModel->hasBody(targetId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);
            TopoDS_Shape result = thickenFeat->execute(*m_kernel, targetShape);
            propagateAttributes(targetId, targetShape, result);
            m_brepModel->addBody(targetId, result);
            autoTagFeatureFaces(feat, targetId, result);
            registerBodyFeature(targetId, feat->id());
        }
    }
    else if (feat->type() == features::FeatureType::Thread) {
        auto* threadFeat = static_cast<features::ThreadFeature*>(feat);
        std::string targetId = threadFeat->params().targetBodyId;
        if (m_brepModel->hasBody(targetId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

            // Stable reference: remap cylindrical face index
            auto& tp = threadFeat->params();
            if (!tp.faceSignatures.empty() && tp.cylindricalFaceIndex >= 0) {
                auto remapped = kernel::StableReference::remapFacesFromSignatures(
                    tp.faceSignatures, targetShape);
                if (!remapped.empty() && remapped[0] >= 0)
                    tp.cylindricalFaceIndex = remapped[0];
            }

            TopoDS_Shape result = threadFeat->execute(*m_kernel, targetShape);
            propagateAttributes(targetId, targetShape, result);
            m_brepModel->addBody(targetId, result);
            autoTagFeatureFaces(feat, targetId, result);
            registerBodyFeature(targetId, feat->id());
        }
    }
    else if (feat->type() == features::FeatureType::Scale) {
        auto* scaleFeat = static_cast<features::ScaleFeature*>(feat);
        std::string targetId = scaleFeat->params().targetBodyId;
        if (m_brepModel->hasBody(targetId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);
            TopoDS_Shape result = scaleFeat->execute(*m_kernel, targetShape);
            propagateAttributes(targetId, targetShape, result);
            m_brepModel->addBody(targetId, result);
            registerBodyFeature(targetId, feat->id());
        }
    }
    else if (feat->type() == features::FeatureType::PathPattern) {
        auto* patternFeat = static_cast<features::PathPatternFeature*>(feat);
        std::string targetId = patternFeat->params().targetBodyId;
        std::string pathId = patternFeat->params().pathBodyId;
        if (m_brepModel->hasBody(targetId) && m_brepModel->hasBody(pathId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);
            TopoDS_Shape pathShape = m_brepModel->getShape(pathId);
            TopoDS_Shape result = patternFeat->execute(*m_kernel, targetShape, pathShape);
            propagateAttributes(targetId, targetShape, result);
            m_brepModel->addBody(targetId, result);
            registerBodyFeature(targetId, feat->id());
        }
    }
    else if (feat->type() == features::FeatureType::Coil) {
        auto* coilFeat = static_cast<features::CoilFeature*>(feat);
        std::string profileId = coilFeat->params().profileBodyId;
        TopoDS_Shape profileShape;
        if (!profileId.empty() && m_brepModel->hasBody(profileId))
            profileShape = m_brepModel->getShape(profileId);

        TopoDS_Shape shape = coilFeat->execute(*m_kernel, profileShape);
        bodyIdStream << "body_" << bodyCounter;
        std::string bodyId = bodyIdStream.str();
        m_brepModel->addBody(bodyId, shape);
        autoTagFeatureFaces(feat, bodyId, shape);
        registerBodyFeature(bodyId, feat->id());
        bodyCounter++;
    }
    else if (feat->type() == features::FeatureType::DeleteFace) {
        auto* deleteFeat = static_cast<features::DeleteFaceFeature*>(feat);
        std::string targetId = deleteFeat->params().targetBodyId;
        if (m_brepModel->hasBody(targetId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

            // Stable reference: remap face indices using stored signatures
            auto& dp = deleteFeat->params();
            if (!dp.faceSignatures.empty() &&
                dp.faceSignatures.size() == dp.faceIndices.size()) {
                auto remapped = kernel::StableReference::remapFacesFromSignatures(
                    dp.faceSignatures, targetShape);
                bool allMatched = true;
                for (int idx : remapped) {
                    if (idx < 0) { allMatched = false; break; }
                }
                if (allMatched)
                    dp.faceIndices = remapped;
            }

            TopoDS_Shape result = deleteFeat->execute(*m_kernel, targetShape);
            propagateAttributes(targetId, targetShape, result);
            m_brepModel->addBody(targetId, result);
            registerBodyFeature(targetId, feat->id());
        }
    }
    else if (feat->type() == features::FeatureType::ReplaceFace) {
        auto* replaceFeat = static_cast<features::ReplaceFaceFeature*>(feat);
        std::string targetId = replaceFeat->params().targetBodyId;
        std::string replId = replaceFeat->params().replacementBodyId;
        if (m_brepModel->hasBody(targetId) && m_brepModel->hasBody(replId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);
            TopoDS_Shape replShape = m_brepModel->getShape(replId);

            // Stable reference: remap face index using stored signature
            auto& rp = replaceFeat->params();
            if (rp.faceSignature.area > 0) {
                std::vector<kernel::FaceSignature> sigs = { rp.faceSignature };
                auto remapped = kernel::StableReference::remapFacesFromSignatures(
                    sigs, targetShape);
                if (!remapped.empty() && remapped[0] >= 0)
                    rp.faceIndex = remapped[0];
            }

            TopoDS_Shape result = replaceFeat->execute(*m_kernel, targetShape, replShape);
            propagateAttributes(targetId, targetShape, result);
            m_brepModel->addBody(targetId, result);
            registerBodyFeature(targetId, feat->id());
        }
    }
    else if (feat->type() == features::FeatureType::ReverseNormal) {
        auto* reverseFeat = static_cast<features::ReverseNormalFeature*>(feat);
        std::string targetId = reverseFeat->params().targetBodyId;
        if (m_brepModel->hasBody(targetId)) {
            TopoDS_Shape targetShape = m_brepModel->getShape(targetId);

            // Stable reference: remap face indices using stored signatures
            auto& rp = reverseFeat->params();
            if (!rp.faceSignatures.empty() &&
                rp.faceSignatures.size() == rp.faceIndices.size()) {
                auto remapped = kernel::StableReference::remapFacesFromSignatures(
                    rp.faceSignatures, targetShape);
                bool allMatched = true;
                for (int idx : remapped) {
                    if (idx < 0) { allMatched = false; break; }
                }
                if (allMatched)
                    rp.faceIndices = remapped;
            }

            TopoDS_Shape result = reverseFeat->execute(*m_kernel, targetShape);
            propagateAttributes(targetId, targetShape, result);
            m_brepModel->addBody(targetId, result);
            registerBodyFeature(targetId, feat->id());
        }
    }
}

void Document::recompute()
{
    // Full recompute: clear all bodies and replay the timeline
    m_brepModel->clear();
    m_bodyToFeature.clear();
    m_lastGoodShapes.clear();
    m_erroredFeatureIds.clear();
    int bodyCounter = 1;

    for (size_t i = 0; i < m_timeline->count(); ++i) {
        const auto& entry = m_timeline->entry(i);
        // Skip rolled-back entries and effectively-suppressed entries
        // (own flag OR group suppression).
        if (entry.isRolledBack || m_timeline->isEffectivelySuppressed(i))
            continue;

        auto* feat = entry.feature.get();

        // Snapshot all bodies that this feature might modify
        // (target body features).  We snapshot every known body for
        // simplicity -- only the modified one matters.
        for (const auto& [bid, _] : m_bodyToFeature)
            snapshotBody(bid);

        try {
            executeFeature(feat, bodyCounter);
            feat->setHealthState(features::HealthState::Healthy);
            feat->setErrorMessage("");

            // Update last-good snapshots after successful execution
            for (const auto& [bid, _] : m_bodyToFeature) {
                if (m_brepModel->hasBody(bid))
                    m_lastGoodShapes[bid] = m_brepModel->getShape(bid);
            }
        } catch (const std::exception& e) {
            feat->setHealthState(features::HealthState::Error);
            feat->setErrorMessage(e.what());
            m_erroredFeatureIds.insert(feat->id());

            // Propagate error to all downstream dependents
            auto dependents = m_depGraph.allDependentsOf(feat->id());
            for (const auto& depId : dependents) {
                for (size_t j = i + 1; j < m_timeline->count(); ++j) {
                    auto& depEntry = m_timeline->entry(j);
                    if (depEntry.id == depId && depEntry.feature) {
                        depEntry.feature->setHealthState(features::HealthState::Error);
                        depEntry.feature->setErrorMessage(
                            "Upstream feature '" + feat->name() + "' failed");
                        m_erroredFeatureIds.insert(depId);
                    }
                }
            }

            // Restore bodies to last-good state so downstream features
            // can continue with valid geometry.
            for (const auto& [bid, shape] : m_lastGoodShapes) {
                if (m_brepModel->hasBody(bid))
                    m_brepModel->addBody(bid, shape);
            }
        }
    }

    // After all features are recomputed, solve joint constraints
    // to update occurrence transforms.
    std::vector<std::shared_ptr<features::Feature>> joints;
    for (size_t i = 0; i < m_timeline->count(); ++i) {
        const auto& entry = m_timeline->entry(i);
        if (entry.feature && entry.feature->type() == features::FeatureType::Joint
            && !m_timeline->isEffectivelySuppressed(i) && !entry.isRolledBack) {
            joints.push_back(entry.feature);
        }
    }
    if (!joints.empty())
        JointSolver::solve(m_components, joints);
}

void Document::recompute(const std::vector<std::string>& dirtyFeatureIds)
{
    if (dirtyFeatureIds.empty())
        return;

    // Use the dependency graph to find the full set that needs recompute
    auto dirtySet = m_depGraph.propagateDirty(dirtyFeatureIds);

    // Build a lookup set for O(1) membership test
    std::unordered_set<std::string> dirtyLookup(dirtySet.begin(), dirtySet.end());

    // Walk the timeline in order, only re-executing features in the dirty set.
    // We need a body counter consistent with the full timeline to generate
    // correct body IDs. Non-dirty features keep their existing shapes.
    int bodyCounter = 1;

    for (size_t i = 0; i < m_timeline->count(); ++i) {
        const auto& entry = m_timeline->entry(i);
        if (entry.isRolledBack || m_timeline->isEffectivelySuppressed(i))
            continue;

        auto* feat = entry.feature.get();

        // Count bodies produced by body-producing features to keep IDs consistent
        bool producesBody = (feat->type() == features::FeatureType::Extrude ||
                             feat->type() == features::FeatureType::Revolve ||
                             feat->type() == features::FeatureType::Sweep ||
                             feat->type() == features::FeatureType::Loft ||
                             feat->type() == features::FeatureType::Coil);

        if (dirtyLookup.count(feat->id())) {
            // Input hashing: check if the feature's inputs have changed
            size_t currentHash = computeFeatureInputHash(feat);
            auto hashIt = m_featureInputHashes.find(feat->id());
            if (hashIt != m_featureInputHashes.end() && hashIt->second == currentHash) {
                // Inputs unchanged -- skip execution, reuse cached output
                if (producesBody)
                    bodyCounter++;
                continue;
            }

            // Snapshot bodies before execution
            for (const auto& [bid, _] : m_bodyToFeature)
                snapshotBody(bid);

            try {
                executeFeature(feat, bodyCounter);
                feat->setHealthState(features::HealthState::Healthy);
                feat->setErrorMessage("");
                m_erroredFeatureIds.erase(feat->id());
                m_featureInputHashes[feat->id()] = currentHash;

                // Update last-good snapshots
                for (const auto& [bid, _] : m_bodyToFeature) {
                    if (m_brepModel->hasBody(bid))
                        m_lastGoodShapes[bid] = m_brepModel->getShape(bid);
                }
            } catch (const std::exception& e) {
                feat->setHealthState(features::HealthState::Error);
                feat->setErrorMessage(e.what());
                m_erroredFeatureIds.insert(feat->id());

                // Propagate error to all downstream dependents
                auto dependents = m_depGraph.allDependentsOf(feat->id());
                for (const auto& depId : dependents) {
                    for (size_t j = i + 1; j < m_timeline->count(); ++j) {
                        auto& depEntry = m_timeline->entry(j);
                        if (depEntry.id == depId && depEntry.feature) {
                            depEntry.feature->setHealthState(features::HealthState::Error);
                            depEntry.feature->setErrorMessage(
                                "Upstream feature '" + feat->name() + "' failed");
                            m_erroredFeatureIds.insert(depId);
                        }
                    }
                }

                // Restore from last-good
                for (const auto& [bid, shape] : m_lastGoodShapes) {
                    if (m_brepModel->hasBody(bid))
                        m_brepModel->addBody(bid, shape);
                }
            }
        } else {
            // Feature is clean -- skip execution but advance the body counter
            if (producesBody)
                bodyCounter++;
        }
    }
}

void Document::recomputeFrom(const std::string& featureId)
{
    recompute({featureId});
}

void Document::executeCommand(std::unique_ptr<Command> cmd)
{
    m_history.execute(std::move(cmd), *this);
}

// ── Transaction support ───────────────────────────────────────────────────────

void Document::beginTransaction()
{
    m_transactionSnapshot.timelineCount = m_timeline->count();
    m_transactionSnapshot.bodyIds = m_brepModel->bodyIds();
    m_transactionSnapshot.active = true;
}

void Document::commitTransaction()
{
    m_transactionSnapshot.active = false;
}

void Document::rollbackTransaction()
{
    if (!m_transactionSnapshot.active) return;

    // Remove any features added during the failed transaction
    while (m_timeline->count() > m_transactionSnapshot.timelineCount) {
        auto& entry = m_timeline->entry(m_timeline->count() - 1);
        m_timeline->remove(entry.id);
    }

    // Remove any bodies added during the failed transaction
    auto currentIds = m_brepModel->bodyIds();
    for (const auto& id : currentIds) {
        bool wasExisting = false;
        for (const auto& origId : m_transactionSnapshot.bodyIds) {
            if (origId == id) { wasExisting = true; break; }
        }
        if (!wasExisting) m_brepModel->removeBody(id);
    }

    m_transactionSnapshot.active = false;
}

// ── Error recovery helpers ────────────────────────────────────────────────────

void Document::snapshotBody(const std::string& bodyId)
{
    if (m_brepModel->hasBody(bodyId))
        m_lastGoodShapes[bodyId] = m_brepModel->getShape(bodyId);
}

void Document::restoreBody(const std::string& bodyId)
{
    auto it = m_lastGoodShapes.find(bodyId);
    if (it != m_lastGoodShapes.end() && m_brepModel->hasBody(bodyId))
        m_brepModel->addBody(bodyId, it->second);
}

bool Document::bodyHasError(const std::string& bodyId) const
{
    // A body has error if the feature that last modified it is in error state,
    // OR if any feature in the error set references this body.
    auto bodyFeatIt = m_bodyToFeature.find(bodyId);
    if (bodyFeatIt != m_bodyToFeature.end()) {
        if (m_erroredFeatureIds.count(bodyFeatIt->second))
            return true;
    }
    return false;
}

// ── Recompute input hashing ──────────────────────────────────────────────────

size_t Document::computeFeatureInputHash(features::Feature* feat) const
{
    // Build a quick fingerprint: feature id + type + name (as a proxy for
    // params -- actual param serialization would be heavier).  Then fold in
    // a cheap body geometry summary for body-modifying features.
    std::hash<std::string> strHash;
    size_t h = strHash(feat->id());
    h ^= strHash(feat->name()) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<int>{}(static_cast<int>(feat->type())) + 0x9e3779b9 + (h << 6) + (h >> 2);

    // For features that operate on a target body, fold in a geometry fingerprint
    // (face count + edge count).  We inspect common feature types.
    auto hashBody = [&](const std::string& bodyId) {
        if (bodyId.empty() || !m_brepModel->hasBody(bodyId))
            return;
        const TopoDS_Shape& shape = m_brepModel->getShape(bodyId);
        int faceCount = 0, edgeCount = 0;
        for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next())
            ++faceCount;
        for (TopExp_Explorer ex(shape, TopAbs_EDGE); ex.More(); ex.Next())
            ++edgeCount;
        h ^= std::hash<int>{}(faceCount)  + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(edgeCount)  + 0x9e3779b9 + (h << 6) + (h >> 2);
    };

    // Extract target body ID from common feature types via the timeline entry
    // name heuristic.  For a precise approach we would serialize all params,
    // but this lightweight fingerprint covers the most important cases.
    if (feat->type() == features::FeatureType::Fillet) {
        auto* f = static_cast<features::FilletFeature*>(feat);
        hashBody(f->params().targetBodyId);
    } else if (feat->type() == features::FeatureType::Chamfer) {
        auto* f = static_cast<features::ChamferFeature*>(feat);
        hashBody(f->params().targetBodyId);
    } else if (feat->type() == features::FeatureType::Shell) {
        auto* f = static_cast<features::ShellFeature*>(feat);
        hashBody(f->params().targetBodyId);
    } else if (feat->type() == features::FeatureType::Mirror) {
        auto* f = static_cast<features::MirrorFeature*>(feat);
        hashBody(f->params().targetBodyId);
    } else if (feat->type() == features::FeatureType::Hole) {
        auto* f = static_cast<features::HoleFeature*>(feat);
        hashBody(f->params().targetBodyId);
    } else if (feat->type() == features::FeatureType::Scale) {
        auto* f = static_cast<features::ScaleFeature*>(feat);
        hashBody(f->params().targetBodyId);
    } else if (feat->type() == features::FeatureType::Draft) {
        auto* f = static_cast<features::DraftFeature*>(feat);
        hashBody(f->params().targetBodyId);
    } else if (feat->type() == features::FeatureType::Thicken) {
        auto* f = static_cast<features::ThickenFeature*>(feat);
        hashBody(f->params().targetBodyId);
    } else if (feat->type() == features::FeatureType::Thread) {
        auto* f = static_cast<features::ThreadFeature*>(feat);
        hashBody(f->params().targetBodyId);
    } else if (feat->type() == features::FeatureType::Move) {
        auto* f = static_cast<features::MoveFeature*>(feat);
        hashBody(f->params().targetBodyId);
    } else if (feat->type() == features::FeatureType::OffsetFaces) {
        auto* f = static_cast<features::OffsetFacesFeature*>(feat);
        hashBody(f->params().targetBodyId);
    } else if (feat->type() == features::FeatureType::Combine) {
        auto* f = static_cast<features::CombineFeature*>(feat);
        hashBody(f->params().targetBodyId);
        hashBody(f->params().toolBodyId);
    } else if (feat->type() == features::FeatureType::SplitBody) {
        auto* f = static_cast<features::SplitBodyFeature*>(feat);
        hashBody(f->params().targetBodyId);
    } else if (feat->type() == features::FeatureType::RectangularPattern) {
        auto* f = static_cast<features::RectangularPatternFeature*>(feat);
        hashBody(f->params().targetBodyId);
    } else if (feat->type() == features::FeatureType::CircularPattern) {
        auto* f = static_cast<features::CircularPatternFeature*>(feat);
        hashBody(f->params().targetBodyId);
    } else if (feat->type() == features::FeatureType::PathPattern) {
        auto* f = static_cast<features::PathPatternFeature*>(feat);
        hashBody(f->params().targetBodyId);
        hashBody(f->params().pathBodyId);
    } else if (feat->type() == features::FeatureType::DeleteFace) {
        auto* f = static_cast<features::DeleteFaceFeature*>(feat);
        hashBody(f->params().targetBodyId);
    } else if (feat->type() == features::FeatureType::ReplaceFace) {
        auto* f = static_cast<features::ReplaceFaceFeature*>(feat);
        hashBody(f->params().targetBodyId);
        hashBody(f->params().replacementBodyId);
    } else if (feat->type() == features::FeatureType::ReverseNormal) {
        auto* f = static_cast<features::ReverseNormalFeature*>(feat);
        hashBody(f->params().targetBodyId);
    }

    return h;
}

// ── Attribute propagation ─────────────────────────────────────────────────────

void Document::propagateAttributes(const std::string& bodyId,
                                    const TopoDS_Shape& oldShape,
                                    const TopoDS_Shape& newShape)
{
    const kernel::AttributeMap& oldAttrs = m_brepModel->attributes(bodyId);
    if (oldAttrs.empty())
        return;

    int oldFC = m_kernel->faceCount(oldShape);
    int newFC = m_kernel->faceCount(newShape);

    std::vector<float> oldCentroids = m_kernel->faceCentroids(oldShape);
    std::vector<float> newCentroids = m_kernel->faceCentroids(newShape);

    // We write into a temporary map, then replace the body's attribute map
    // after addBody is called (which doesn't touch attributes).
    kernel::AttributeMap newAttrs;
    kernel::AttributeMap::propagate(oldAttrs, oldFC, newAttrs, newFC,
                                     oldCentroids, newCentroids);

    // Store the propagated map; it will persist even after addBody
    // clears the mesh cache (addBody doesn't touch attributeMaps).
    // We write to a temporary and assign after addBody call in the caller,
    // but since addBody doesn't erase attributeMaps, we can assign here.
    m_brepModel->attributes(bodyId) = std::move(newAttrs);
}

// ── Auto-tagging ─────────────────────────────────────────────────────────────

void Document::autoTagFeatureFaces(features::Feature* feat,
                                    const std::string& bodyId,
                                    const TopoDS_Shape& shape)
{
    const std::string featureId = feat->id();
    kernel::AttributeMap& attrs = m_brepModel->attributes(bodyId);
    int nFaces = m_kernel->faceCount(shape);

    if (feat->type() == features::FeatureType::Extrude) {
        // Extrude convention (OCCT BRepPrimAPI_MakePrism order for a box or
        // extruded face):
        //   - For a box (6 faces): bottom=0, top=1, front=2, back=3, left=4, right=5
        //   - For a sketch extrude: first face = start (profile), last face = end,
        //     middle faces = side faces
        // We use a heuristic: tag face 0 as StartFace, face nFaces-1 as EndFace,
        // and all in-between as SideFace.
        if (nFaces >= 2) {
            attrs.setFaceAttribute(0, {featureId, "StartFace", "0"});
            attrs.setFaceAttribute(nFaces - 1, {featureId, "EndFace",
                                   std::to_string(nFaces - 1)});
            for (int i = 1; i < nFaces - 1; ++i) {
                attrs.setFaceAttribute(i, {featureId, "SideFace", std::to_string(i)});
            }
        } else if (nFaces == 1) {
            attrs.setFaceAttribute(0, {featureId, "Face", "0"});
        }
    }
    else if (feat->type() == features::FeatureType::Revolve) {
        // Revolve: faces are typically start cap, end cap (if not 360), and
        // the revolved surface(s).
        if (nFaces >= 2) {
            attrs.setFaceAttribute(0, {featureId, "StartFace", "0"});
            attrs.setFaceAttribute(nFaces - 1, {featureId, "EndFace",
                                   std::to_string(nFaces - 1)});
            for (int i = 1; i < nFaces - 1; ++i) {
                attrs.setFaceAttribute(i, {featureId, "RevolveSurface", std::to_string(i)});
            }
        }
    }
    else if (feat->type() == features::FeatureType::Fillet) {
        // After fillet, new faces are typically added at the end.
        // The old faces were already propagated; we tag any faces that
        // don't yet have attributes from a previous feature as fillet faces.
        for (int i = 0; i < nFaces; ++i) {
            if (attrs.faceAttributes(i).empty()) {
                attrs.setFaceAttribute(i, {featureId, "FilletFace", std::to_string(i)});
            }
        }
    }
    else if (feat->type() == features::FeatureType::Chamfer) {
        // Same strategy as fillet: untagged faces are chamfer faces.
        for (int i = 0; i < nFaces; ++i) {
            if (attrs.faceAttributes(i).empty()) {
                attrs.setFaceAttribute(i, {featureId, "ChamferFace", std::to_string(i)});
            }
        }
    }
    else if (feat->type() == features::FeatureType::Hole) {
        // After hole, new faces are the hole cylinder and possibly bottom face.
        // Untagged faces are from the hole feature.
        for (int i = 0; i < nFaces; ++i) {
            if (attrs.faceAttributes(i).empty()) {
                attrs.setFaceAttribute(i, {featureId, "HoleFace", std::to_string(i)});
            }
        }
    }
    else if (feat->type() == features::FeatureType::Sweep) {
        // Sweep: start cap, end cap, swept surface(s)
        if (nFaces >= 2) {
            attrs.setFaceAttribute(0, {featureId, "StartFace", "0"});
            attrs.setFaceAttribute(nFaces - 1, {featureId, "EndFace",
                                   std::to_string(nFaces - 1)});
            for (int i = 1; i < nFaces - 1; ++i) {
                attrs.setFaceAttribute(i, {featureId, "SweptSurface", std::to_string(i)});
            }
        }
    }
    else if (feat->type() == features::FeatureType::Loft) {
        // Loft: first section, last section, lofted surfaces
        if (nFaces >= 2) {
            attrs.setFaceAttribute(0, {featureId, "StartFace", "0"});
            attrs.setFaceAttribute(nFaces - 1, {featureId, "EndFace",
                                   std::to_string(nFaces - 1)});
            for (int i = 1; i < nFaces - 1; ++i) {
                attrs.setFaceAttribute(i, {featureId, "LoftSurface", std::to_string(i)});
            }
        }
    }
    else if (feat->type() == features::FeatureType::Draft) {
        // Draft: untagged faces are draft-modified faces.
        for (int i = 0; i < nFaces; ++i) {
            if (attrs.faceAttributes(i).empty()) {
                attrs.setFaceAttribute(i, {featureId, "DraftFace", std::to_string(i)});
            }
        }
    }
    else if (feat->type() == features::FeatureType::Thicken) {
        // Thicken: the new faces created by thickening (inner/outer surfaces).
        for (int i = 0; i < nFaces; ++i) {
            if (attrs.faceAttributes(i).empty()) {
                attrs.setFaceAttribute(i, {featureId, "ThickenFace", std::to_string(i)});
            }
        }
    }
}

} // namespace document
