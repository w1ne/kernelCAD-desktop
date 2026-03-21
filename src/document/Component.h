#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace document {

class Component {
public:
    explicit Component(const std::string& id, const std::string& name = "Component");

    const std::string& id() const;
    const std::string& name() const;
    void setName(const std::string& name);

    // ── Child occurrences ──────────────────────────────────────────────
    struct Occurrence {
        std::string id;
        std::string name;
        std::string componentId;  // ID of the Component this is an instance of
        // Transform (4x4 column-major)
        double transform[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        bool isVisible = true;
        bool isGrounded = false;  // if true, cannot be moved by joints
    };

    std::string addOccurrence(const std::string& componentId, const std::string& name = "");
    void removeOccurrence(const std::string& occurrenceId);
    const std::vector<Occurrence>& occurrences() const;
    Occurrence* findOccurrence(const std::string& id);

    // ── Body references ────────────────────────────────────────────────
    void addBodyRef(const std::string& bodyId);
    void removeBodyRef(const std::string& bodyId);
    const std::vector<std::string>& bodyRefs() const;

    // ── Properties ─────────────────────────────────────────────────────
    bool isActive() const;
    void setActive(bool active);

private:
    std::string m_id;
    std::string m_name;
    bool m_isActive = true;
    std::vector<Occurrence> m_occurrences;
    std::vector<std::string> m_bodyRefs;
    int m_nextOccId = 1;
};

/// Registry of all components in a design.
class ComponentRegistry {
public:
    /// Create a new component and return its ID.
    std::string createComponent(const std::string& name = "Component");

    /// Get a component by ID.
    Component* findComponent(const std::string& id);
    const Component* findComponent(const std::string& id) const;

    /// Get the root component.
    Component& rootComponent();
    const Component& rootComponent() const;

    /// Get all component IDs.
    std::vector<std::string> componentIds() const;

    void clear();

private:
    std::unordered_map<std::string, std::unique_ptr<Component>> m_components;
    std::string m_rootId;
    int m_nextId = 1;
};

} // namespace document
