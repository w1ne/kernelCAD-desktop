#include "JointCreator.h"
#include "../kernel/BRepModel.h"
#include "../kernel/BRepQuery.h"
#include <cmath>

JointCreator::JointCreator(QObject* parent)
    : QObject(parent)
{}

void JointCreator::begin(features::JointType type)
{
    m_jointType = type;
    m_state = State::PickingFace1;
    m_body1Id.clear();
    m_face1Index = -1;
    emit stateChanged(m_state);
}

void JointCreator::cancel()
{
    m_state = State::Idle;
    m_body1Id.clear();
    m_face1Index = -1;
    emit stateChanged(m_state);
    emit cancelled();
}

JointCreator::State JointCreator::state() const
{
    return m_state;
}

void JointCreator::setBRepModel(kernel::BRepModel* brep)
{
    m_brep = brep;
}

void JointCreator::onFaceSelected(const SelectionHit& hit)
{
    if (m_state == State::Idle)
        return;

    if (hit.bodyId.empty() || hit.faceIndex < 0)
        return;

    if (m_state == State::PickingFace1) {
        // Save first face info
        m_body1Id = hit.bodyId;
        m_face1Index = hit.faceIndex;

        if (m_brep && m_brep->hasBody(hit.bodyId)) {
            auto geom = geometryFromFace(hit.bodyId, hit.faceIndex);
            m_face1NormalX = geom.primaryAxisX;
            m_face1NormalY = geom.primaryAxisY;
            m_face1NormalZ = geom.primaryAxisZ;
            m_face1CenterX = geom.originX;
            m_face1CenterY = geom.originY;
            m_face1CenterZ = geom.originZ;
        }

        m_state = State::PickingFace2;
        emit stateChanged(m_state);
        return;
    }

    if (m_state == State::PickingFace2) {
        // Build JointParams from the two faces
        features::JointParams params;
        params.jointType = m_jointType;

        // Geometry for face 1
        params.geometryOne.originX = m_face1CenterX;
        params.geometryOne.originY = m_face1CenterY;
        params.geometryOne.originZ = m_face1CenterZ;
        params.geometryOne.primaryAxisX = m_face1NormalX;
        params.geometryOne.primaryAxisY = m_face1NormalY;
        params.geometryOne.primaryAxisZ = m_face1NormalZ;

        // Geometry for face 2
        if (m_brep && m_brep->hasBody(hit.bodyId)) {
            auto geom2 = geometryFromFace(hit.bodyId, hit.faceIndex);
            params.geometryTwo = geom2;
        }

        // Store body IDs as occurrence references -- the caller will resolve
        // these to actual occurrence IDs based on the component registry.
        params.occurrenceOneId = m_body1Id;
        params.occurrenceTwoId = hit.bodyId;

        m_state = State::Idle;
        emit stateChanged(m_state);
        emit jointReady(params);
    }
}

features::JointGeometry JointCreator::geometryFromFace(
    const std::string& bodyId, int faceIndex) const
{
    features::JointGeometry geom;

    if (!m_brep || !m_brep->hasBody(bodyId))
        return geom;

    kernel::BRepQuery query(m_brep->getShape(bodyId));

    if (faceIndex >= 0 && faceIndex < query.faceCount()) {
        kernel::FaceInfo fi = query.faceInfo(faceIndex);
        geom.originX = fi.centroidX;
        geom.originY = fi.centroidY;
        geom.originZ = fi.centroidZ;
        geom.primaryAxisX = fi.normalX;
        geom.primaryAxisY = fi.normalY;
        geom.primaryAxisZ = fi.normalZ;

        // Compute a secondary axis perpendicular to the normal.
        // Use cross product with a non-parallel reference vector.
        double nx = fi.normalX, ny = fi.normalY, nz = fi.normalZ;
        double refX = 1.0, refY = 0.0, refZ = 0.0;
        // If normal is nearly parallel to X, use Y instead
        if (std::abs(nx) > 0.9) {
            refX = 0.0;
            refY = 1.0;
        }
        // secondary = normal x ref
        double sx = ny * refZ - nz * refY;
        double sy = nz * refX - nx * refZ;
        double sz = nx * refY - ny * refX;
        double len = std::sqrt(sx * sx + sy * sy + sz * sz);
        if (len > 1e-15) {
            sx /= len; sy /= len; sz /= len;
        }
        geom.secondaryAxisX = sx;
        geom.secondaryAxisY = sy;
        geom.secondaryAxisZ = sz;
    }

    return geom;
}
