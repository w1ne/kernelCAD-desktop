#include "BRepModel.h"
#include <TopoDS_Shape.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <GCPnts_TangentialDeflection.hxx>
#include <TopoDS_Edge.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <Poly_Triangulation.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <cmath>
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

    // Tessellate the shape
    const TopoDS_Shape& shape = getShape(id);

    BRepMesh_IncrementalMesh mesher(shape, deflection);
    mesher.Perform();

    OCCTKernel::Mesh result;
    TopExp_Explorer faceEx(shape, TopAbs_FACE);
    for (; faceEx.More(); faceEx.Next()) {
        const TopoDS_Face& face = TopoDS::Face(faceEx.Current());
        TopLoc_Location loc;
        auto tri = BRep_Tool::Triangulation(face, loc);
        if (tri.IsNull()) continue;

        const bool reversed = (face.Orientation() == TopAbs_REVERSED);
        const uint32_t offset = static_cast<uint32_t>(result.vertices.size() / 3);

        const gp_Trsf trsf = loc.Transformation();
        const bool hasNormals = tri->HasNormals();

        for (int i = 1; i <= tri->NbNodes(); ++i) {
            gp_Pnt p = tri->Node(i).Transformed(trsf);
            result.vertices.push_back(static_cast<float>(p.X()));
            result.vertices.push_back(static_cast<float>(p.Y()));
            result.vertices.push_back(static_cast<float>(p.Z()));

            if (hasNormals) {
                gp_Dir n = tri->Normal(i);
                n = n.Transformed(trsf);
                if (reversed) n.Reverse();
                result.normals.push_back(static_cast<float>(n.X()));
                result.normals.push_back(static_cast<float>(n.Y()));
                result.normals.push_back(static_cast<float>(n.Z()));
            } else {
                // Will be filled by face-normal accumulation below
                result.normals.push_back(0);
                result.normals.push_back(0);
                result.normals.push_back(0);
            }
        }

        for (int i = 1; i <= tri->NbTriangles(); ++i) {
            int n1, n2, n3;
            tri->Triangle(i).Get(n1, n2, n3);
            if (reversed) std::swap(n2, n3);
            result.indices.push_back(offset + n1 - 1);
            result.indices.push_back(offset + n2 - 1);
            result.indices.push_back(offset + n3 - 1);

            if (!hasNormals) {
                // Accumulate area-weighted face normals for smooth shading
                uint32_t i0 = offset + n1 - 1;
                uint32_t i1 = offset + n2 - 1;
                uint32_t i2 = offset + n3 - 1;
                gp_Vec v0(result.vertices[i0*3], result.vertices[i0*3+1], result.vertices[i0*3+2]);
                gp_Vec v1(result.vertices[i1*3], result.vertices[i1*3+1], result.vertices[i1*3+2]);
                gp_Vec v2(result.vertices[i2*3], result.vertices[i2*3+1], result.vertices[i2*3+2]);
                gp_Vec faceN = (v1 - v0).Crossed(v2 - v0);
                for (uint32_t vi : {i0, i1, i2}) {
                    result.normals[vi*3]   += static_cast<float>(faceN.X());
                    result.normals[vi*3+1] += static_cast<float>(faceN.Y());
                    result.normals[vi*3+2] += static_cast<float>(faceN.Z());
                }
            }
        }
    }

    // Normalize any accumulated face normals (fallback path)
    for (size_t i = 0; i + 2 < result.normals.size(); i += 3) {
        float nx = result.normals[i], ny = result.normals[i+1], nz = result.normals[i+2];
        float len = std::sqrt(nx*nx + ny*ny + nz*nz);
        if (len > 1e-10f) {
            result.normals[i]   = nx / len;
            result.normals[i+1] = ny / len;
            result.normals[i+2] = nz / len;
        } else {
            result.normals[i] = 0; result.normals[i+1] = 0; result.normals[i+2] = 1;
        }
    }

    // Cache result
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

    const TopoDS_Shape& shape = getShape(id);

    // Ensure the shape is tessellated (needed for edge polylines too)
    BRepMesh_IncrementalMesh mesher(shape, deflection);
    mesher.Perform();

    OCCTKernel::EdgeMesh result;

    TopExp_Explorer edgeEx(shape, TopAbs_EDGE);
    for (; edgeEx.More(); edgeEx.Next()) {
        const TopoDS_Edge& edge = TopoDS::Edge(edgeEx.Current());

        if (BRep_Tool::Degenerated(edge))
            continue;

        BRepAdaptor_Curve curve(edge);
        GCPnts_TangentialDeflection discretizer(curve, deflection, 0.1);

        int nbPoints = discretizer.NbPoints();
        if (nbPoints < 2)
            continue;

        uint32_t baseIndex = static_cast<uint32_t>(result.vertices.size() / 3);

        for (int i = 1; i <= nbPoints; ++i) {
            gp_Pnt p = discretizer.Value(i);
            result.vertices.push_back(static_cast<float>(p.X()));
            result.vertices.push_back(static_cast<float>(p.Y()));
            result.vertices.push_back(static_cast<float>(p.Z()));
        }

        for (int i = 0; i < nbPoints - 1; ++i) {
            result.indices.push_back(baseIndex + static_cast<uint32_t>(i));
            result.indices.push_back(baseIndex + static_cast<uint32_t>(i + 1));
        }
    }

    // Cache result
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
