#pragma once
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace kernel { class OCCTKernel; class BRepModel; }
namespace features { class Feature; }

namespace document {

class Document;

/// Manages transient B-Rep preview geometry for live parameter editing.
/// When the user edits a feature parameter in the PropertiesPanel, the
/// PreviewEngine re-executes only that single feature and produces a
/// tessellated mesh that the Viewport3D renders as a transparent overlay.
/// The preview is ephemeral: committing writes the params permanently,
/// cancelling restores the originals.
class PreviewEngine {
public:
    explicit PreviewEngine(Document& doc);
    ~PreviewEngine();

    /// Start a preview session for a feature. Saves the current parameter
    /// values so they can be restored on cancel.
    void beginPreview(const std::string& featureId);

    /// Update the preview by re-executing the feature with its current
    /// (modified) params. Returns true if preview geometry was generated.
    bool updatePreview();

    /// Commit the preview (make the modified params permanent).
    /// Performs a full recompute so downstream features see the change.
    void commitPreview();

    /// Cancel the preview and restore the original parameter values.
    void cancelPreview();

    bool isActive() const;
    const std::string& activeFeatureId() const;

    /// Callback to push preview mesh data to the viewport for rendering.
    using MeshCallback = std::function<void(const std::vector<float>& verts,
                                             const std::vector<float>& normals,
                                             const std::vector<uint32_t>& indices)>;
    void setMeshCallback(MeshCallback cb);

    /// Callback to clear the preview mesh from the viewport.
    using ClearCallback = std::function<void()>;
    void setClearCallback(ClearCallback cb);

private:
    Document& m_doc;
    bool m_active = false;
    std::string m_featureId;
    MeshCallback m_meshCallback;
    ClearCallback m_clearCallback;

    /// Saved parameter values (key -> string representation) for cancel.
    std::unordered_map<std::string, std::string> m_savedParams;

    /// Save the current feature params so we can restore on cancel.
    void saveFeatureParams(features::Feature* feat);

    /// Restore saved feature params (called by cancelPreview).
    void restoreFeatureParams(features::Feature* feat);
};

} // namespace document
