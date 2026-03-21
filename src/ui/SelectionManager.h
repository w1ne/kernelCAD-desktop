#pragma once
#include <string>
#include <vector>
#include <functional>

enum class SelectionFilter {
    All, Faces, Edges, Vertices, Bodies, Sketches, ConstructionGeometry
};

struct SelectionHit {
    std::string bodyId;
    int faceIndex = -1;      // -1 = no face hit
    int edgeIndex = -1;
    float worldX = 0.0f, worldY = 0.0f, worldZ = 0.0f;  // 3D hit point
    float depth = 0.0f;     // distance from camera
};

class SelectionManager {
public:
    SelectionManager() = default;

    // Set active selection filter
    void setFilter(SelectionFilter filter);
    SelectionFilter filter() const;

    // Current selection
    void select(const SelectionHit& hit);
    void addToSelection(const SelectionHit& hit);  // multi-select (Shift+click)
    void clearSelection();

    const std::vector<SelectionHit>& selection() const;
    bool hasSelection() const;

    // Pre-selection (hover highlight)
    void setPreSelection(const SelectionHit& hit);
    void clearPreSelection();
    const SelectionHit* preSelection() const;
    bool hasPreSelection() const;

    // Selection limits for commands
    void setSelectionLimits(int min, int max);
    bool isSelectionComplete() const;

    // Callbacks
    using SelectionCallback = std::function<void(const std::vector<SelectionHit>&)>;
    void setOnSelectionChanged(SelectionCallback cb);

private:
    SelectionFilter m_filter = SelectionFilter::All;
    std::vector<SelectionHit> m_selection;
    SelectionHit m_preSelection;
    bool m_hasPreSelection = false;
    int m_minSelections = 0;
    int m_maxSelections = 999;
    SelectionCallback m_onChanged;
};
