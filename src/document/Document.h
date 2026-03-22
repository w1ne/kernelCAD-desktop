#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "Timeline.h"
#include "ParameterStore.h"
#include "Origin.h"
#include "CommandHistory.h"
#include "DependencyGraph.h"
#include "Component.h"
// Forward declarations — full headers in Document.cpp
namespace kernel { class OCCTKernel; class BRepModel; }
#include "../kernel/Appearance.h"
#include <TopoDS_Shape.hxx>
#include "../features/ExtrudeFeature.h"
#include "../features/RevolveFeature.h"
#include "../features/FilletFeature.h"
#include "../features/ChamferFeature.h"
#include "../features/SweepFeature.h"
#include "../features/LoftFeature.h"
#include "../features/ShellFeature.h"
#include "../features/SketchFeature.h"
#include "../features/MirrorFeature.h"
#include "../features/RectangularPatternFeature.h"
#include "../features/CircularPatternFeature.h"
#include "../features/HoleFeature.h"
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
#include "../features/ConstructionPlane.h"
#include "../features/Joint.h"
#include "JointSolver.h"

namespace document {

class Document
{
public:
    Document();
    ~Document();

    bool save(const std::string& path);
    bool load(const std::string& path);
    void newDocument();

    /// Import a STEP or IGES file. Each root shape becomes a body in the model.
    /// Returns the number of bodies imported.
    int importFile(const std::string& path);

    Timeline&       timeline()       { return *m_timeline; }
    const Timeline& timeline() const { return *m_timeline; }
    ParameterStore& parameters()     { return *m_paramStore; }
    const ParameterStore& parameters() const { return *m_paramStore; }

    kernel::OCCTKernel& kernel()     { return *m_kernel; }
    kernel::BRepModel&  brepModel()  { return *m_brepModel; }

    kernel::AppearanceManager&       appearances()       { return m_appearances; }
    const kernel::AppearanceManager& appearances() const { return m_appearances; }

    Origin&       origin()       { return m_origin; }
    const Origin& origin() const { return m_origin; }

    DependencyGraph&       depGraph()       { return m_depGraph; }
    const DependencyGraph& depGraph() const { return m_depGraph; }

    ComponentRegistry&       components()       { return m_components; }
    const ComponentRegistry& components() const { return m_components; }

    const std::string& name() const  { return m_name; }
    void setName(const std::string& n) { m_name = n; }
    bool isModified() const          { return m_modified; }
    void setModified(bool v)         { m_modified = v; }

    /// Create an extrude feature, execute it, store the body in BRepModel,
    /// and append to the timeline. Returns the body ID.
    std::string addExtrude(features::ExtrudeParams params);

    /// Create a revolve feature (or cylinder shortcut when profileId is empty).
    std::string addRevolve(features::RevolveParams params);

    /// Create a fillet feature on an existing body. Returns the body ID.
    std::string addFillet(features::FilletParams params);

    /// Create a chamfer feature on an existing body. Returns the body ID.
    std::string addChamfer(features::ChamferParams params);

    /// Create a sketch feature on the given plane, appends to timeline.
    /// Returns the sketch feature ID.
    std::string addSketch(features::SketchParams params);

    /// Sweep a profile along a path. Returns the body ID.
    std::string addSweep(features::SweepParams params);

    /// Loft through multiple sections. Returns the body ID.
    std::string addLoft(features::LoftParams params);

    /// Shell an existing body. Returns the body ID.
    std::string addShell(features::ShellParams params);

    /// Mirror a body about a plane. Returns the body ID.
    std::string addMirror(features::MirrorParams params);

    /// Create a rectangular pattern of a body. Returns the body ID.
    std::string addRectangularPattern(features::RectangularPatternParams params);

    /// Create a circular pattern of a body. Returns the body ID.
    std::string addCircularPattern(features::CircularPatternParams params);

    /// Create a hole feature on an existing body. Returns the body ID.
    std::string addHole(features::HoleParams params);

    /// Combine two bodies with a boolean operation. Returns the target body ID.
    /// If keepToolBody is false, the tool body is removed from BRepModel.
    std::string addCombine(features::CombineParams params);

    /// Split a body with a plane or another body. Returns the target body ID
    /// (the body is replaced with the compound of split pieces).
    std::string addSplitBody(features::SplitBodyParams params);

    /// Offset specified faces of a body. Returns the body ID.
    std::string addOffsetFaces(features::OffsetFacesParams params);

    /// Move/transform a body. Returns the body ID (or new body ID if createCopy).
    std::string addMove(features::MoveParams params);

    /// Draft (taper) faces of a body. Returns the body ID.
    std::string addDraft(features::DraftParams params);

    /// Thicken a surface/shell into a solid. Returns the body ID.
    std::string addThicken(features::ThickenParams params);

    /// Create a thread on a cylindrical face. Returns the body ID.
    std::string addThread(features::ThreadParams params);

    /// Scale a body uniformly or non-uniformly. Returns the body ID.
    std::string addScale(features::ScaleParams params);

    /// Pattern a body along a path. Returns the body ID.
    std::string addPathPattern(features::PathPatternParams params);

    /// Create a coil (helical sweep). Returns the body ID.
    std::string addCoil(features::CoilParams params);

    /// Delete faces from a body, healing the gaps. Returns the body ID.
    std::string addDeleteFace(features::DeleteFaceParams params);

    /// Replace a face on a body with a face from another body. Returns the body ID.
    std::string addReplaceFace(features::ReplaceFaceParams params);

    /// Reverse normals of faces on a body. Returns the body ID.
    std::string addReverseNormal(features::ReverseNormalParams params);

    /// Add a construction plane feature. Returns the feature ID.
    std::string addConstructionPlane(features::ConstructionPlaneParams params);

    /// Add a joint between two component occurrences. Returns the joint feature ID.
    std::string addJoint(features::JointParams params);

    /// Import a .kcd file as a new component. Creates a Component from
    /// the imported document's bodies and adds an Occurrence in the root.
    /// Returns the occurrence ID.
    std::string insertComponentFromFile(const std::string& kcdPath);

    /// Find the occurrence ID that owns a given body ID, by searching
    /// component body refs.  Returns empty string if not found.
    std::string findOccurrenceForBody(const std::string& bodyId) const;

    /// Find a sketch feature by ID in the timeline. Returns nullptr if not found.
    features::SketchFeature* findSketch(const std::string& sketchId);

    /// Full recompute: replay all non-suppressed timeline entries, rebuilding BRepModel.
    void recompute();

    /// Incremental recompute: only re-execute the given dirty features and their
    /// transitive dependents, in topological order.
    void recompute(const std::vector<std::string>& dirtyFeatureIds);

    /// Mark a single feature dirty and recompute only it and its downstream dependents.
    void recomputeFrom(const std::string& featureId);

    /// Access the command history (undo/redo stacks).
    CommandHistory& history() { return m_history; }
    const CommandHistory& history() const { return m_history; }

    /// Execute a command through the history (pushes onto the undo stack).
    void executeCommand(std::unique_ptr<Command> cmd);

    // Transaction support for atomic command execution
    void beginTransaction();
    void commitTransaction();
    void rollbackTransaction();

    /// Lookup which feature last created/modified a body.
    std::string featureForBody(const std::string& bodyId) const;

    /// Rebuild the dependency edges for a feature from its current params.
    /// Removes all existing incoming edges and re-adds them based on the
    /// feature's sketchId, targetBodyId, or other reference fields.
    void updateDependenciesFromParams(const std::string& featureId);

    /// Append a feature to the timeline with an auto-generated numbered name.
    void appendFeatureToTimeline(std::shared_ptr<features::Feature> feature);

private:
    std::string                    m_name = "Untitled";
    bool                           m_modified = false;
    Origin                         m_origin;
    std::unique_ptr<Timeline>      m_timeline;
    std::unique_ptr<ParameterStore> m_paramStore;
    std::unique_ptr<kernel::OCCTKernel> m_kernel;
    std::unique_ptr<kernel::BRepModel>  m_brepModel;

    int m_nextBodyCounter = 1;
    CommandHistory m_history;

    /// Dependency graph tracking inter-feature dependencies.
    DependencyGraph m_depGraph;

    /// Assembly component hierarchy.
    ComponentRegistry m_components;

    /// Material/appearance assignments per body and per face.
    kernel::AppearanceManager m_appearances;

    /// Maps body IDs to the feature ID that created/last modified them.
    std::unordered_map<std::string, std::string> m_bodyToFeature;

    /// Transaction snapshot for atomic command execution rollback.
    struct TransactionSnapshot {
        size_t timelineCount = 0;
        std::vector<std::string> bodyIds;
        bool active = false;
    };
    TransactionSnapshot m_transactionSnapshot;

    /// Last-good shape per body for error recovery.  Before executing a
    /// feature that modifies a body, we snapshot its current shape.  If the
    /// feature fails we restore from this snapshot so downstream features
    /// can continue with a valid body.
    std::unordered_map<std::string, TopoDS_Shape> m_lastGoodShapes;

    /// Feature IDs whose last execution resulted in HealthState::Error.
    /// Used by the viewport to determine red-tint rendering.
    std::unordered_set<std::string> m_erroredFeatureIds;

    /// Cached input hashes per feature id.  If a feature's current input
    /// hash matches the cached value, its execution is skipped during
    /// incremental recompute and the existing body is reused.
    std::unordered_map<std::string, size_t> m_featureInputHashes;

    /// Register a body->feature mapping.
    void registerBodyFeature(const std::string& bodyId, const std::string& featureId);

    /// Generate a numbered feature name (e.g. "Extrude 3") by counting existing
    /// features of the given type in the timeline.
    std::string generateFeatureName(features::FeatureType type) const;

    /// Execute a single feature during recompute (shared logic).
    void executeFeature(features::Feature* feat, int& bodyCounter);

    /// Snapshot the current shape of a body before a feature modifies it.
    void snapshotBody(const std::string& bodyId);

    /// Restore a body shape from the last-good snapshot.
    void restoreBody(const std::string& bodyId);

    /// Compute a quick fingerprint hash of a feature's inputs: its serialized
    /// params string combined with a cheap body geometry summary.
    size_t computeFeatureInputHash(features::Feature* feat) const;

    /// Auto-tag faces created by a feature (extrude start/end/side, fillet, etc.)
    void autoTagFeatureFaces(features::Feature* feat,
                             const std::string& bodyId,
                             const TopoDS_Shape& shape);

    /// Propagate attributes from old shape to new shape for a body after a
    /// modifying feature (fillet, chamfer, shell, etc.) runs.
    void propagateAttributes(const std::string& bodyId,
                             const TopoDS_Shape& oldShape,
                             const TopoDS_Shape& newShape);

public:
    /// Query whether any feature on a given body is in error state.
    /// Used by the viewport to apply red tinting.
    bool bodyHasError(const std::string& bodyId) const;

    /// Access the set of errored feature IDs (read-only).
    const std::unordered_set<std::string>& erroredFeatureIds() const { return m_erroredFeatureIds; }
};

} // namespace document
