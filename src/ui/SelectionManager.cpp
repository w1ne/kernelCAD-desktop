#include "SelectionManager.h"
#include <algorithm>

// ---------------------------------------------------------------------------
// Filter
// ---------------------------------------------------------------------------

void SelectionManager::setFilter(SelectionFilter filter)
{
    if (m_filter != filter) {
        m_filter = filter;
        clearSelection();
        clearPreSelection();
    }
}

SelectionFilter SelectionManager::filter() const
{
    return m_filter;
}

void SelectionManager::setFilterSoft(SelectionFilter filter)
{
    // Change the filter for future picks without clearing the current selection.
    m_filter = filter;
    clearPreSelection();
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------

void SelectionManager::select(const SelectionHit& hit)
{
    m_selection.clear();

    // Respect max-selections limit (at least 1)
    if (m_maxSelections > 0)
        m_selection.push_back(hit);

    if (m_onChanged)
        m_onChanged(m_selection);
}

void SelectionManager::addToSelection(const SelectionHit& hit)
{
    // Avoid duplicates: same body + face + edge
    auto sameEntity = [&](const SelectionHit& h) {
        return h.bodyId == hit.bodyId &&
               h.faceIndex == hit.faceIndex &&
               h.edgeIndex == hit.edgeIndex;
    };

    auto it = std::find_if(m_selection.begin(), m_selection.end(), sameEntity);
    if (it != m_selection.end()) {
        // Toggle off — deselect the entity
        m_selection.erase(it);
    } else {
        // Add if within max limit
        if (static_cast<int>(m_selection.size()) < m_maxSelections)
            m_selection.push_back(hit);
    }

    if (m_onChanged)
        m_onChanged(m_selection);
}

void SelectionManager::clearSelection()
{
    if (!m_selection.empty()) {
        m_selection.clear();
        if (m_onChanged)
            m_onChanged(m_selection);
    }
}

const std::vector<SelectionHit>& SelectionManager::selection() const
{
    return m_selection;
}

bool SelectionManager::hasSelection() const
{
    return !m_selection.empty();
}

// ---------------------------------------------------------------------------
// Pre-selection (hover)
// ---------------------------------------------------------------------------

void SelectionManager::setPreSelection(const SelectionHit& hit)
{
    m_preSelection = hit;
    m_hasPreSelection = true;
}

void SelectionManager::clearPreSelection()
{
    m_hasPreSelection = false;
}

const SelectionHit* SelectionManager::preSelection() const
{
    return m_hasPreSelection ? &m_preSelection : nullptr;
}

bool SelectionManager::hasPreSelection() const
{
    return m_hasPreSelection;
}

// ---------------------------------------------------------------------------
// Selection limits
// ---------------------------------------------------------------------------

void SelectionManager::setSelectionLimits(int min, int max)
{
    m_minSelections = min;
    m_maxSelections = max;
}

bool SelectionManager::isSelectionComplete() const
{
    const int count = static_cast<int>(m_selection.size());
    return count >= m_minSelections && count <= m_maxSelections;
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

void SelectionManager::setOnSelectionChanged(SelectionCallback cb)
{
    m_onChanged = std::move(cb);
}
