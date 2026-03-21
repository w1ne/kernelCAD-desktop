#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace kernel {

/// A persistent tag attached to a B-Rep entity (face, edge, vertex).
struct EntityAttribute {
    std::string groupName;    // namespace (feature ID that created it)
    std::string name;         // tag name within the group (e.g., "SideFace", "TopEdge")
    std::string value;        // tag value (e.g., stable entity ID or index hint)
    int splitIndex = 0;       // 0 = original, 1+ = split children (otherParents mechanism)

    bool operator==(const EntityAttribute& o) const {
        return groupName == o.groupName && name == o.name && value == o.value
            && splitIndex == o.splitIndex;
    }
};

/// Manages attributes on the B-Rep entities of a shape.
/// Uses OCCT's internal face/edge indexing (TopExp_Explorer order) to map attributes.
class AttributeMap {
public:
    /// Tag a face by its index within the shape.
    void setFaceAttribute(int faceIndex, const EntityAttribute& attr);

    /// Tag an edge by its index.
    void setEdgeAttribute(int edgeIndex, const EntityAttribute& attr);

    /// Get all attributes on a face.
    std::vector<EntityAttribute> faceAttributes(int faceIndex) const;

    /// Get all attributes on an edge.
    std::vector<EntityAttribute> edgeAttributes(int edgeIndex) const;

    /// Find face indices that have a specific attribute (group + name + optional value).
    /// If value is empty, matches any value for the given group+name.
    std::vector<int> findFaces(const std::string& group, const std::string& name,
                                const std::string& value = "") const;

    /// Find edge indices matching an attribute.
    std::vector<int> findEdges(const std::string& group, const std::string& name,
                                const std::string& value = "") const;

    /// Propagate attributes from old shape to new shape after a boolean/feature operation.
    /// Uses geometric proximity: for each old attributed face, find the closest
    /// face in the new shape and transfer the attributes.
    ///
    /// centroids are packed float arrays: [x0,y0,z0, x1,y1,z1, ...].
    /// oldFaceCount / newFaceCount give the number of faces (centroids.size()/3).
    /// tolerance: maximum distance between centroids to consider a match.
    static void propagate(const AttributeMap& oldMap, int oldFaceCount,
                          AttributeMap& newMap, int newFaceCount,
                          const std::vector<float>& oldCentroids,
                          const std::vector<float>& newCentroids,
                          float tolerance = 1.0f);

    void clear();

    /// Merge attributes from another map (for combine operations).
    /// Attributes from other are appended; face/edge indices must already be
    /// consistent with the target shape's indexing.
    void merge(const AttributeMap& other);

    /// Return true if no attributes are stored.
    bool empty() const;

    /// Return all face indices that have at least one attribute.
    std::vector<int> attributedFaceIndices() const;

    /// Return all edge indices that have at least one attribute.
    std::vector<int> attributedEdgeIndices() const;

private:
    std::unordered_map<int, std::vector<EntityAttribute>> m_faceAttrs;
    std::unordered_map<int, std::vector<EntityAttribute>> m_edgeAttrs;
};

} // namespace kernel
