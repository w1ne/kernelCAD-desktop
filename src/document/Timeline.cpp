#include "Timeline.h"
#include "DependencyGraph.h"
#include <algorithm>
#include <stdexcept>
#include <sstream>

namespace document {

void Timeline::append(std::shared_ptr<features::Feature> feature)
{
    TimelineEntry e;
    e.id      = feature->id();
    e.name    = feature->name();
    e.feature = std::move(feature);
    m_entries.push_back(std::move(e));
    m_markerPos = m_entries.size();
}

void Timeline::remove(const std::string& id)
{
    // Find the index before erasing so we can adjust groups
    size_t removedIdx = indexOfFeature(id);

    m_entries.erase(
        std::remove_if(m_entries.begin(), m_entries.end(),
            [&id](const TimelineEntry& e){ return e.id == id; }),
        m_entries.end());
    m_markerPos = std::min(m_markerPos, m_entries.size());

    if (removedIdx < m_entries.size() + 1) // was a valid index before removal
        adjustGroupIndicesAfterRemove(removedIdx);
}

size_t Timeline::indexOfFeature(const std::string& featureId) const
{
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id == featureId)
            return i;
    }
    return m_entries.size();
}

bool Timeline::canReorder(size_t fromIndex, size_t toIndex) const
{
    if (fromIndex >= m_entries.size() || toIndex > m_entries.size())
        return false;
    return fromIndex != toIndex;
}

bool Timeline::canReorder(size_t fromIndex, size_t toIndex, const DependencyGraph& depGraph) const
{
    // Basic bounds/identity check first
    if (!canReorder(fromIndex, toIndex))
        return false;

    const std::string& featureId = m_entries[fromIndex].id;

    // Compute where the feature would end up after removal + insertion
    size_t insertAt = (toIndex > fromIndex) ? toIndex - 1 : toIndex;

    // The feature cannot be placed before any of its dependencies.
    auto deps = depGraph.dependenciesOf(featureId);
    for (const auto& depId : deps) {
        size_t depIdx = indexOfFeature(depId);
        if (depIdx == m_entries.size())
            continue;
        // Adjust depIdx if the removal shifts it
        size_t adjustedDepIdx = depIdx;
        if (depIdx > fromIndex)
            adjustedDepIdx--;
        if (insertAt <= adjustedDepIdx)
            return false; // Would place this feature before a dependency
    }

    // The feature cannot be placed after any of its dependents.
    auto dependents = depGraph.dependentsOf(featureId);
    for (const auto& depId : dependents) {
        size_t depIdx = indexOfFeature(depId);
        if (depIdx == m_entries.size())
            continue;
        // Adjust depIdx if the removal shifts it
        size_t adjustedDepIdx = depIdx;
        if (depIdx > fromIndex)
            adjustedDepIdx--;
        if (insertAt >= adjustedDepIdx)
            return false; // Would place this feature after a dependent
    }

    return true;
}

bool Timeline::reorder(size_t fromIndex, size_t toIndex)
{
    if (!canReorder(fromIndex, toIndex)) return false;
    TimelineEntry entry = std::move(m_entries[fromIndex]);
    m_entries.erase(m_entries.begin() + fromIndex);
    size_t insertAt = (toIndex > fromIndex) ? toIndex - 1 : toIndex;
    m_entries.insert(m_entries.begin() + insertAt, std::move(entry));
    return true;
}

bool Timeline::reorder(size_t fromIndex, size_t toIndex, const DependencyGraph& depGraph)
{
    if (!canReorder(fromIndex, toIndex, depGraph)) return false;
    TimelineEntry entry = std::move(m_entries[fromIndex]);
    m_entries.erase(m_entries.begin() + fromIndex);
    size_t insertAt = (toIndex > fromIndex) ? toIndex - 1 : toIndex;
    m_entries.insert(m_entries.begin() + insertAt, std::move(entry));
    return true;
}

void Timeline::insert(size_t index, TimelineEntry entry)
{
    if (index > m_entries.size())
        index = m_entries.size();
    m_entries.insert(m_entries.begin() + static_cast<ptrdiff_t>(index), std::move(entry));
    m_markerPos = m_entries.size();
}

bool Timeline::replaceFeature(size_t index, std::shared_ptr<features::Feature> newFeature)
{
    if (index >= m_entries.size())
        return false;
    m_entries[index].id      = newFeature->id();
    m_entries[index].name    = newFeature->name();
    m_entries[index].feature = std::move(newFeature);
    return true;
}

void Timeline::setMarker(size_t index)
{
    m_markerPos = std::min(index, m_entries.size());
    for (size_t i = 0; i < m_entries.size(); ++i)
        m_entries[i].isRolledBack = (i >= m_markerPos);
}

bool Timeline::isEffectivelySuppressed(size_t index) const
{
    if (index >= m_entries.size())
        return false;
    if (m_entries[index].isSuppressed)
        return true;
    // Check if the entry belongs to a suppressed group
    const TimelineGroup* grp = groupForEntry(index);
    return grp && grp->isSuppressed;
}

std::vector<size_t> Timeline::dirtyFrom(size_t index) const
{
    std::vector<size_t> result;
    for (size_t i = index; i < m_markerPos; ++i)
        if (!isEffectivelySuppressed(i))
            result.push_back(i);
    return result;
}

// ── Timeline Groups ──────────────────────────────────────────────────────────

std::string Timeline::createGroup(const std::string& name, size_t startIdx, size_t endIdx)
{
    if (startIdx > endIdx || endIdx >= m_entries.size())
        throw std::runtime_error("Timeline::createGroup: invalid range");

    // Check for overlap with existing groups
    for (const auto& g : m_groups) {
        bool overlaps = !(endIdx < g.startIndex || startIdx > g.endIndex);
        if (overlaps)
            throw std::runtime_error("Timeline::createGroup: overlaps with group '" + g.name + "'");
    }

    std::ostringstream idStream;
    idStream << "group_" << m_nextGroupCounter++;
    std::string groupId = idStream.str();

    TimelineGroup grp;
    grp.id         = groupId;
    grp.name       = name;
    grp.startIndex = startIdx;
    grp.endIndex   = endIdx;

    // Insert sorted by startIndex
    auto insertPos = std::lower_bound(m_groups.begin(), m_groups.end(), grp,
        [](const TimelineGroup& a, const TimelineGroup& b) {
            return a.startIndex < b.startIndex;
        });
    m_groups.insert(insertPos, std::move(grp));

    return groupId;
}

void Timeline::removeGroup(const std::string& groupId, bool deleteContents)
{
    auto it = std::find_if(m_groups.begin(), m_groups.end(),
        [&groupId](const TimelineGroup& g) { return g.id == groupId; });
    if (it == m_groups.end())
        return;

    if (deleteContents) {
        // Remove entries from endIndex down to startIndex (reverse to preserve indices)
        size_t start = it->startIndex;
        size_t end   = it->endIndex;

        // Erase group first so adjustGroupIndicesAfterRemove doesn't see it
        m_groups.erase(it);

        for (size_t i = end; i >= start && i < m_entries.size(); --i) {
            m_entries.erase(m_entries.begin() + static_cast<ptrdiff_t>(i));
            adjustGroupIndicesAfterRemove(i);
            if (i == 0) break; // avoid underflow
        }
        m_markerPos = std::min(m_markerPos, m_entries.size());
    } else {
        m_groups.erase(it);
    }
}

void Timeline::setGroupCollapsed(const std::string& groupId, bool collapsed)
{
    for (auto& g : m_groups) {
        if (g.id == groupId) {
            g.isCollapsed = collapsed;
            return;
        }
    }
}

void Timeline::setGroupSuppressed(const std::string& groupId, bool suppressed)
{
    for (auto& g : m_groups) {
        if (g.id == groupId) {
            g.isSuppressed = suppressed;
            return;
        }
    }
}

const TimelineGroup* Timeline::groupForEntry(size_t index) const
{
    for (const auto& g : m_groups) {
        if (index >= g.startIndex && index <= g.endIndex)
            return &g;
    }
    return nullptr;
}

void Timeline::adjustGroupIndicesAfterRemove(size_t removedIndex)
{
    auto it = m_groups.begin();
    while (it != m_groups.end()) {
        auto& g = *it;
        if (removedIndex > g.endIndex) {
            // Group is entirely before the removed entry -- no change
            ++it;
        } else if (removedIndex < g.startIndex) {
            // Group is entirely after the removed entry -- shift both indices
            g.startIndex--;
            g.endIndex--;
            ++it;
        } else {
            // Removed entry is inside the group
            if (g.startIndex == g.endIndex) {
                // Group had only one entry -- remove the group
                it = m_groups.erase(it);
            } else {
                g.endIndex--;
                ++it;
            }
        }
    }
}

} // namespace document
