#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

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

} // namespace kernel
