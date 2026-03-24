#include "ViewportOverlays.h"
#include <QPainter>
#include <QPolygonF>
#include <QColor>
#include <QFont>
#include <algorithm>

namespace ViewportOverlays {

// ── Shared cube geometry ────────────────────────────────────────────────────

struct CubeFace {
    int v[4];
    const char* label;
    QVector3D direction;
    QVector3D up;
};

static const CubeFace kFaces[6] = {
    // Front face: -Y direction  (camera looks along -Y to see front)
    {{4, 5, 6, 7}, "Front",  { 0,  1, 0}, {0, 0, 1}},
    // Back face: +Y direction
    {{1, 0, 3, 2}, "Back",   { 0, -1, 0}, {0, 0, 1}},
    // Right face: -X direction
    {{5, 1, 2, 6}, "Right",  {-1,  0, 0}, {0, 0, 1}},
    // Left face: +X direction
    {{0, 4, 7, 3}, "Left",   { 1,  0, 0}, {0, 0, 1}},
    // Top face: -Z direction (camera looks down)
    {{7, 6, 2, 3}, "Top",    { 0,  0, 1}, {0, 1, 0}},
    // Bottom face: +Z direction
    {{0, 1, 5, 4}, "Bottom", { 0,  0,-1}, {0,-1, 0}},
};

struct FaceOrder { int idx; float avgZ; };

/// Build the projected 2D corners and depths from viewRot and cube parameters.
static void projectCorners(const QMatrix4x4& viewRot, int cubeSize,
                           float cx, float cy,
                           QPointF p[8], float pz[8])
{
    const float hs = cubeSize * 0.35f;
    const QVector3D corners[8] = {
        {-hs, -hs, -hs}, { hs, -hs, -hs}, { hs,  hs, -hs}, {-hs,  hs, -hs},
        {-hs, -hs,  hs}, { hs, -hs,  hs}, { hs,  hs,  hs}, {-hs,  hs,  hs}
    };
    for (int i = 0; i < 8; ++i) {
        QVector3D r = viewRot.map(corners[i]);
        p[i]  = QPointF(cx + r.x(), cy - r.y());
        pz[i] = r.z();
    }
}

// ── drawViewCube ────────────────────────────────────────────────────────────

void drawViewCube(QPainter& painter,
                  const QMatrix4x4& viewRot,
                  int widgetWidth, int /*widgetHeight*/,
                  int cubeSize, int margin,
                  int hoveredFace,
                  bool isPerspective)
{
    const float cx = widgetWidth - margin - cubeSize * 0.5f;
    const float cy = margin + cubeSize * 0.5f;

    QPointF p[8];
    float   pz[8];
    projectCorners(viewRot, cubeSize, cx, cy, p, pz);

    // Compute average Z for each face (for painter's algorithm)
    FaceOrder order[6];
    for (int i = 0; i < 6; ++i) {
        float z = 0;
        for (int j = 0; j < 4; ++j)
            z += pz[kFaces[i].v[j]];
        order[i] = {i, z * 0.25f};
    }
    // Sort back-to-front (most negative Z first = farthest)
    std::sort(std::begin(order), std::end(order),
              [](const FaceOrder& a, const FaceOrder& b) { return a.avgZ < b.avgZ; });

    painter.setRenderHint(QPainter::Antialiasing);

    // Drop shadow behind the cube
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 40));
    painter.drawEllipse(QPointF(cx + 2, cy + 2), cubeSize * 0.54, cubeSize * 0.54);

    // Semi-transparent background circle behind the cube
    painter.setBrush(QColor(40, 40, 40, 120));
    painter.drawEllipse(QPointF(cx, cy), cubeSize * 0.52, cubeSize * 0.52);

    // Draw faces back-to-front
    for (int fi = 0; fi < 6; ++fi) {
        const CubeFace& face = kFaces[order[fi].idx];

        QPolygonF poly;
        for (int j = 0; j < 4; ++j)
            poly << p[face.v[j]];

        // Face fill: slightly transparent gray, brighter for front-facing
        bool isHovered = (order[fi].idx == hoveredFace && order[fi].avgZ > 0);
        int alpha = (order[fi].avgZ > 0) ? 180 : 100;
        if (isHovered) {
            painter.setBrush(QColor(0, 120, 212, 200));
            painter.setPen(QPen(QColor(220, 220, 220, 230), 1.0));
        } else {
            painter.setBrush(QColor(70, 75, 80, alpha));
            painter.setPen(QPen(QColor(180, 180, 180, 200), 1.0));
        }
        painter.drawPolygon(poly);

        // Draw label only on front-facing faces (positive average Z = closer)
        if (order[fi].avgZ > 0) {
            QPointF center(0, 0);
            for (int j = 0; j < 4; ++j)
                center += p[face.v[j]];
            center /= 4.0;

            painter.setPen(isHovered ? QColor(255, 255, 255) : QColor(220, 220, 220));
            QFont f = painter.font();
            f.setPixelSize(isHovered ? 11 : 10);
            f.setBold(true);
            painter.setFont(f);
            painter.drawText(QRectF(center.x() - 30, center.y() - 8, 60, 16),
                             Qt::AlignCenter, face.label);
        }
    }

    // Draw XYZ coordinate axes below the cube
    {
        const float axisLen = cubeSize * 0.3f;
        // Axis origin: below-left of the cube center
        const float aox = cx - cubeSize * 0.42f;
        const float aoy = cy + cubeSize * 0.42f;

        struct AxisDef { QVector3D dir; QColor color; const char* label; };
        AxisDef axes[3] = {
            {{1, 0, 0}, QColor(220, 60, 60),  "X"},
            {{0, 1, 0}, QColor(60, 180, 60),  "Y"},
            {{0, 0, 1}, QColor(70, 120, 220), "Z"},
        };

        QFont af = painter.font();
        af.setPixelSize(9);
        af.setBold(true);
        painter.setFont(af);

        for (const auto& ax : axes) {
            QVector3D projected = viewRot.map(ax.dir * axisLen);
            float ex = aox + projected.x();
            float ey = aoy - projected.y();

            painter.setPen(QPen(ax.color, 1.8, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(QPointF(aox, aoy), QPointF(ex, ey));

            // Label at the tip
            painter.setPen(ax.color);
            painter.drawText(QRectF(ex - 6, ey - 6, 12, 12), Qt::AlignCenter, ax.label);
        }
    }

    // Draw projection mode indicator
    {
        painter.setPen(QColor(140, 140, 140));
        QFont f = painter.font();
        f.setPixelSize(9);
        f.setBold(false);
        painter.setFont(f);
        const char* projLabel = isPerspective ? "Persp" : "Ortho";
        painter.drawText(QRectF(cx - 25, cy + cubeSize * 0.5f + 2, 50, 14),
                         Qt::AlignCenter, projLabel);
    }
}

// ── handleViewCubeClick ─────────────────────────────────────────────────────

int handleViewCubeClick(const QPoint& pos,
                        const QMatrix4x4& viewRot,
                        int widgetWidth, int /*widgetHeight*/,
                        int cubeSize, int margin,
                        QVector3D& outDirection,
                        QVector3D& outUp)
{
    const float cx = widgetWidth - margin - cubeSize * 0.5f;
    const float cy = margin + cubeSize * 0.5f;
    const float dx = pos.x() - cx;
    const float dy = pos.y() - cy;
    const float radius = cubeSize * 0.55f;
    if (dx * dx + dy * dy > radius * radius)
        return -1;

    QPointF p[8];
    float   pz[8];
    projectCorners(viewRot, cubeSize, cx, cy, p, pz);

    // Sort front-to-back (most positive Z first)
    FaceOrder order[6];
    for (int i = 0; i < 6; ++i) {
        float z = 0;
        for (int j = 0; j < 4; ++j)
            z += pz[kFaces[i].v[j]];
        order[i] = {i, z * 0.25f};
    }
    std::sort(std::begin(order), std::end(order),
              [](const FaceOrder& a, const FaceOrder& b) { return a.avgZ > b.avgZ; });

    QPointF clickPt(pos);
    for (int fi = 0; fi < 6; ++fi) {
        if (order[fi].avgZ <= 0)
            break;  // back-facing, not clickable

        const CubeFace& face = kFaces[order[fi].idx];
        QPolygonF poly;
        for (int j = 0; j < 4; ++j)
            poly << p[face.v[j]];

        if (poly.containsPoint(clickPt, Qt::OddEvenFill)) {
            outDirection = face.direction;
            outUp = face.up;
            return order[fi].idx;
        }
    }

    return -1;
}

// ── hitTestViewCubeFace ─────────────────────────────────────────────────────

int hitTestViewCubeFace(const QPoint& pos,
                        const QMatrix4x4& viewRot,
                        int widgetWidth, int /*widgetHeight*/,
                        int cubeSize, int margin)
{
    const float cx = widgetWidth - margin - cubeSize * 0.5f;
    const float cy = margin + cubeSize * 0.5f;
    const float ddx = pos.x() - cx;
    const float ddy = pos.y() - cy;
    const float radius = cubeSize * 0.55f;
    if (ddx * ddx + ddy * ddy > radius * radius)
        return -1;

    QPointF p[8];
    float   pz[8];
    projectCorners(viewRot, cubeSize, cx, cy, p, pz);

    FaceOrder order[6];
    for (int i = 0; i < 6; ++i) {
        float z = 0;
        for (int j = 0; j < 4; ++j)
            z += pz[kFaces[i].v[j]];
        order[i] = {i, z * 0.25f};
    }
    std::sort(std::begin(order), std::end(order),
              [](const FaceOrder& a, const FaceOrder& b) { return a.avgZ > b.avgZ; });

    QPointF clickPt(pos);
    for (int fi = 0; fi < 6; ++fi) {
        if (order[fi].avgZ <= 0) break;
        QPolygonF poly;
        for (int j = 0; j < 4; ++j)
            poly << p[kFaces[order[fi].idx].v[j]];
        if (poly.containsPoint(clickPt, Qt::OddEvenFill))
            return order[fi].idx;
    }
    return -1;
}

// ── isInViewCubeArea ────────────────────────────────────────────────────────

bool isInViewCubeArea(const QPoint& pos,
                      int widgetWidth, int /*widgetHeight*/,
                      int cubeSize, int margin)
{
    const float cx = widgetWidth - margin - cubeSize * 0.5f;
    const float cy = margin + cubeSize * 0.5f;
    const float ddx = pos.x() - cx;
    const float ddy = pos.y() - cy;
    const float radius = cubeSize * 0.55f;
    return (ddx * ddx + ddy * ddy) <= (radius * radius);
}

} // namespace ViewportOverlays
