#include "BRepModel.h"
#include <TopoDS_Shape.hxx>
#include <stdexcept>

namespace kernel {

struct BRepModel::Impl {
    std::unordered_map<std::string, TopoDS_Shape>      shapes;
    std::unordered_map<std::string, OCCTKernel::Mesh>  meshCache;
    std::unordered_map<std::string, OCCTKernel::EdgeMesh> edgeMeshCache;
    // deflection used for cached meshes, per body
    std::unordered_map<std::string, double>             meshDeflection;
    std::unordered_map<std::string, double>             edgeMeshDeflection;
    // Per-body attribute maps for stable entity naming
    std::unordered_map<std::string, AttributeMap>       attributeMaps;
    // Per-body visibility (default: true)
    std::unordered_map<std::string, bool>               visibility;
};

BRepModel::BRepModel() : m_impl(std::make_unique<Impl>()) {}
BRepModel::~BRepModel() = default;

void BRepModel::addBody(const std::string& id, const TopoDS_Shape& shape)
{
    m_impl->shapes[id] = shape;
    // Invalidate any cached mesh for this body
    m_impl->meshCache.erase(id);
    m_impl->meshDeflection.erase(id);
    m_impl->edgeMeshCache.erase(id);
    m_impl->edgeMeshDeflection.erase(id);
}

void BRepModel::removeBody(const std::string& id)
{
    m_impl->shapes.erase(id);
    m_impl->meshCache.erase(id);
    m_impl->meshDeflection.erase(id);
    m_impl->edgeMeshCache.erase(id);
    m_impl->edgeMeshDeflection.erase(id);
    m_impl->attributeMaps.erase(id);
    m_impl->visibility.erase(id);
}

void BRepModel::clear()
{
    m_impl->shapes.clear();
    m_impl->meshCache.clear();
    m_impl->meshDeflection.clear();
    m_impl->edgeMeshCache.clear();
    m_impl->edgeMeshDeflection.clear();
    m_impl->attributeMaps.clear();
    m_impl->visibility.clear();
}

bool BRepModel::hasBody(const std::string& id) const
{
    return m_impl->shapes.count(id) > 0;
}

const TopoDS_Shape& BRepModel::getShape(const std::string& id) const
{
    auto it = m_impl->shapes.find(id);
    if (it == m_impl->shapes.end())
        throw std::runtime_error("BRepModel::getShape: unknown body id '" + id + "'");
    return it->second;
}

OCCTKernel::Mesh BRepModel::getMesh(const std::string& id, double deflection)
{
    // Return cached mesh if deflection matches
    auto cacheIt = m_impl->meshCache.find(id);
    auto deflIt  = m_impl->meshDeflection.find(id);
    if (cacheIt != m_impl->meshCache.end() &&
        deflIt != m_impl->meshDeflection.end() &&
        deflIt->second == deflection)
    {
        return cacheIt->second;
    }

    OCCTKernel kernel;
    OCCTKernel::Mesh result = kernel.tessellate(getShape(id), deflection);

    m_impl->meshCache[id] = result;
    m_impl->meshDeflection[id] = deflection;
    return result;
}

OCCTKernel::EdgeMesh BRepModel::getEdgeMesh(const std::string& id, double deflection)
{
    // Return cached edge mesh if deflection matches
    auto cacheIt = m_impl->edgeMeshCache.find(id);
    auto deflIt  = m_impl->edgeMeshDeflection.find(id);
    if (cacheIt != m_impl->edgeMeshCache.end() &&
        deflIt != m_impl->edgeMeshDeflection.end() &&
        deflIt->second == deflection)
    {
        return cacheIt->second;
    }

    // Edge polylines do not need a second IncrementalMesh. getMesh() already
    // tessellates when the viewport asks for both, and extractEdges samples curves directly.
    OCCTKernel kernel;
    OCCTKernel::EdgeMesh result = kernel.extractEdges(getShape(id), deflection);

    m_impl->edgeMeshCache[id] = result;
    m_impl->edgeMeshDeflection[id] = deflection;
    return result;
}

std::vector<std::string> BRepModel::bodyIds() const
{
    std::vector<std::string> ids;
    ids.reserve(m_impl->shapes.size());
    for (const auto& [id, _] : m_impl->shapes)
        ids.push_back(id);
    return ids;
}

AttributeMap& BRepModel::attributes(const std::string& bodyId)
{
    return m_impl->attributeMaps[bodyId];
}

const AttributeMap& BRepModel::attributes(const std::string& bodyId) const
{
    static const AttributeMap empty;
    auto it = m_impl->attributeMaps.find(bodyId);
    if (it != m_impl->attributeMaps.end())
        return it->second;
    return empty;
}

BRepQuery BRepModel::query(const std::string& bodyId) const
{
    return BRepQuery(getShape(bodyId));
}

OCCTKernel::PhysicalProperties BRepModel::getProperties(const std::string& bodyId,
                                                         double density)
{
    const TopoDS_Shape& shape = getShape(bodyId);
    OCCTKernel kernel;
    return kernel.computeProperties(shape, density);
}

bool BRepModel::isBodyVisible(const std::string& id) const
{
    auto it = m_impl->visibility.find(id);
    if (it != m_impl->visibility.end())
        return it->second;
    return true;  // default visible
}

void BRepModel::setBodyVisible(const std::string& id, bool visible)
{
    m_impl->visibility[id] = visible;
}

} // namespace kernel
