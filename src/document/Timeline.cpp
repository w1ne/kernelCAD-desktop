#include "Timeline.h"
#include <algorithm>
#include <stdexcept>

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
    m_entries.erase(
        std::remove_if(m_entries.begin(), m_entries.end(),
            [&id](const TimelineEntry& e){ return e.id == id; }),
        m_entries.end());
    m_markerPos = std::min(m_markerPos, m_entries.size());
}

bool Timeline::canReorder(size_t fromIndex, size_t toIndex) const
{
    if (fromIndex >= m_entries.size() || toIndex > m_entries.size())
        return false;
    // TODO: check dependency graph — a feature cannot move before its dependencies
    // or after its dependents. For now accept all moves.
    return fromIndex != toIndex;
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

void Timeline::setMarker(size_t index)
{
    m_markerPos = std::min(index, m_entries.size());
    for (size_t i = 0; i < m_entries.size(); ++i)
        m_entries[i].isRolledBack = (i >= m_markerPos);
}

std::vector<size_t> Timeline::dirtyFrom(size_t index) const
{
    std::vector<size_t> result;
    for (size_t i = index; i < m_markerPos; ++i)
        if (!m_entries[i].isSuppressed)
            result.push_back(i);
    return result;
}

} // namespace document
