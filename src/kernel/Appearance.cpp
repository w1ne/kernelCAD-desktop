#include "Appearance.h"

namespace kernel {

// =============================================================================
// MaterialLibrary — predefined materials with realistic PBR values
// =============================================================================

Material MaterialLibrary::steel()
{
    Material m;
    m.name = "Steel";
    m.baseR = 0.6f;  m.baseG = 0.6f;  m.baseB = 0.65f;
    m.opacity = 1.0f;
    m.metallic = 0.8f;
    m.roughness = 0.4f;
    m.density = 0.00785;
    return m;
}

Material MaterialLibrary::aluminum()
{
    Material m;
    m.name = "Aluminum";
    m.baseR = 0.8f;  m.baseG = 0.8f;  m.baseB = 0.82f;
    m.opacity = 1.0f;
    m.metallic = 0.9f;
    m.roughness = 0.3f;
    m.density = 0.0027;
    return m;
}

Material MaterialLibrary::brass()
{
    Material m;
    m.name = "Brass";
    m.baseR = 0.78f; m.baseG = 0.67f; m.baseB = 0.35f;
    m.opacity = 1.0f;
    m.metallic = 0.9f;
    m.roughness = 0.3f;
    m.density = 0.0085;
    return m;
}

Material MaterialLibrary::copper()
{
    Material m;
    m.name = "Copper";
    m.baseR = 0.85f; m.baseG = 0.55f; m.baseB = 0.40f;
    m.opacity = 1.0f;
    m.metallic = 0.95f;
    m.roughness = 0.25f;
    m.density = 0.00896;
    return m;
}

Material MaterialLibrary::plastic()
{
    Material m;
    m.name = "Plastic";
    m.baseR = 0.3f;  m.baseG = 0.3f;  m.baseB = 0.8f;
    m.opacity = 1.0f;
    m.metallic = 0.0f;
    m.roughness = 0.6f;
    m.density = 0.00125;
    return m;
}

Material MaterialLibrary::wood()
{
    Material m;
    m.name = "Wood";
    m.baseR = 0.55f; m.baseG = 0.35f; m.baseB = 0.18f;
    m.opacity = 1.0f;
    m.metallic = 0.0f;
    m.roughness = 0.8f;
    m.density = 0.0006;
    return m;
}

Material MaterialLibrary::glass()
{
    Material m;
    m.name = "Glass";
    m.baseR = 0.85f; m.baseG = 0.9f;  m.baseB = 0.95f;
    m.opacity = 0.4f;
    m.metallic = 0.0f;
    m.roughness = 0.05f;
    m.density = 0.0025;
    return m;
}

Material MaterialLibrary::rubber()
{
    Material m;
    m.name = "Rubber";
    m.baseR = 0.15f; m.baseG = 0.15f; m.baseB = 0.15f;
    m.opacity = 1.0f;
    m.metallic = 0.0f;
    m.roughness = 0.9f;
    m.density = 0.0012;
    return m;
}

Material MaterialLibrary::gold()
{
    Material m;
    m.name = "Gold";
    m.baseR = 0.95f; m.baseG = 0.80f; m.baseB = 0.35f;
    m.opacity = 1.0f;
    m.metallic = 1.0f;
    m.roughness = 0.2f;
    m.density = 0.01932;
    return m;
}

Material MaterialLibrary::titanium()
{
    Material m;
    m.name = "Titanium";
    m.baseR = 0.65f; m.baseG = 0.65f; m.baseB = 0.68f;
    m.opacity = 1.0f;
    m.metallic = 0.85f;
    m.roughness = 0.35f;
    m.density = 0.00451;
    return m;
}

Material MaterialLibrary::carbonFiber()
{
    Material m;
    m.name = "Carbon Fiber";
    m.baseR = 0.12f; m.baseG = 0.12f; m.baseB = 0.14f;
    m.opacity = 1.0f;
    m.metallic = 0.3f;
    m.roughness = 0.45f;
    m.density = 0.0016;
    return m;
}

const std::vector<Material>& MaterialLibrary::all()
{
    static std::vector<Material> lib = {
        steel(), aluminum(), brass(), copper(), plastic(),
        wood(), glass(), rubber(), gold(), titanium(), carbonFiber()
    };
    return lib;
}

const Material* MaterialLibrary::byName(const std::string& name)
{
    const auto& lib = all();
    for (const auto& m : lib) {
        if (m.name == name)
            return &m;
    }
    return nullptr;
}

// =============================================================================
// AppearanceManager
// =============================================================================

void AppearanceManager::setBodyMaterial(const std::string& bodyId, const Material& mat)
{
    m_bodyMaterials[bodyId] = mat;
}

void AppearanceManager::setFaceMaterial(const std::string& bodyId, int faceIndex, const Material& mat)
{
    m_faceMaterials[bodyId][faceIndex] = mat;
}

const Material& AppearanceManager::bodyMaterial(const std::string& bodyId) const
{
    auto it = m_bodyMaterials.find(bodyId);
    if (it != m_bodyMaterials.end())
        return it->second;
    return m_defaultMaterial;
}

const Material& AppearanceManager::faceMaterial(const std::string& bodyId, int faceIndex) const
{
    auto faceIt = m_faceMaterials.find(bodyId);
    if (faceIt != m_faceMaterials.end()) {
        auto it = faceIt->second.find(faceIndex);
        if (it != faceIt->second.end())
            return it->second;
    }
    return bodyMaterial(bodyId);
}

bool AppearanceManager::hasFaceOverride(const std::string& bodyId, int faceIndex) const
{
    auto faceIt = m_faceMaterials.find(bodyId);
    if (faceIt != m_faceMaterials.end())
        return faceIt->second.find(faceIndex) != faceIt->second.end();
    return false;
}

void AppearanceManager::clearFaceOverride(const std::string& bodyId, int faceIndex)
{
    auto faceIt = m_faceMaterials.find(bodyId);
    if (faceIt != m_faceMaterials.end()) {
        faceIt->second.erase(faceIndex);
        if (faceIt->second.empty())
            m_faceMaterials.erase(faceIt);
    }
}

void AppearanceManager::clearBody(const std::string& bodyId)
{
    m_bodyMaterials.erase(bodyId);
    m_faceMaterials.erase(bodyId);
}

void AppearanceManager::clear()
{
    m_bodyMaterials.clear();
    m_faceMaterials.clear();
}

} // namespace kernel
