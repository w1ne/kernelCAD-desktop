#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <optional>
#include "OCCTKernel.h"
#include "EntityAttribute.h"
#include "BRepQuery.h"

class TopoDS_Shape;

namespace kernel {

/// Stable entity identifier — survives boolean operations via GenerationRule
struct EntityId {
    std::string value;
    bool operator==(const EntityId& o) const { return value == o.value; }
};

/// Lightweight wrapper around a computed body's tessellation + metadata.
/// The TopoDS_Shape lives inside OCCTKernel and is referenced by bodyId.
struct BodyRecord {
    std::string     id;           // stable UUID
    std::string     name;
    std::string     revisionId;   // changes on every recompute
    bool            isVisible = true;
};

/// Holds the computed B-Rep model: a map of body IDs to TopoDS_Shape,
/// plus a lazily-computed tessellated mesh cache per body.
class BRepModel
{
public:
    BRepModel();
    ~BRepModel();

    // ── Body management ────────────────────────────────────────────────
    void addBody(const std::string& id, const TopoDS_Shape& shape);
    void removeBody(const std::string& id);
    void clear();

    // ── Queries ────────────────────────────────────────────────────────
    bool hasBody(const std::string& id) const;
    const TopoDS_Shape& getShape(const std::string& id) const;
    OCCTKernel::Mesh getMesh(const std::string& id, double deflection = 0.1);
    OCCTKernel::EdgeMesh getEdgeMesh(const std::string& id, double deflection = 0.1);
    std::vector<std::string> bodyIds() const;

    // ── Attribute map per body ────────────────────────────────────────
    /// Get mutable attribute map for a body. Creates an empty map if none exists.
    AttributeMap& attributes(const std::string& bodyId);

    /// Get const attribute map for a body. Returns a static empty map if none exists.
    const AttributeMap& attributes(const std::string& bodyId) const;

    // ── B-Rep topology query ────────────────────────────────────────
    /// Create a BRepQuery object for inspecting topology of a body.
    BRepQuery query(const std::string& bodyId) const;

    // ── Physical properties ────────────────────────────────────────
    /// Compute physical properties for a body (volume, area, mass, CoG, bbox).
    /// @param density Material density in g/mm^3 (default: steel = 0.00785).
    OCCTKernel::PhysicalProperties getProperties(const std::string& bodyId,
                                                  double density = 0.00785);

    // ── Body visibility ──────────────────────────────────────────────
    bool isBodyVisible(const std::string& id) const;
    void setBodyVisible(const std::string& id, bool visible);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace kernel
