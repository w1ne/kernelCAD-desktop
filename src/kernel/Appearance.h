#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace kernel {

struct Material {
    std::string name = "Default";
    // Base color (sRGB, 0-1)
    float baseR = 0.6f, baseG = 0.65f, baseB = 0.7f;
    float opacity = 1.0f;
    // PBR-like properties
    float metallic = 0.0f;    // 0 = dielectric, 1 = metal
    float roughness = 0.5f;   // 0 = mirror, 1 = fully rough
    // Density for physical properties (g/mm^3)
    double density = 0.00785;  // steel default
};

/// Predefined material library
struct MaterialLibrary {
    static Material steel();
    static Material aluminum();
    static Material brass();
    static Material copper();
    static Material plastic();
    static Material wood();
    static Material glass();
    static Material rubber();
    static Material gold();
    static Material titanium();
    static Material carbonFiber();

    static const std::vector<Material>& all();
    static const Material* byName(const std::string& name);
};

/// Manages material assignments per body and per face.
class AppearanceManager {
public:
    /// Set material for an entire body.
    void setBodyMaterial(const std::string& bodyId, const Material& mat);

    /// Set material for a specific face of a body.
    void setFaceMaterial(const std::string& bodyId, int faceIndex, const Material& mat);

    /// Get the effective material for a body (body-level assignment).
    const Material& bodyMaterial(const std::string& bodyId) const;

    /// Get the effective material for a face (face override, or fallback to body).
    const Material& faceMaterial(const std::string& bodyId, int faceIndex) const;

    /// Check if a face has an override.
    bool hasFaceOverride(const std::string& bodyId, int faceIndex) const;

    /// Remove face override.
    void clearFaceOverride(const std::string& bodyId, int faceIndex);

    /// Clear all assignments for a body.
    void clearBody(const std::string& bodyId);

    void clear();

    /// Access body-level material assignments (for serialization).
    const std::unordered_map<std::string, Material>& bodyMaterials() const { return m_bodyMaterials; }

    /// Access face-level material assignments (for serialization).
    const std::unordered_map<std::string, std::unordered_map<int, Material>>& faceMaterials() const { return m_faceMaterials; }

private:
    Material m_defaultMaterial;
    std::unordered_map<std::string, Material> m_bodyMaterials;
    std::unordered_map<std::string, std::unordered_map<int, Material>> m_faceMaterials;
};

} // namespace kernel
