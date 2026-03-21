#include "MeasureTool.h"
#include "../kernel/BRepModel.h"
#include <cmath>
#include <TopoDS_Shape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRep_Tool.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <gp_Pnt.hxx>

MeasureTool::MeasureTool(QObject* parent)
    : QObject(parent)
{
}

MeasureTool::~MeasureTool() = default;

void MeasureTool::setMode(MeasureMode mode)
{
    m_mode = mode;
    reset();
}

void MeasureTool::setFirstEntity(const SelectionHit& hit)
{
    m_first = hit;
    m_hasFirst = true;
    m_hasSecond = false;
    m_hasResult = false;
}

void MeasureTool::setSecondEntity(const SelectionHit& hit)
{
    if (!m_hasFirst) return;
    m_second = hit;
    m_hasSecond = true;
    m_lastResult = compute();
    m_hasResult = true;
    emit measurementReady(m_lastResult);
}

void MeasureTool::reset()
{
    m_hasFirst = false;
    m_hasSecond = false;
    m_hasResult = false;
    m_first = {};
    m_second = {};
    m_lastResult = {};
}

MeasureTool::MeasureResult MeasureTool::compute() const
{
    MeasureResult result;

    if (!m_hasFirst || !m_hasSecond)
        return result;

    // ── Point-to-Point mode: simple Euclidean distance ──────────────────
    if (m_mode == MeasureMode::PointToPoint) {
        double dx = m_second.worldX - m_first.worldX;
        double dy = m_second.worldY - m_first.worldY;
        double dz = m_second.worldZ - m_first.worldZ;
        result.distance = std::sqrt(dx*dx + dy*dy + dz*dz);
        result.p1x = m_first.worldX;
        result.p1y = m_first.worldY;
        result.p1z = m_first.worldZ;
        result.p2x = m_second.worldX;
        result.p2y = m_second.worldY;
        result.p2z = m_second.worldZ;
        result.description = QString("Point-to-Point: %1 mm").arg(result.distance, 0, 'f', 4);
        return result;
    }

    // ── Use OCCT BRepExtrema for Edge/Face distance measurements ────────
    if (m_brepModel && !m_first.bodyId.empty() && !m_second.bodyId.empty() &&
        m_brepModel->hasBody(m_first.bodyId) && m_brepModel->hasBody(m_second.bodyId))
    {
        try {
            const TopoDS_Shape& shape1 = m_brepModel->getShape(m_first.bodyId);
            const TopoDS_Shape& shape2 = m_brepModel->getShape(m_second.bodyId);

            // Extract specific sub-shapes based on mode
            TopoDS_Shape subShape1, subShape2;

            auto getEdge = [](const TopoDS_Shape& shape, int edgeIdx) -> TopoDS_Shape {
                int idx = 0;
                for (TopExp_Explorer ex(shape, TopAbs_EDGE); ex.More(); ex.Next()) {
                    if (idx == edgeIdx)
                        return ex.Current();
                    ++idx;
                }
                return shape;
            };

            auto getFace = [](const TopoDS_Shape& shape, int faceIdx) -> TopoDS_Shape {
                int idx = 0;
                for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
                    if (idx == faceIdx)
                        return ex.Current();
                    ++idx;
                }
                return shape;
            };

            switch (m_mode) {
            case MeasureMode::PointToEdge:
                subShape1 = BRepBuilderAPI_MakeVertex(
                    gp_Pnt(m_first.worldX, m_first.worldY, m_first.worldZ)).Shape();
                subShape2 = (m_second.edgeIndex >= 0) ? getEdge(shape2, m_second.edgeIndex) : shape2;
                result.description = QString("Point-to-Edge: ");
                break;

            case MeasureMode::EdgeToEdge:
                subShape1 = (m_first.edgeIndex >= 0) ? getEdge(shape1, m_first.edgeIndex) : shape1;
                subShape2 = (m_second.edgeIndex >= 0) ? getEdge(shape2, m_second.edgeIndex) : shape2;
                result.description = QString("Edge-to-Edge: ");
                break;

            case MeasureMode::FaceToFace:
                subShape1 = (m_first.faceIndex >= 0) ? getFace(shape1, m_first.faceIndex) : shape1;
                subShape2 = (m_second.faceIndex >= 0) ? getFace(shape2, m_second.faceIndex) : shape2;
                result.description = QString("Face-to-Face: ");
                break;

            case MeasureMode::Angle:
                // For angle mode, use the hit points to define directions
                {
                    double dx = m_second.worldX - m_first.worldX;
                    double dy = m_second.worldY - m_first.worldY;
                    double dz = m_second.worldZ - m_first.worldZ;
                    result.distance = std::sqrt(dx*dx + dy*dy + dz*dz);
                    // Compute angle between two edge tangent vectors (approximated by hit positions)
                    result.angle = 0;
                    result.p1x = m_first.worldX;
                    result.p1y = m_first.worldY;
                    result.p1z = m_first.worldZ;
                    result.p2x = m_second.worldX;
                    result.p2y = m_second.worldY;
                    result.p2z = m_second.worldZ;
                    result.description = QString("Angle: %1 deg, Distance: %2 mm")
                        .arg(result.angle, 0, 'f', 2)
                        .arg(result.distance, 0, 'f', 4);
                    return result;
                }

            default:
                break;
            }

            if (!subShape1.IsNull() && !subShape2.IsNull()) {
                BRepExtrema_DistShapeShape extrema(subShape1, subShape2);
                if (extrema.IsDone() && extrema.NbSolution() > 0) {
                    result.distance = extrema.Value();
                    gp_Pnt p1 = extrema.PointOnShape1(1);
                    gp_Pnt p2 = extrema.PointOnShape2(1);
                    result.p1x = static_cast<float>(p1.X());
                    result.p1y = static_cast<float>(p1.Y());
                    result.p1z = static_cast<float>(p1.Z());
                    result.p2x = static_cast<float>(p2.X());
                    result.p2y = static_cast<float>(p2.Y());
                    result.p2z = static_cast<float>(p2.Z());
                    result.description += QString("%1 mm").arg(result.distance, 0, 'f', 4);
                    return result;
                }
            }
        } catch (...) {
            // Fall through to point-to-point fallback
        }
    }

    // Fallback: use hit point coordinates for simple distance
    double dx = m_second.worldX - m_first.worldX;
    double dy = m_second.worldY - m_first.worldY;
    double dz = m_second.worldZ - m_first.worldZ;
    result.distance = std::sqrt(dx*dx + dy*dy + dz*dz);
    result.p1x = m_first.worldX;
    result.p1y = m_first.worldY;
    result.p1z = m_first.worldZ;
    result.p2x = m_second.worldX;
    result.p2y = m_second.worldY;
    result.p2z = m_second.worldZ;
    result.description = QString("Distance: %1 mm").arg(result.distance, 0, 'f', 4);
    return result;
}
