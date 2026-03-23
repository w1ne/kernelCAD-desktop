#pragma once
#include <QIcon>
#include <QString>
#include <QStringList>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

struct ToolDefinition {
    std::string id;           // unique: "fillet", "createBox", "sketchLine"
    QString name;             // display: "Fillet", "Box", "Line"
    QString shortcut;         // keyboard: "F", "Ctrl+N", "L"
    QString group;            // ribbon group: "Create", "Modify", "Draw"
    QString tab;              // ribbon tab: "SOLID", "SKETCH", "ASSEMBLY"
    QString menuPath;         // menu: "Model", "Sketch", "File"
    QIcon icon;               // toolbar icon
    QString tooltip;          // full tooltip text
    QString helpParams;       // CLI help: "{bodyId, radius}"
    QString helpReturns;      // CLI help: "{featureId, bodyId}"
    QString helpHint;         // CLI help: "Rounds edges. Omit edgeIds for all."
    std::function<void()> action;  // what happens when activated
    bool showInContextMenu = false;  // show in right-click menu
    QString contextMenuFor;   // "face", "edge", "body", "empty", "" (context filter)
    int sortOrder = 100;      // for ordering within group
    bool isDropdownExtra = false;  // shown only in the dropdown overflow, not primary ribbon row
};

class ToolRegistry {
public:
    static ToolRegistry& instance();

    /// Remove all registered tools (for re-initialization).
    void clear();

    /// Register a tool. Called once during app startup.
    void registerTool(ToolDefinition def);

    /// Get all tools for a ribbon tab (e.g., "SOLID")
    std::vector<const ToolDefinition*> toolsForTab(const QString& tab) const;

    /// Get all tools for a ribbon group within a tab
    std::vector<const ToolDefinition*> toolsForGroup(const QString& tab, const QString& group) const;

    /// Get all group names for a tab (in insertion order)
    QStringList groupsForTab(const QString& tab) const;

    /// Get all tab names (in insertion order)
    QStringList tabs() const;

    /// Get all tools for a menu path (e.g., "Model", "File")
    std::vector<const ToolDefinition*> toolsForMenu(const QString& menuPath) const;

    /// Get tools for context menu (filtered by what's under cursor)
    std::vector<const ToolDefinition*> contextMenuTools(const QString& contextType) const;

    /// Find a tool by ID (for CLI/scripting)
    const ToolDefinition* findById(const std::string& id) const;

    /// Get all tools (for command palette search)
    const std::vector<ToolDefinition>& allTools() const { return m_tools; }

private:
    ToolRegistry() = default;
    std::vector<ToolDefinition> m_tools;
    std::unordered_map<std::string, size_t> m_idIndex;
};
