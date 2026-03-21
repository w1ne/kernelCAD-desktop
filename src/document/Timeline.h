#pragma once
#include <string>
#include <vector>
#include <memory>
#include "../features/Feature.h"

namespace document {

struct TimelineEntry {
    std::string                        id;
    std::string                        name;
    std::shared_ptr<features::Feature> feature;
    bool                               isSuppressed = false;
    bool                               isRolledBack = false;
};

class Timeline
{
public:
    void append(std::shared_ptr<features::Feature> feature);
    void remove(const std::string& id);
    bool canReorder(size_t fromIndex, size_t toIndex) const;
    bool reorder(size_t fromIndex, size_t toIndex);

    void setMarker(size_t index);
    size_t markerPosition() const { return m_markerPos; }
    size_t count() const          { return m_entries.size(); }

    const TimelineEntry& entry(size_t index) const { return m_entries[index]; }
    TimelineEntry&       entry(size_t index)       { return m_entries[index]; }

    /// Returns indices of entries that need recomputing from dirtyFrom onward
    std::vector<size_t> dirtyFrom(size_t index) const;

private:
    std::vector<TimelineEntry> m_entries;
    size_t                     m_markerPos = 0;
};

} // namespace document
