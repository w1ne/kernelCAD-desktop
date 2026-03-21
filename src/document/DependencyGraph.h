#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace document {

/// Directed acyclic graph tracking feature dependencies.
/// An edge from A -> B means "B depends on A" (B uses A's output).
class DependencyGraph {
public:
    /// Add a feature node.
    void addNode(const std::string& featureId);

    /// Remove a node and all its edges.
    void removeNode(const std::string& featureId);

    /// Add a dependency: dependentId depends on dependencyId.
    void addEdge(const std::string& dependencyId, const std::string& dependentId);

    /// Remove a dependency edge.
    void removeEdge(const std::string& dependencyId, const std::string& dependentId);

    /// Get all direct dependencies of a feature (what it depends on).
    std::vector<std::string> dependenciesOf(const std::string& featureId) const;

    /// Get all direct dependents of a feature (what depends on it).
    std::vector<std::string> dependentsOf(const std::string& featureId) const;

    /// Given a set of dirty features, compute the full set that needs recompute
    /// (all downstream dependents, transitively). The returned list includes
    /// the initially dirty features themselves.
    std::vector<std::string> propagateDirty(const std::vector<std::string>& dirtyIds) const;

    /// Topological sort of all features (respecting dependencies).
    /// Returns empty vector if a cycle is detected.
    std::vector<std::string> topologicalSort() const;

    /// Check if adding an edge would create a cycle.
    bool wouldCreateCycle(const std::string& fromId, const std::string& toId) const;

    /// Whether a node exists in the graph.
    bool hasNode(const std::string& featureId) const;

    void clear();

private:
    std::unordered_map<std::string, std::unordered_set<std::string>> m_dependents;   // node -> set of nodes that depend on it
    std::unordered_map<std::string, std::unordered_set<std::string>> m_dependencies; // node -> set of nodes it depends on
    std::unordered_set<std::string> m_nodes;

    // BFS helper for transitive closure (collects all downstream of id)
    void collectDownstream(const std::string& id, std::unordered_set<std::string>& visited) const;
};

} // namespace document
