#include "ToolRegistry.h"
#include <algorithm>

ToolRegistry& ToolRegistry::instance()
{
    static ToolRegistry reg;
    return reg;
}

void ToolRegistry::clear()
{
    m_tools.clear();
    m_idIndex.clear();
}

void ToolRegistry::registerTool(ToolDefinition def)
{
    m_idIndex[def.id] = m_tools.size();
    m_tools.push_back(std::move(def));
}

std::vector<const ToolDefinition*> ToolRegistry::toolsForTab(const QString& tab) const
{
    std::vector<const ToolDefinition*> result;
    for (const auto& t : m_tools)
        if (t.tab == tab)
            result.push_back(&t);
    std::sort(result.begin(), result.end(),
              [](const ToolDefinition* a, const ToolDefinition* b) {
                  return a->sortOrder < b->sortOrder;
              });
    return result;
}

std::vector<const ToolDefinition*> ToolRegistry::toolsForGroup(const QString& tab, const QString& group) const
{
    std::vector<const ToolDefinition*> result;
    for (const auto& t : m_tools)
        if (t.tab == tab && t.group == group)
            result.push_back(&t);
    std::sort(result.begin(), result.end(),
              [](const ToolDefinition* a, const ToolDefinition* b) {
                  return a->sortOrder < b->sortOrder;
              });
    return result;
}

QStringList ToolRegistry::groupsForTab(const QString& tab) const
{
    QStringList groups;
    for (const auto& t : m_tools) {
        if (t.tab == tab && !groups.contains(t.group))
            groups.append(t.group);
    }
    return groups;
}

QStringList ToolRegistry::tabs() const
{
    QStringList tabList;
    for (const auto& t : m_tools) {
        if (!tabList.contains(t.tab))
            tabList.append(t.tab);
    }
    return tabList;
}

std::vector<const ToolDefinition*> ToolRegistry::toolsForMenu(const QString& menuPath) const
{
    std::vector<const ToolDefinition*> result;
    for (const auto& t : m_tools)
        if (t.menuPath == menuPath)
            result.push_back(&t);
    std::sort(result.begin(), result.end(),
              [](const ToolDefinition* a, const ToolDefinition* b) {
                  return a->sortOrder < b->sortOrder;
              });
    return result;
}

std::vector<const ToolDefinition*> ToolRegistry::contextMenuTools(const QString& contextType) const
{
    std::vector<const ToolDefinition*> result;
    for (const auto& t : m_tools) {
        if (!t.showInContextMenu)
            continue;
        if (t.contextMenuFor.isEmpty() || t.contextMenuFor == contextType)
            result.push_back(&t);
    }
    std::sort(result.begin(), result.end(),
              [](const ToolDefinition* a, const ToolDefinition* b) {
                  return a->sortOrder < b->sortOrder;
              });
    return result;
}

const ToolDefinition* ToolRegistry::findById(const std::string& id) const
{
    auto it = m_idIndex.find(id);
    if (it != m_idIndex.end())
        return &m_tools[it->second];
    return nullptr;
}
