#include "Component.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace document {

// ═══════════════════════════════════════════════════════════════════════════════
// Component
// ═══════════════════════════════════════════════════════════════════════════════

Component::Component(const std::string& id, const std::string& name)
    : m_id(id)
    , m_name(name)
{}

const std::string& Component::id() const { return m_id; }
const std::string& Component::name() const { return m_name; }
void Component::setName(const std::string& name) { m_name = name; }

std::string Component::addOccurrence(const std::string& componentId, const std::string& name)
{
    Occurrence occ;
    std::ostringstream oss;
    oss << m_id << ":occ_" << m_nextOccId++;
    occ.id = oss.str();
    occ.name = name.empty() ? ("Occurrence " + std::to_string(m_nextOccId - 1)) : name;
    occ.componentId = componentId;
    // transform is identity by default
    m_occurrences.push_back(std::move(occ));
    return m_occurrences.back().id;
}

void Component::removeOccurrence(const std::string& occurrenceId)
{
    m_occurrences.erase(
        std::remove_if(m_occurrences.begin(), m_occurrences.end(),
            [&](const Occurrence& o) { return o.id == occurrenceId; }),
        m_occurrences.end());
}

const std::vector<Component::Occurrence>& Component::occurrences() const
{
    return m_occurrences;
}

Component::Occurrence* Component::findOccurrence(const std::string& id)
{
    for (auto& occ : m_occurrences) {
        if (occ.id == id)
            return &occ;
    }
    return nullptr;
}

void Component::addBodyRef(const std::string& bodyId)
{
    // Avoid duplicates
    if (std::find(m_bodyRefs.begin(), m_bodyRefs.end(), bodyId) == m_bodyRefs.end())
        m_bodyRefs.push_back(bodyId);
}

void Component::removeBodyRef(const std::string& bodyId)
{
    m_bodyRefs.erase(
        std::remove(m_bodyRefs.begin(), m_bodyRefs.end(), bodyId),
        m_bodyRefs.end());
}

const std::vector<std::string>& Component::bodyRefs() const
{
    return m_bodyRefs;
}

bool Component::isActive() const { return m_isActive; }
void Component::setActive(bool active) { m_isActive = active; }

// ═══════════════════════════════════════════════════════════════════════════════
// ComponentRegistry
// ═══════════════════════════════════════════════════════════════════════════════

std::string ComponentRegistry::createComponent(const std::string& name)
{
    std::ostringstream oss;
    oss << "comp_" << m_nextId++;
    std::string id = oss.str();

    auto comp = std::make_unique<Component>(id, name);

    // First component created becomes the root
    if (m_rootId.empty())
        m_rootId = id;

    m_components[id] = std::move(comp);
    return id;
}

Component* ComponentRegistry::findComponent(const std::string& id)
{
    auto it = m_components.find(id);
    return (it != m_components.end()) ? it->second.get() : nullptr;
}

const Component* ComponentRegistry::findComponent(const std::string& id) const
{
    auto it = m_components.find(id);
    return (it != m_components.end()) ? it->second.get() : nullptr;
}

Component& ComponentRegistry::rootComponent()
{
    auto* c = findComponent(m_rootId);
    if (!c)
        throw std::runtime_error("ComponentRegistry: no root component");
    return *c;
}

const Component& ComponentRegistry::rootComponent() const
{
    auto* c = findComponent(m_rootId);
    if (!c)
        throw std::runtime_error("ComponentRegistry: no root component");
    return *c;
}

std::vector<std::string> ComponentRegistry::componentIds() const
{
    std::vector<std::string> ids;
    ids.reserve(m_components.size());
    for (const auto& [id, _] : m_components)
        ids.push_back(id);
    return ids;
}

void ComponentRegistry::clear()
{
    m_components.clear();
    m_rootId.clear();
    m_nextId = 1;
}

} // namespace document
