#include "EntityAttribute.h"
#include "StableReference.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace kernel {

// ── Face attributes ──────────────────────────────────────────────────────────

void AttributeMap::setFaceAttribute(int faceIndex, const EntityAttribute& attr)
{
    auto& vec = m_faceAttrs[faceIndex];
    // Avoid duplicates
    for (const auto& existing : vec) {
        if (existing == attr)
            return;
    }
    vec.push_back(attr);
}

void AttributeMap::setEdgeAttribute(int edgeIndex, const EntityAttribute& attr)
{
    auto& vec = m_edgeAttrs[edgeIndex];
    for (const auto& existing : vec) {
        if (existing == attr)
            return;
    }
    vec.push_back(attr);
}

std::vector<EntityAttribute> AttributeMap::faceAttributes(int faceIndex) const
{
    auto it = m_faceAttrs.find(faceIndex);
    if (it != m_faceAttrs.end())
        return it->second;
    return {};
}

std::vector<EntityAttribute> AttributeMap::edgeAttributes(int edgeIndex) const
{
    auto it = m_edgeAttrs.find(edgeIndex);
    if (it != m_edgeAttrs.end())
        return it->second;
    return {};
}

// ── Find by attribute ────────────────────────────────────────────────────────

std::vector<int> AttributeMap::findFaces(const std::string& group,
                                          const std::string& name,
                                          const std::string& value) const
{
    std::vector<int> result;
    for (const auto& [idx, attrs] : m_faceAttrs) {
        for (const auto& attr : attrs) {
            if (attr.groupName == group && attr.name == name) {
                if (value.empty() || attr.value == value) {
                    result.push_back(idx);
                    break; // one match per face index is enough
                }
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<int> AttributeMap::findEdges(const std::string& group,
                                          const std::string& name,
                                          const std::string& value) const
{
    std::vector<int> result;
    for (const auto& [idx, attrs] : m_edgeAttrs) {
        for (const auto& attr : attrs) {
            if (attr.groupName == group && attr.name == name) {
                if (value.empty() || attr.value == value) {
                    result.push_back(idx);
                    break;
                }
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

// ── Propagation ──────────────────────────────────────────────────────────────

void AttributeMap::propagate(const AttributeMap& oldMap, int oldFaceCount,
                              AttributeMap& newMap, int newFaceCount,
                              const std::vector<float>& oldCentroids,
                              const std::vector<float>& newCentroids,
                              float tolerance)
{
    if (oldFaceCount == 0 || newFaceCount == 0)
        return;

    const float tolSq = tolerance * tolerance;

    // For each old face that has attributes, find matching new faces by centroid proximity
    for (const auto& [oldIdx, attrs] : oldMap.m_faceAttrs) {
        if (oldIdx < 0 || oldIdx >= oldFaceCount)
            continue;

        float ox = oldCentroids[oldIdx * 3];
        float oy = oldCentroids[oldIdx * 3 + 1];
        float oz = oldCentroids[oldIdx * 3 + 2];

        // Search all new faces for matches within tolerance.
        // Collect all matches to detect splits (one old → multiple new).
        std::vector<int> matchedNewFaces;
        for (int newIdx = 0; newIdx < newFaceCount; ++newIdx) {
            float nx = newCentroids[newIdx * 3];
            float ny = newCentroids[newIdx * 3 + 1];
            float nz = newCentroids[newIdx * 3 + 2];

            float dx = nx - ox, dy = ny - oy, dz = nz - oz;
            if (dx*dx + dy*dy + dz*dz <= tolSq)
                matchedNewFaces.push_back(newIdx);
        }

        // Transfer attributes. If multiple matches, it's a face split —
        // assign incrementing splitIndex (otherParents mechanism).
        for (size_t mi = 0; mi < matchedNewFaces.size(); ++mi) {
            int newIdx = matchedNewFaces[mi];
            for (const auto& attr : attrs) {
                EntityAttribute copy = attr;
                copy.splitIndex = (matchedNewFaces.size() > 1)
                    ? static_cast<int>(mi) : attr.splitIndex;
                newMap.setFaceAttribute(newIdx, copy);
            }
        }
    }

    // Similarly propagate edge attributes (if any)
    // Edge propagation is more complex in practice; for now we skip it
    // since edges require a different matching strategy (midpoint + tangent).
}

// ── Housekeeping ─────────────────────────────────────────────────────────────

void AttributeMap::clear()
{
    m_faceAttrs.clear();
    m_edgeAttrs.clear();
}

void AttributeMap::merge(const AttributeMap& other)
{
    for (const auto& [idx, attrs] : other.m_faceAttrs) {
        for (const auto& attr : attrs) {
            setFaceAttribute(idx, attr);
        }
    }
    for (const auto& [idx, attrs] : other.m_edgeAttrs) {
        for (const auto& attr : attrs) {
            setEdgeAttribute(idx, attr);
        }
    }
}

bool AttributeMap::empty() const
{
    return m_faceAttrs.empty() && m_edgeAttrs.empty();
}

std::vector<int> AttributeMap::attributedFaceIndices() const
{
    std::vector<int> result;
    result.reserve(m_faceAttrs.size());
    for (const auto& [idx, _] : m_faceAttrs)
        result.push_back(idx);
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<int> AttributeMap::attributedEdgeIndices() const
{
    std::vector<int> result;
    result.reserve(m_edgeAttrs.size());
    for (const auto& [idx, _] : m_edgeAttrs)
        result.push_back(idx);
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace kernel
