#pragma once
#include <string>
#include <vector>
#include <memory>
#include "../features/Feature.h"

namespace document {

// Forward declaration
class DependencyGraph;

struct TimelineEntry {
    std::string                        id;
    std::string                        name;        ///< Auto-generated name (e.g. "Extrude 1")
    std::string                        customName;  ///< User-assigned rename (empty = use name)
    std::shared_ptr<features::Feature> feature;
    bool                               isSuppressed = false;
    bool                               isRolledBack = false;

    /// Returns customName if set, otherwise the auto-generated name.
    const std::string& displayName() const {
        return customName.empty() ? name : customName;
    }
};

/// A collapsible group of consecutive timeline entries.
struct TimelineGroup {
    std::string id;
    std::string name;
    size_t startIndex = 0;   ///< first entry in the group
    size_t endIndex   = 0;   ///< last entry (inclusive)
    bool isCollapsed  = false;
    bool isSuppressed = false;
};

class Timeline
{
public:
    void append(std::shared_ptr<features::Feature> feature);
    void remove(const std::string& id);

    /// Check if reorder is allowed. Without a dependency graph this performs
    /// only basic bounds checking. With a graph it also validates that the
    /// move does not violate dependency ordering.
    bool canReorder(size_t fromIndex, size_t toIndex) const;
    bool canReorder(size_t fromIndex, size_t toIndex, const DependencyGraph& depGraph) const;

    bool reorder(size_t fromIndex, size_t toIndex);
    bool reorder(size_t fromIndex, size_t toIndex, const DependencyGraph& depGraph);

    void setMarker(size_t index);
    size_t markerPosition() const { return m_markerPos; }
    size_t count() const          { return m_entries.size(); }

    /// Replace a feature at the given index with a new one (same position in history).
    /// Returns false if index is out of bounds.
    bool replaceFeature(size_t index, std::shared_ptr<features::Feature> newFeature);

    const TimelineEntry& entry(size_t index) const { return m_entries[index]; }
    TimelineEntry&       entry(size_t index)       { return m_entries[index]; }

    /// Insert an entry at the given position (used by undo of delete).
    void insert(size_t index, TimelineEntry entry);

    /// Returns indices of entries that need recomputing from dirtyFrom onward.
    /// Entries whose group is suppressed are also skipped.
    std::vector<size_t> dirtyFrom(size_t index) const;

    // ── Timeline Groups ──────────────────────────────────────────────────

    /// Create a group spanning entries [startIdx, endIdx]. Returns the group id.
    std::string createGroup(const std::string& name, size_t startIdx, size_t endIdx);

    /// Remove a group. If deleteContents is true, the entries within the group
    /// are also removed from the timeline.
    void removeGroup(const std::string& groupId, bool deleteContents = false);

    /// Set the collapsed (fold) state of a group.
    void setGroupCollapsed(const std::string& groupId, bool collapsed);

    /// When a group is suppressed, all entries within it are treated as
    /// suppressed during recompute (individual entry isSuppressed flags are
    /// NOT changed -- the group override takes precedence).
    void setGroupSuppressed(const std::string& groupId, bool suppressed);

    /// Read-only access to all groups.
    const std::vector<TimelineGroup>& groups() const { return m_groups; }

    /// Find the group that contains the given entry index, or nullptr.
    const TimelineGroup* groupForEntry(size_t index) const;

    /// Check if entry at index is effectively suppressed (own flag OR group override).
    bool isEffectivelySuppressed(size_t index) const;

private:
    std::vector<TimelineEntry> m_entries;
    size_t                     m_markerPos = 0;

    /// Groups (sorted by startIndex, non-overlapping).
    std::vector<TimelineGroup> m_groups;
    int m_nextGroupCounter = 1;

    /// Helper: find the timeline index for a feature id. Returns count() if not found.
    size_t indexOfFeature(const std::string& featureId) const;

    /// Adjust group indices after an entry removal at the given index.
    void adjustGroupIndicesAfterRemove(size_t removedIndex);
};

} // namespace document
