#include "DependencyGraph.h"
#include <queue>
#include <set>
#include <algorithm>

namespace document {

void DependencyGraph::addNode(const std::string& featureId)
{
    m_nodes.insert(featureId);
    // Ensure adjacency entries exist even if empty
    m_dependents[featureId];
    m_dependencies[featureId];
}

void DependencyGraph::removeNode(const std::string& featureId)
{
    if (m_nodes.find(featureId) == m_nodes.end())
        return;

    // Remove all edges where featureId is a dependency (i.e. others depend on it)
    if (auto it = m_dependents.find(featureId); it != m_dependents.end()) {
        for (const auto& dep : it->second) {
            auto depIt = m_dependencies.find(dep);
            if (depIt != m_dependencies.end())
                depIt->second.erase(featureId);
        }
        m_dependents.erase(it);
    }

    // Remove all edges where featureId is a dependent (i.e. it depends on others)
    if (auto it = m_dependencies.find(featureId); it != m_dependencies.end()) {
        for (const auto& dep : it->second) {
            auto depIt = m_dependents.find(dep);
            if (depIt != m_dependents.end())
                depIt->second.erase(featureId);
        }
        m_dependencies.erase(it);
    }

    m_nodes.erase(featureId);
}

void DependencyGraph::addEdge(const std::string& dependencyId, const std::string& dependentId)
{
    // Ensure both nodes exist
    addNode(dependencyId);
    addNode(dependentId);

    m_dependents[dependencyId].insert(dependentId);
    m_dependencies[dependentId].insert(dependencyId);
}

void DependencyGraph::removeEdge(const std::string& dependencyId, const std::string& dependentId)
{
    if (auto it = m_dependents.find(dependencyId); it != m_dependents.end())
        it->second.erase(dependentId);
    if (auto it = m_dependencies.find(dependentId); it != m_dependencies.end())
        it->second.erase(dependencyId);
}

std::vector<std::string> DependencyGraph::dependenciesOf(const std::string& featureId) const
{
    std::vector<std::string> result;
    auto it = m_dependencies.find(featureId);
    if (it != m_dependencies.end()) {
        result.assign(it->second.begin(), it->second.end());
        std::sort(result.begin(), result.end());
    }
    return result;
}

std::vector<std::string> DependencyGraph::dependentsOf(const std::string& featureId) const
{
    std::vector<std::string> result;
    auto it = m_dependents.find(featureId);
    if (it != m_dependents.end()) {
        result.assign(it->second.begin(), it->second.end());
        std::sort(result.begin(), result.end());
    }
    return result;
}

std::vector<std::string> DependencyGraph::allDependentsOf(const std::string& featureId) const
{
    std::unordered_set<std::string> visited;
    collectDownstream(featureId, visited);
    visited.erase(featureId); // collectDownstream includes the seed via BFS
    return std::vector<std::string>(visited.begin(), visited.end());
}

void DependencyGraph::collectDownstream(const std::string& id,
                                         std::unordered_set<std::string>& visited) const
{
    std::queue<std::string> queue;
    queue.push(id);

    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();

        auto it = m_dependents.find(current);
        if (it == m_dependents.end())
            continue;

        for (const auto& dep : it->second) {
            if (visited.insert(dep).second)
                queue.push(dep);
        }
    }
}

std::vector<std::string> DependencyGraph::propagateDirty(const std::vector<std::string>& dirtyIds) const
{
    std::unordered_set<std::string> dirtySet(dirtyIds.begin(), dirtyIds.end());

    // For each initially dirty node, collect all transitive dependents
    for (const auto& id : dirtyIds)
        collectDownstream(id, dirtySet);

    // Return in topological order so that dependencies are recomputed before dependents
    auto topoOrder = topologicalSort();
    std::vector<std::string> result;
    result.reserve(dirtySet.size());
    for (const auto& id : topoOrder) {
        if (dirtySet.count(id))
            result.push_back(id);
    }
    return result;
}

std::vector<std::string> DependencyGraph::topologicalSort() const
{
    // Kahn's algorithm
    std::unordered_map<std::string, int> inDegree;
    for (const auto& node : m_nodes)
        inDegree[node] = 0;

    for (const auto& [node, deps] : m_dependencies) {
        if (m_nodes.count(node))
            inDegree[node] = static_cast<int>(deps.size());
    }

    // Use a sorted container to get deterministic ordering among peers
    // (nodes with the same in-degree). This uses a std::set as a priority queue.
    std::set<std::string> ready;
    for (const auto& [node, deg] : inDegree) {
        if (deg == 0)
            ready.insert(node);
    }

    std::vector<std::string> result;
    result.reserve(m_nodes.size());

    while (!ready.empty()) {
        auto it = ready.begin();
        std::string current = *it;
        ready.erase(it);
        result.push_back(current);

        auto depIt = m_dependents.find(current);
        if (depIt != m_dependents.end()) {
            for (const auto& dep : depIt->second) {
                inDegree[dep]--;
                if (inDegree[dep] == 0)
                    ready.insert(dep);
            }
        }
    }

    // If not all nodes were visited, there is a cycle
    if (result.size() != m_nodes.size())
        return {};

    return result;
}

bool DependencyGraph::wouldCreateCycle(const std::string& fromId, const std::string& toId) const
{
    if (fromId == toId)
        return true;

    // Adding edge fromId -> toId means toId depends on fromId.
    // This creates a cycle if fromId is already reachable from toId
    // (i.e. fromId is downstream of toId).
    std::unordered_set<std::string> visited;
    visited.insert(toId);
    collectDownstream(toId, visited);
    return visited.count(fromId) > 0;
}

bool DependencyGraph::hasNode(const std::string& featureId) const
{
    return m_nodes.count(featureId) > 0;
}

void DependencyGraph::clear()
{
    m_nodes.clear();
    m_dependents.clear();
    m_dependencies.clear();
}

} // namespace document
