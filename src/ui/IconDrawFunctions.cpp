#include "IconDrawFunctions.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <cmath>

// ─── helpers ────────────────────────────────────────────────────────────────

namespace {

/// Margin inside each icon (keeps shapes away from the edge).
constexpr int kMargin = 3;

/// Create a transparent pixmap of the given size with high-DPI support.
QPixmap makePixmap(int size) {
    QPixmap px(size, size);
    px.fill(Qt::transparent);
    return px;
}

/// Set up the painter with antialiasing and a default pen.
void initPainter(QPainter& p, const QColor& penColor, qreal penWidth = 1.5) {
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(penColor, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
}

} // anonymous namespace

// ─── icon draw functions ─────────────────────────────────────────────────────

QPixmap IconDraw::extrude(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);

    // Subtle rounded-rect background
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(100, 180, 255, 150), 1.0));
    p.setBrush(QColor(100, 180, 255, 40));
    p.drawRoundedRect(QRectF(2, 2, size - 4, size - 4), 8, 8);

    initPainter(p, QColor(100, 180, 255), 2.0);

    int m = kMargin;
    int w = size - 2*m;
    int h = size - 2*m;

    // Base rectangle (profile)
    QRectF base(m + w*0.15, m + h*0.55, w*0.7, h*0.35);
    p.setBrush(QColor(100, 180, 255, 60));
    p.drawRect(base);

    // Extruded top rectangle (offset up)
    QRectF top(m + w*0.15, m + h*0.15, w*0.7, h*0.35);
    p.setBrush(QColor(100, 180, 255, 30));
    p.drawRect(top);

    // Upward arrow in the center
    p.setPen(QPen(QColor(100, 180, 255), 2.0, Qt::SolidLine, Qt::RoundCap));
    qreal cx = m + w*0.5;
    p.drawLine(QPointF(cx, m + h*0.7), QPointF(cx, m + h*0.1));
    // Arrowhead
    p.drawLine(QPointF(cx, m + h*0.1), QPointF(cx - w*0.1, m + h*0.22));
    p.drawLine(QPointF(cx, m + h*0.1), QPointF(cx + w*0.1, m + h*0.22));

    return px;
}

QPixmap IconDraw::revolve(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);

    // Subtle rounded-rect background
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(100, 200, 130, 150), 1.0));
    p.setBrush(QColor(100, 200, 130, 40));
    p.drawRoundedRect(QRectF(2, 2, size - 4, size - 4), 8, 8);

    initPainter(p, QColor(100, 200, 130), 2.0);

    int m = kMargin;
    int w = size - 2*m;
    int h = size - 2*m;

    // Vertical axis (dashed)
    p.setPen(QPen(QColor(100, 200, 130, 150), 1.0, Qt::DashLine));
    qreal ax = m + w*0.3;
    p.drawLine(QPointF(ax, m), QPointF(ax, m + h));

    // Half-profile on the right of axis
    p.setPen(QPen(QColor(100, 200, 130), 2.0));
    p.setBrush(QColor(100, 200, 130, 40));
    QPainterPath profile;
    profile.moveTo(ax, m + h*0.2);
    profile.lineTo(m + w*0.7, m + h*0.2);
    profile.lineTo(m + w*0.7, m + h*0.8);
    profile.lineTo(ax, m + h*0.8);
    profile.closeSubpath();
    p.drawPath(profile);

    // Rotation arrow (curved arrow around axis)
    p.setPen(QPen(QColor(100, 200, 130), 2.0, Qt::SolidLine, Qt::RoundCap));
    QPainterPath arrow;
    qreal r = w * 0.3;
    qreal cy = m + h*0.5;
    arrow.moveTo(ax - r*0.4, cy - r*0.8);
    arrow.cubicTo(ax + r*0.6, cy - r*1.2,
                  ax + r*0.8, cy + r*0.4,
                  ax - r*0.2, cy + r*0.6);
    p.drawPath(arrow);
    // Small arrowhead at end
    QPointF tip = arrow.pointAtPercent(1.0);
    p.drawLine(tip, tip + QPointF(-4, -3));
    p.drawLine(tip, tip + QPointF(2, -4));

    return px;
}

QPixmap IconDraw::fillet(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);

    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(255, 170, 60, 150), 1.0));
    p.setBrush(QColor(255, 170, 60, 40));
    p.drawRoundedRect(QRectF(2, 2, size - 4, size - 4), 8, 8);

    initPainter(p, QColor(255, 170, 60), 2.5);

    int m = kMargin + 1;
    int w = size - 2*m;
    int h = size - 2*m;

    // L-shape with rounded corner
    QPainterPath path;
    path.moveTo(m, m + h*0.15);
    path.lineTo(m, m + h*0.85);  // left edge down
    path.lineTo(m + w*0.85, m + h*0.85);  // bottom edge right
    path.lineTo(m + w*0.85, m + h*0.45); // right edge up to corner area
    // Rounded corner (the fillet)
    path.cubicTo(m + w*0.85, m + h*0.25,
                 m + w*0.6,  m + h*0.15,
                 m + w*0.4,  m + h*0.15);
    path.lineTo(m, m + h*0.15);

    p.setBrush(QColor(255, 170, 60, 40));
    p.drawPath(path);

    // Highlight the arc portion
    p.setPen(QPen(QColor(255, 220, 100), 3.0, Qt::SolidLine, Qt::RoundCap));
    QPainterPath arc;
    arc.moveTo(m + w*0.85, m + h*0.45);
    arc.cubicTo(m + w*0.85, m + h*0.25,
                m + w*0.6,  m + h*0.15,
                m + w*0.4,  m + h*0.15);
    p.drawPath(arc);

    return px;
}

QPixmap IconDraw::chamfer(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(255, 170, 60, 150), 1.0));
    p.setBrush(QColor(255, 170, 60, 40));
    p.drawRoundedRect(QRectF(2, 2, size - 4, size - 4), 8, 8);
    initPainter(p, QColor(255, 170, 60), 2.5);

    int m = kMargin + 1;
    int w = size - 2*m;
    int h = size - 2*m;

    // L-shape with angled cut at corner
    QPainterPath path;
    path.moveTo(m, m + h*0.15);
    path.lineTo(m, m + h*0.85);
    path.lineTo(m + w*0.85, m + h*0.85);
    path.lineTo(m + w*0.85, m + h*0.45);
    // Chamfer: straight diagonal line instead of curve
    path.lineTo(m + w*0.4, m + h*0.15);
    path.lineTo(m, m + h*0.15);

    p.setBrush(QColor(255, 170, 60, 40));
    p.drawPath(path);

    // Highlight the chamfer line
    p.setPen(QPen(QColor(255, 220, 100), 3.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(m + w*0.85, m + h*0.45), QPointF(m + w*0.4, m + h*0.15));

    return px;
}

QPixmap IconDraw::hole(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(220, 80, 80, 150), 1.0));
    p.setBrush(QColor(220, 80, 80, 40));
    p.drawRoundedRect(QRectF(2, 2, size - 4, size - 4), 8, 8);
    initPainter(p, QColor(220, 80, 80), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;
    qreal cx = m + w*0.5;
    qreal cy = m + h*0.5;

    // Outer circle
    p.setBrush(QColor(220, 80, 80, 30));
    p.drawEllipse(QPointF(cx, cy), w*0.42, h*0.42);

    // Inner circle (the hole)
    p.setBrush(QColor(220, 80, 80, 60));
    p.drawEllipse(QPointF(cx, cy), w*0.18, h*0.18);

    // Center crosshair
    p.setPen(QPen(QColor(220, 80, 80), 1.0, Qt::DashLine));
    p.drawLine(QPointF(cx - w*0.35, cy), QPointF(cx + w*0.35, cy));
    p.drawLine(QPointF(cx, cy - h*0.35), QPointF(cx, cy + h*0.35));

    return px;
}

QPixmap IconDraw::shell(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(80, 200, 200, 150), 1.0));
    p.setBrush(QColor(80, 200, 200, 40));
    p.drawRoundedRect(QRectF(2, 2, size - 4, size - 4), 8, 8);
    initPainter(p, QColor(80, 200, 200), 2.0);

    int m = kMargin + 1;
    int w = size - 2*m;
    int h = size - 2*m;

    // Outer rectangle
    p.setBrush(QColor(80, 200, 200, 40));
    QRectF outer(m, m + h*0.15, w, h*0.85);
    p.drawRect(outer);

    // Inner rectangle (hollow) -- open at top
    p.setBrush(QColor(40, 40, 40));
    qreal t = w * 0.15; // wall thickness
    QRectF inner(m + t, m + h*0.15, w - 2*t, h*0.85 - t);
    p.drawRect(inner);

    // Opening indicator at top
    p.setPen(QPen(QColor(80, 200, 200, 180), 1.5, Qt::DashLine));
    p.drawLine(QPointF(m + t, m + h*0.15), QPointF(m + w - t, m + h*0.15));

    return px;
}

QPixmap IconDraw::draft(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(180, 140, 255, 150), 1.0));
    p.setBrush(QColor(180, 140, 255, 40));
    p.drawRoundedRect(QRectF(2, 2, size - 4, size - 4), 8, 8);
    initPainter(p, QColor(180, 140, 255), 2.0);

    int m = kMargin + 1;
    int w = size - 2*m;
    int h = size - 2*m;

    // Original vertical shape
    p.setPen(QPen(QColor(180, 140, 255, 100), 1.5, Qt::DashLine));
    p.drawLine(QPointF(m + w*0.35, m + h*0.15), QPointF(m + w*0.35, m + h*0.85));
    p.drawLine(QPointF(m + w*0.65, m + h*0.15), QPointF(m + w*0.65, m + h*0.85));

    // Drafted shape (tapered)
    p.setPen(QPen(QColor(180, 140, 255), 2.5));
    p.setBrush(QColor(180, 140, 255, 40));
    QPainterPath path;
    path.moveTo(m + w*0.3, m + h*0.85);
    path.lineTo(m + w*0.4, m + h*0.15);
    path.lineTo(m + w*0.6, m + h*0.15);
    path.lineTo(m + w*0.7, m + h*0.85);
    path.closeSubpath();
    p.drawPath(path);

    // Angle arc indicator
    p.setPen(QPen(QColor(255, 200, 100), 1.5));
    p.drawArc(QRectF(m + w*0.2, m + h*0.65, w*0.3, h*0.3), 60*16, 30*16);

    return px;
}

QPixmap IconDraw::sketch(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);

    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(180, 130, 255, 150), 1.0));
    p.setBrush(QColor(180, 130, 255, 40));
    p.drawRoundedRect(QRectF(2, 2, size - 4, size - 4), 8, 8);

    initPainter(p, QColor(180, 130, 255), 1.5);

    int m = kMargin;
    int w = size - 2*m;
    int h = size - 2*m;

    // Grid lines (faint)
    p.setPen(QPen(QColor(180, 130, 255, 50), 0.5));
    for (int i = 0; i <= 4; ++i) {
        qreal x = m + w * i / 4.0;
        qreal y = m + h * i / 4.0;
        p.drawLine(QPointF(x, m), QPointF(x, m + h));
        p.drawLine(QPointF(m, y), QPointF(m + w, y));
    }

    // Pencil shape
    p.setPen(QPen(QColor(180, 130, 255), 2.0, Qt::SolidLine, Qt::RoundCap));
    // Pencil body (diagonal)
    QPointF tip(m + w*0.2, m + h*0.8);
    QPointF top(m + w*0.8, m + h*0.2);
    p.drawLine(tip, top);
    // Pencil sides
    QPointF offset(2.5, 2.5);
    p.drawLine(tip, top + offset);
    p.drawLine(tip, top - offset);

    return px;
}

QPixmap IconDraw::line(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(230, 230, 230), 2.5);

    int m = kMargin + 3;
    p.drawLine(QPointF(m, size - m), QPointF(size - m, m));

    // Endpoints
    p.setBrush(QColor(100, 180, 255));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(m, size - m), 2.5, 2.5);
    p.drawEllipse(QPointF(size - m, m), 2.5, 2.5);

    return px;
}

QPixmap IconDraw::rectangle(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(230, 230, 230), 2.0);

    int m = kMargin + 3;
    p.drawRect(m, m + 2, size - 2*m, size - 2*m - 4);

    return px;
}

QPixmap IconDraw::circle(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(230, 230, 230), 2.0);

    qreal cx = size * 0.5;
    qreal cy = size * 0.5;
    qreal r = (size - 2*kMargin - 4) * 0.5;
    p.drawEllipse(QPointF(cx, cy), r, r);

    // Center dot
    p.setBrush(QColor(100, 180, 255));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(cx, cy), 2, 2);

    return px;
}

QPixmap IconDraw::arc(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(230, 230, 230), 2.0);

    int m = kMargin + 2;
    p.drawArc(QRectF(m, m, size - 2*m, size - 2*m), 30*16, 120*16);

    // Endpoints
    p.setBrush(QColor(100, 180, 255));
    p.setPen(Qt::NoPen);
    qreal r2 = (size - 2*m) * 0.5;
    qreal cx = m + r2, cy = m + r2;
    p.drawEllipse(QPointF(cx + r2*std::cos(30.0*M_PI/180.0), cy - r2*std::sin(30.0*M_PI/180.0)), 2.5, 2.5);
    p.drawEllipse(QPointF(cx + r2*std::cos(150.0*M_PI/180.0), cy - r2*std::sin(150.0*M_PI/180.0)), 2.5, 2.5);

    return px;
}

QPixmap IconDraw::ellipse(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(230, 230, 230), 2.0);

    int m = kMargin + 2;
    p.drawEllipse(QRectF(m, m + 4, size - 2*m, size - 2*m - 8));

    return px;
}

QPixmap IconDraw::polygon(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(230, 230, 230), 2.0);

    qreal cx = size * 0.5;
    qreal cy = size * 0.5;
    qreal r = (size - 2*kMargin - 4) * 0.5;
    int sides = 6;
    QPolygonF poly;
    for (int i = 0; i < sides; ++i) {
        qreal angle = -M_PI/2 + 2*M_PI*i/sides;
        poly << QPointF(cx + r*std::cos(angle), cy + r*std::sin(angle));
    }
    p.drawPolygon(poly);

    return px;
}

QPixmap IconDraw::slot(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(230, 230, 230), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;
    qreal rr = h * 0.25;
    p.drawRoundedRect(QRectF(m, m + h*0.25, w, h*0.5), rr, rr);

    return px;
}

QPixmap IconDraw::box(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(100, 180, 255, 150), 1.0));
    p.setBrush(QColor(100, 180, 255, 40));
    p.drawRoundedRect(QRectF(2, 2, size - 4, size - 4), 8, 8);
    initPainter(p, QColor(100, 180, 255), 1.8);

    int m = kMargin + 1;
    int w = size - 2*m;
    int h = size - 2*m;

    // Isometric box: front face, top face, side face
    qreal ox = w * 0.2; // isometric offset

    // Front face
    QPolygonF front;
    front << QPointF(m, m + h*0.35) << QPointF(m + w*0.65, m + h*0.35)
          << QPointF(m + w*0.65, m + h) << QPointF(m, m + h);
    p.setBrush(QColor(100, 180, 255, 50));
    p.drawPolygon(front);

    // Top face
    QPolygonF top;
    top << QPointF(m, m + h*0.35) << QPointF(m + ox, m)
        << QPointF(m + w*0.65 + ox, m) << QPointF(m + w*0.65, m + h*0.35);
    p.setBrush(QColor(100, 180, 255, 80));
    p.drawPolygon(top);

    // Right face
    QPolygonF right;
    right << QPointF(m + w*0.65, m + h*0.35) << QPointF(m + w*0.65 + ox, m)
          << QPointF(m + w*0.65 + ox, m + h*0.65) << QPointF(m + w*0.65, m + h);
    p.setBrush(QColor(100, 180, 255, 30));
    p.drawPolygon(right);

    return px;
}

QPixmap IconDraw::cylinder(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(100, 180, 255, 150), 1.0));
    p.setBrush(QColor(100, 180, 255, 40));
    p.drawRoundedRect(QRectF(2, 2, size - 4, size - 4), 8, 8);
    initPainter(p, QColor(100, 180, 255), 1.8);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    qreal ey = h * 0.12; // ellipse vertical radius for top/bottom caps

    // Body sides
    p.setBrush(QColor(100, 180, 255, 40));
    p.drawRect(QRectF(m, m + ey, w, h - 2*ey));

    // Bottom ellipse
    p.setBrush(QColor(100, 180, 255, 25));
    p.drawEllipse(QRectF(m, m + h - 2*ey, w, 2*ey));

    // Top ellipse
    p.setBrush(QColor(100, 180, 255, 70));
    p.drawEllipse(QRectF(m, m, w, 2*ey));

    return px;
}

QPixmap IconDraw::sphere(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(100, 180, 255, 150), 1.0));
    p.setBrush(QColor(100, 180, 255, 40));
    p.drawRoundedRect(QRectF(2, 2, size - 4, size - 4), 8, 8);
    initPainter(p, QColor(100, 180, 255), 1.8);

    int m = kMargin + 2;
    qreal cx = size * 0.5;
    qreal cy = size * 0.5;
    qreal r = (size - 2*m) * 0.5;

    // Gradient fill for 3D look
    QRadialGradient grad(cx - r*0.3, cy - r*0.3, r*1.4);
    grad.setColorAt(0, QColor(140, 210, 255, 120));
    grad.setColorAt(1, QColor(40, 80, 140, 40));
    p.setBrush(grad);
    p.drawEllipse(QPointF(cx, cy), r, r);

    // Equator line
    p.setPen(QPen(QColor(100, 180, 255, 100), 1.0));
    p.drawEllipse(QRectF(cx - r, cy - r*0.15, 2*r, r*0.3));

    return px;
}

QPixmap IconDraw::mirror(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(180, 180, 190, 150), 1.0));
    p.setBrush(QColor(180, 180, 190, 40));
    p.drawRoundedRect(QRectF(2, 2, size - 4, size - 4), 8, 8);
    initPainter(p, QColor(180, 180, 190), 1.8);

    int m = kMargin + 1;
    int w = size - 2*m;
    int h = size - 2*m;
    qreal cx = m + w*0.5;

    // Mirror axis (dashed center line)
    p.setPen(QPen(QColor(255, 200, 100), 1.5, Qt::DashLine));
    p.drawLine(QPointF(cx, m), QPointF(cx, m + h));

    // Left shape
    p.setPen(QPen(QColor(180, 180, 190), 1.8));
    p.setBrush(QColor(180, 180, 190, 50));
    QRectF left(m + w*0.05, m + h*0.25, w*0.35, h*0.5);
    p.drawRect(left);

    // Right shape (mirrored, slightly transparent to indicate it's the copy)
    p.setBrush(QColor(180, 180, 190, 30));
    p.setPen(QPen(QColor(180, 180, 190, 140), 1.8, Qt::DashLine));
    QRectF right(m + w*0.6, m + h*0.25, w*0.35, h*0.5);
    p.drawRect(right);

    return px;
}

QPixmap IconDraw::rectPattern(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(180, 180, 190), 1.5);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;
    qreal cellW = w / 3.0;
    qreal cellH = h / 3.0;
    qreal boxS = cellW * 0.55;

    p.setBrush(QColor(180, 180, 190, 50));
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            qreal x = m + c*cellW + (cellW - boxS)*0.5;
            qreal y = m + r*cellH + (cellH - boxS)*0.5;
            if (r == 0 && c == 0) {
                p.setBrush(QColor(100, 180, 255, 80));
                p.setPen(QPen(QColor(100, 180, 255), 1.5));
            } else {
                p.setBrush(QColor(180, 180, 190, 40));
                p.setPen(QPen(QColor(180, 180, 190), 1.5));
            }
            p.drawRect(QRectF(x, y, boxS, boxS));
        }
    }

    return px;
}

QPixmap IconDraw::circPattern(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(180, 180, 190), 1.5);

    qreal cx = size * 0.5;
    qreal cy = size * 0.5;
    qreal r = (size - 2*kMargin - 6) * 0.5;
    qreal boxR = 3.5;
    int count = 6;

    // Dashed circle path
    p.setPen(QPen(QColor(180, 180, 190, 80), 1.0, Qt::DashLine));
    p.drawEllipse(QPointF(cx, cy), r, r);

    for (int i = 0; i < count; ++i) {
        qreal angle = 2*M_PI*i/count - M_PI/2;
        qreal x = cx + r*std::cos(angle);
        qreal y = cy + r*std::sin(angle);
        if (i == 0) {
            p.setBrush(QColor(100, 180, 255, 80));
            p.setPen(QPen(QColor(100, 180, 255), 1.5));
        } else {
            p.setBrush(QColor(180, 180, 190, 40));
            p.setPen(QPen(QColor(180, 180, 190), 1.5));
        }
        p.drawRect(QRectF(x - boxR, y - boxR, boxR*2, boxR*2));
    }

    return px;
}

QPixmap IconDraw::measure(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(255, 220, 80), 1.8);

    int m = kMargin + 1;
    int w = size - 2*m;
    int h = size - 2*m;

    // Ruler body
    p.setBrush(QColor(255, 220, 80, 40));
    QRectF ruler(m, m + h*0.3, w, h*0.4);
    p.drawRect(ruler);

    // Tick marks along the top
    p.setPen(QPen(QColor(255, 220, 80), 1.2));
    int ticks = 7;
    for (int i = 0; i <= ticks; ++i) {
        qreal x = m + w * i / (qreal)ticks;
        qreal tickH = (i % 2 == 0) ? h*0.15 : h*0.08;
        p.drawLine(QPointF(x, m + h*0.3), QPointF(x, m + h*0.3 - tickH));
    }

    return px;
}

QPixmap IconDraw::undo(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(200, 200, 200), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Curved left arrow
    QPainterPath path;
    path.moveTo(m + w*0.7, m + h*0.7);
    path.cubicTo(m + w*0.8, m + h*0.3,
                 m + w*0.4, m + h*0.15,
                 m + w*0.15, m + h*0.45);
    p.drawPath(path);

    // Arrowhead
    QPointF tip(m + w*0.15, m + h*0.45);
    p.drawLine(tip, tip + QPointF(6, -4));
    p.drawLine(tip, tip + QPointF(5, 5));

    return px;
}

QPixmap IconDraw::redo(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(200, 200, 200), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Curved right arrow
    QPainterPath path;
    path.moveTo(m + w*0.3, m + h*0.7);
    path.cubicTo(m + w*0.2, m + h*0.3,
                 m + w*0.6, m + h*0.15,
                 m + w*0.85, m + h*0.45);
    p.drawPath(path);

    // Arrowhead
    QPointF tip(m + w*0.85, m + h*0.45);
    p.drawLine(tip, tip + QPointF(-6, -4));
    p.drawLine(tip, tip + QPointF(-5, 5));

    return px;
}

QPixmap IconDraw::save(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(200, 200, 200), 1.8);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Floppy disk body
    p.setBrush(QColor(200, 200, 200, 50));
    QPainterPath disk;
    disk.moveTo(m, m);
    disk.lineTo(m + w, m);
    disk.lineTo(m + w, m + h);
    disk.lineTo(m, m + h);
    disk.closeSubpath();
    p.drawPath(disk);

    // Metal slider (top center)
    p.setBrush(QColor(200, 200, 200, 80));
    qreal slotW = w * 0.5;
    p.drawRect(QRectF(m + (w - slotW)*0.5, m, slotW, h*0.35));

    // Label area (bottom)
    p.setBrush(QColor(100, 180, 255, 60));
    p.drawRect(QRectF(m + w*0.15, m + h*0.6, w*0.7, h*0.3));

    return px;
}

QPixmap IconDraw::open(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(200, 200, 200), 1.8);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Folder back
    p.setBrush(QColor(220, 190, 80, 60));
    QPainterPath back;
    back.moveTo(m, m + h*0.2);
    back.lineTo(m + w*0.35, m + h*0.2);
    back.lineTo(m + w*0.45, m + h*0.05);
    back.lineTo(m + w, m + h*0.05);
    back.lineTo(m + w, m + h);
    back.lineTo(m, m + h);
    back.closeSubpath();
    p.drawPath(back);

    // Folder front flap (open)
    p.setBrush(QColor(220, 190, 80, 80));
    QPainterPath front;
    front.moveTo(m, m + h*0.4);
    front.lineTo(m + w*0.2, m + h*0.25);
    front.lineTo(m + w, m + h*0.25);
    front.lineTo(m + w*0.85, m + h);
    front.lineTo(m, m + h);
    front.closeSubpath();
    p.drawPath(front);

    return px;
}

QPixmap IconDraw::newIcon(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(200, 200, 200), 1.8);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Page with folded corner
    p.setBrush(QColor(200, 200, 200, 40));
    QPainterPath page;
    qreal fold = w * 0.25;
    page.moveTo(m, m);
    page.lineTo(m + w - fold, m);
    page.lineTo(m + w, m + fold);
    page.lineTo(m + w, m + h);
    page.lineTo(m, m + h);
    page.closeSubpath();
    p.drawPath(page);

    // Fold triangle
    p.setBrush(QColor(200, 200, 200, 70));
    QPainterPath foldPath;
    foldPath.moveTo(m + w - fold, m);
    foldPath.lineTo(m + w - fold, m + fold);
    foldPath.lineTo(m + w, m + fold);
    foldPath.closeSubpath();
    p.drawPath(foldPath);

    // Plus sign in center
    p.setPen(QPen(QColor(100, 200, 130), 2.0));
    qreal cx = m + w*0.45;
    qreal cy = m + h*0.55;
    p.drawLine(QPointF(cx - 4, cy), QPointF(cx + 4, cy));
    p.drawLine(QPointF(cx, cy - 4), QPointF(cx, cy + 4));

    return px;
}

QPixmap IconDraw::sweep(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(100, 180, 255, 150), 1.0));
    p.setBrush(QColor(100, 180, 255, 40));
    p.drawRoundedRect(QRectF(2, 2, size - 4, size - 4), 8, 8);
    initPainter(p, QColor(100, 180, 255), 2.0);

    int m = kMargin + 1;
    int w = size - 2*m;
    int h = size - 2*m;

    // Curved path
    p.setPen(QPen(QColor(100, 180, 255, 140), 1.5, Qt::DashLine));
    QPainterPath path;
    path.moveTo(m + w*0.15, m + h*0.8);
    path.cubicTo(m + w*0.15, m + h*0.3,
                 m + w*0.85, m + h*0.3,
                 m + w*0.85, m + h*0.15);
    p.drawPath(path);

    // Profile circle at start
    p.setPen(QPen(QColor(100, 180, 255), 2.0));
    p.setBrush(QColor(100, 180, 255, 50));
    p.drawEllipse(QPointF(m + w*0.15, m + h*0.8), 5, 5);

    // Profile circle at end
    p.drawEllipse(QPointF(m + w*0.85, m + h*0.15), 4, 4);

    return px;
}

QPixmap IconDraw::loft(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(100, 180, 255, 150), 1.0));
    p.setBrush(QColor(100, 180, 255, 40));
    p.drawRoundedRect(QRectF(2, 2, size - 4, size - 4), 8, 8);
    initPainter(p, QColor(100, 180, 255), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Bottom profile (larger)
    p.setBrush(QColor(100, 180, 255, 40));
    p.drawEllipse(QRectF(m, m + h*0.65, w, h*0.3));

    // Top profile (smaller)
    qreal topW = w * 0.5;
    p.drawEllipse(QRectF(m + (w - topW)*0.5, m, topW, h*0.2));

    // Connecting lines (guide rails)
    p.setPen(QPen(QColor(100, 180, 255, 100), 1.2, Qt::DashLine));
    p.drawLine(QPointF(m, m + h*0.8), QPointF(m + (w - topW)*0.5, m + h*0.1));
    p.drawLine(QPointF(m + w, m + h*0.8), QPointF(m + (w + topW)*0.5, m + h*0.1));

    return px;
}

QPixmap IconDraw::joint(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(255, 180, 80), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Two blocks being joined
    p.setBrush(QColor(255, 180, 80, 40));
    p.drawRect(QRectF(m, m + h*0.1, w*0.4, h*0.35));
    p.drawRect(QRectF(m + w*0.6, m + h*0.55, w*0.4, h*0.35));

    // Connection symbol (two dots with line)
    p.setPen(QPen(QColor(255, 220, 100), 2.0));
    QPointF p1(m + w*0.4, m + h*0.35);
    QPointF p2(m + w*0.6, m + h*0.55);
    p.drawLine(p1, p2);
    p.setBrush(QColor(255, 220, 100));
    p.setPen(Qt::NoPen);
    p.drawEllipse(p1, 3, 3);
    p.drawEllipse(p2, 3, 3);

    return px;
}

QPixmap IconDraw::component(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(100, 200, 130), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Component box with a plus
    p.setBrush(QColor(100, 200, 130, 40));
    p.drawRect(QRectF(m, m, w, h));

    p.setPen(QPen(QColor(100, 200, 130), 2.5));
    qreal cx = m + w*0.5;
    qreal cy = m + h*0.5;
    p.drawLine(QPointF(cx - 5, cy), QPointF(cx + 5, cy));
    p.drawLine(QPointF(cx, cy - 5), QPointF(cx, cy + 5));

    return px;
}

QPixmap IconDraw::insert(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(180, 180, 190), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Box outline
    p.setBrush(QColor(180, 180, 190, 30));
    p.drawRect(QRectF(m + w*0.25, m, w*0.75, h*0.75));

    // Arrow pointing into box
    p.setPen(QPen(QColor(100, 180, 255), 2.0));
    p.drawLine(QPointF(m, m + h), QPointF(m + w*0.5, m + h*0.5));
    QPointF tip(m + w*0.5, m + h*0.5);
    p.drawLine(tip, tip + QPointF(-6, 1));
    p.drawLine(tip, tip + QPointF(-1, 6));

    return px;
}

QPixmap IconDraw::trim(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(230, 230, 230), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Two crossing lines
    p.drawLine(QPointF(m, m + h*0.3), QPointF(m + w, m + h*0.3));
    p.drawLine(QPointF(m + w*0.5, m), QPointF(m + w*0.5, m + h));

    // Red X on the trimmed segment
    p.setPen(QPen(QColor(220, 80, 80), 2.5));
    qreal cx = m + w*0.75;
    qreal cy = m + h*0.3;
    p.drawLine(QPointF(cx-3, cy-3), QPointF(cx+3, cy+3));
    p.drawLine(QPointF(cx+3, cy-3), QPointF(cx-3, cy+3));

    return px;
}

QPixmap IconDraw::extend(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(230, 230, 230), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Original line
    p.drawLine(QPointF(m, m + h*0.6), QPointF(m + w*0.5, m + h*0.6));

    // Extended portion (dashed, then solid to target)
    p.setPen(QPen(QColor(100, 200, 130), 2.0, Qt::DashLine));
    p.drawLine(QPointF(m + w*0.5, m + h*0.6), QPointF(m + w*0.85, m + h*0.6));

    // Target line (vertical)
    p.setPen(QPen(QColor(230, 230, 230), 2.0));
    p.drawLine(QPointF(m + w*0.85, m + h*0.2), QPointF(m + w*0.85, m + h));

    return px;
}

QPixmap IconDraw::offset(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(230, 230, 230), 2.0);

    int m = kMargin + 3;
    int w = size - 2*m;
    int h = size - 2*m;

    // Original rectangle
    p.drawRect(QRectF(m, m + 2, w*0.65, h*0.65));

    // Offset rectangle
    p.setPen(QPen(QColor(100, 180, 255), 2.0, Qt::DashLine));
    p.drawRect(QRectF(m + w*0.2, m + h*0.2 + 2, w*0.65, h*0.65));

    // Arrow between them
    p.setPen(QPen(QColor(255, 200, 100), 1.5));
    p.drawLine(QPointF(m + w*0.3, m + h*0.3), QPointF(m + w*0.5, m + h*0.5));

    return px;
}

QPixmap IconDraw::project(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(230, 230, 230), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // 3D edge above
    p.setPen(QPen(QColor(180, 180, 190), 2.0));
    p.drawLine(QPointF(m + w*0.1, m + h*0.2), QPointF(m + w*0.9, m + h*0.35));

    // Projection arrow down
    p.setPen(QPen(QColor(255, 200, 100), 1.5, Qt::DashLine));
    p.drawLine(QPointF(m + w*0.5, m + h*0.3), QPointF(m + w*0.5, m + h*0.6));

    // Projected line on sketch plane
    p.setPen(QPen(QColor(100, 180, 255), 2.0));
    p.drawLine(QPointF(m + w*0.1, m + h*0.7), QPointF(m + w*0.9, m + h*0.7));

    // Plane
    p.setPen(QPen(QColor(180, 130, 255, 60), 0.8));
    p.drawLine(QPointF(m, m + h*0.65), QPointF(m + w, m + h*0.65));
    p.drawLine(QPointF(m, m + h*0.85), QPointF(m + w, m + h*0.85));

    return px;
}

QPixmap IconDraw::construction(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(255, 170, 60), 2.0);

    int m = kMargin + 3;

    // Diagonal dashed line (construction line style)
    p.setPen(QPen(QColor(255, 170, 60), 2.0, Qt::DashDotLine));
    p.drawLine(QPointF(m, size - m), QPointF(size - m, m));

    return px;
}

QPixmap IconDraw::select(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(230, 230, 230), 2.0);

    int m = kMargin + 2;

    // Arrow cursor shape
    QPainterPath cursor;
    cursor.moveTo(m + 2, m);
    cursor.lineTo(m + 2, size - m - 4);
    cursor.lineTo(m + 8, size - m - 10);
    cursor.lineTo(m + 14, size - m - 2);
    cursor.lineTo(m + 16, size - m - 4);
    cursor.lineTo(m + 10, size - m - 12);
    cursor.lineTo(m + 16, size - m - 12);
    cursor.closeSubpath();
    p.setBrush(QColor(230, 230, 230, 200));
    p.drawPath(cursor);

    return px;
}

QPixmap IconDraw::finish(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(100, 200, 130), 2.5);

    int m = kMargin + 4;
    int w = size - 2*m;
    int h = size - 2*m;

    // Checkmark
    QPainterPath check;
    check.moveTo(m, m + h*0.55);
    check.lineTo(m + w*0.35, m + h);
    check.lineTo(m + w, m);
    p.drawPath(check);

    return px;
}

QPixmap IconDraw::deleteIcon(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(220, 80, 80), 2.5);

    int m = kMargin + 4;
    p.drawLine(QPointF(m, m), QPointF(size - m, size - m));
    p.drawLine(QPointF(size - m, m), QPointF(m, size - m));

    return px;
}

QPixmap IconDraw::interference(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(220, 160, 60), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Two overlapping circles
    p.setBrush(QColor(220, 160, 60, 30));
    p.drawEllipse(QPointF(m + w*0.35, m + h*0.5), w*0.3, h*0.35);
    p.setBrush(QColor(220, 80, 80, 30));
    p.drawEllipse(QPointF(m + w*0.65, m + h*0.5), w*0.3, h*0.35);

    // Highlight overlap region with exclamation
    p.setPen(QPen(QColor(255, 80, 80), 2.5));
    qreal cx = m + w*0.5;
    qreal cy = m + h*0.45;
    p.drawLine(QPointF(cx, cy - 4), QPointF(cx, cy + 3));
    p.drawPoint(QPointF(cx, cy + 6));

    return px;
}

// ─── Construct group icons ──────────────────────────────────────────────────

QPixmap IconDraw::plane(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(26, 188, 156), 2.0);  // teal

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Parallelogram representing a plane in perspective
    QPolygonF quad;
    quad << QPointF(m + w*0.1,  m + h*0.65)
         << QPointF(m + w*0.45, m + h*0.25)
         << QPointF(m + w*0.9,  m + h*0.35)
         << QPointF(m + w*0.55, m + h*0.75);
    p.setBrush(QColor(26, 188, 156, 50));
    p.drawPolygon(quad);

    return px;
}

QPixmap IconDraw::axis(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(26, 188, 156), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Diagonal line with arrowhead (axis direction)
    QPointF start(m + w*0.15, m + h*0.85);
    QPointF end(m + w*0.85, m + h*0.15);
    p.drawLine(start, end);

    // Arrowhead
    p.drawLine(end, QPointF(m + w*0.7, m + h*0.18));
    p.drawLine(end, QPointF(m + w*0.82, m + h*0.32));

    // Small circle at base
    p.setBrush(QColor(26, 188, 156));
    p.drawEllipse(start, 2.5, 2.5);

    return px;
}

QPixmap IconDraw::point(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(26, 188, 156), 2.0);

    qreal cx = size * 0.5;
    qreal cy = size * 0.5;

    // Crosshair around a filled dot
    int m = kMargin + 2;
    int w = size - 2*m;
    p.drawLine(QPointF(cx - w*0.35, cy), QPointF(cx - w*0.1, cy));
    p.drawLine(QPointF(cx + w*0.1, cy), QPointF(cx + w*0.35, cy));
    p.drawLine(QPointF(cx, cy - w*0.35), QPointF(cx, cy - w*0.1));
    p.drawLine(QPointF(cx, cy + w*0.1), QPointF(cx, cy + w*0.35));

    // Center dot
    p.setBrush(QColor(26, 188, 156));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(cx, cy), 3.0, 3.0);

    return px;
}

// ─── Sketch constraint icons ────────────────────────────────────────────────

QPixmap IconDraw::coincident(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(255, 200, 60), 2.0);  // gold

    qreal cx = size * 0.5;
    qreal cy = size * 0.5;

    // Two concentric circles (points merged)
    p.drawEllipse(QPointF(cx, cy), size*0.22, size*0.22);
    p.setBrush(QColor(255, 200, 60));
    p.drawEllipse(QPointF(cx, cy), size*0.08, size*0.08);

    return px;
}

QPixmap IconDraw::parallel(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(255, 200, 60), 2.0);

    int m = kMargin + 4;
    // Two parallel diagonal lines
    p.drawLine(QPointF(m, size - m), QPointF(size*0.45, m));
    p.drawLine(QPointF(size*0.55, size - m), QPointF(size - m, m));

    return px;
}

QPixmap IconDraw::perpendicular(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(255, 200, 60), 2.0);

    int m = kMargin + 4;
    int w = size - 2*m;

    // Inverted L shape
    p.drawLine(QPointF(m, size - m), QPointF(m + w, size - m));
    p.drawLine(QPointF(m, size - m), QPointF(m, m));

    // Right-angle square
    qreal sq = w * 0.2;
    p.setPen(QPen(QColor(255, 200, 60), 1.2));
    p.drawLine(QPointF(m + sq, size - m), QPointF(m + sq, size - m - sq));
    p.drawLine(QPointF(m, size - m - sq), QPointF(m + sq, size - m - sq));

    return px;
}

QPixmap IconDraw::tangentIcon(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(255, 200, 60), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Circle with tangent line
    QPointF center(m + w*0.45, m + h*0.55);
    qreal r = w * 0.28;
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(center, r, r);
    // Tangent line at top of circle
    p.drawLine(QPointF(m, m + h*0.27), QPointF(m + w, m + h*0.27));

    return px;
}

QPixmap IconDraw::equalIcon(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(255, 200, 60), 2.5);

    int m = kMargin + 5;
    qreal cy = size * 0.5;

    // Two horizontal parallel lines (equals sign)
    p.drawLine(QPointF(m, cy - 4), QPointF(size - m, cy - 4));
    p.drawLine(QPointF(m, cy + 4), QPointF(size - m, cy + 4));

    return px;
}

QPixmap IconDraw::symmetric(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(255, 200, 60), 2.0);

    int m = kMargin + 3;
    int w = size - 2*m;
    int h = size - 2*m;
    qreal cx = size * 0.5;

    // Dashed center line (axis of symmetry)
    p.setPen(QPen(QColor(255, 200, 60, 140), 1.2, Qt::DashLine));
    p.drawLine(QPointF(cx, m), QPointF(cx, m + h));

    // Two mirrored small triangles
    p.setPen(QPen(QColor(255, 200, 60), 2.0));
    p.setBrush(QColor(255, 200, 60, 60));

    QPolygonF left;
    left << QPointF(cx - w*0.1, m + h*0.35)
         << QPointF(cx - w*0.4, m + h*0.5)
         << QPointF(cx - w*0.1, m + h*0.65);
    p.drawPolygon(left);

    QPolygonF right;
    right << QPointF(cx + w*0.1, m + h*0.35)
          << QPointF(cx + w*0.4, m + h*0.5)
          << QPointF(cx + w*0.1, m + h*0.65);
    p.drawPolygon(right);

    return px;
}

QPixmap IconDraw::fix(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(255, 200, 60), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Pin / pushpin shape: circle head, line body, hatched base
    qreal cx = m + w*0.5;
    p.setBrush(QColor(255, 200, 60, 80));
    p.drawEllipse(QPointF(cx, m + h*0.25), w*0.18, w*0.18);
    p.drawLine(QPointF(cx, m + h*0.43), QPointF(cx, m + h*0.75));

    // Ground hatching
    p.drawLine(QPointF(m + w*0.2, m + h*0.75), QPointF(m + w*0.8, m + h*0.75));
    p.setPen(QPen(QColor(255, 200, 60, 120), 1.0));
    for (int i = 0; i < 4; ++i) {
        qreal x = m + w*(0.25 + i*0.15);
        p.drawLine(QPointF(x, m + h*0.75), QPointF(x - w*0.08, m + h*0.88));
    }

    return px;
}

QPixmap IconDraw::dimension(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(255, 200, 60), 1.5);

    int m = kMargin + 3;
    int w = size - 2*m;
    int h = size - 2*m;
    qreal cy = m + h*0.5;

    // Horizontal dimension line with arrowheads at both ends
    p.drawLine(QPointF(m, cy), QPointF(m + w, cy));

    // Left arrowhead
    p.drawLine(QPointF(m, cy), QPointF(m + w*0.12, cy - h*0.1));
    p.drawLine(QPointF(m, cy), QPointF(m + w*0.12, cy + h*0.1));

    // Right arrowhead
    p.drawLine(QPointF(m + w, cy), QPointF(m + w*0.88, cy - h*0.1));
    p.drawLine(QPointF(m + w, cy), QPointF(m + w*0.88, cy + h*0.1));

    // Extension lines
    p.setPen(QPen(QColor(255, 200, 60, 140), 1.0));
    p.drawLine(QPointF(m, m + h*0.2), QPointF(m, m + h*0.8));
    p.drawLine(QPointF(m + w, m + h*0.2), QPointF(m + w, m + h*0.8));

    return px;
}

// ─── Additional sketch tool icons ───────────────────────────────────────────

QPixmap IconDraw::spline(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(100, 180, 255), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // S-curve spline
    QPainterPath path;
    path.moveTo(m, m + h*0.8);
    path.cubicTo(m + w*0.3, m + h*0.1,
                 m + w*0.7, m + h*0.9,
                 m + w, m + h*0.2);
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // Control point dots
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(100, 180, 255, 180));
    p.drawEllipse(QPointF(m, m + h*0.8), 2.5, 2.5);
    p.drawEllipse(QPointF(m + w*0.3, m + h*0.1), 2.5, 2.5);
    p.drawEllipse(QPointF(m + w*0.7, m + h*0.9), 2.5, 2.5);
    p.drawEllipse(QPointF(m + w, m + h*0.2), 2.5, 2.5);

    return px;
}

QPixmap IconDraw::centerRectangle(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(100, 180, 255), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;
    qreal cx = m + w*0.5;
    qreal cy = m + h*0.5;

    // Rectangle outline
    p.setBrush(QColor(100, 180, 255, 30));
    p.drawRect(QRectF(m + w*0.15, m + h*0.2, w*0.7, h*0.6));

    // Center crosshair
    p.setPen(QPen(QColor(255, 120, 60), 1.5, Qt::DashDotLine));
    p.drawLine(QPointF(cx, m + h*0.1), QPointF(cx, m + h*0.9));
    p.drawLine(QPointF(m + w*0.05, cy), QPointF(m + w*0.95, cy));

    // Center dot
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 120, 60));
    p.drawEllipse(QPointF(cx, cy), 2.5, 2.5);

    return px;
}

QPixmap IconDraw::circle3Point(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(100, 180, 255), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;
    qreal cx = m + w*0.5;
    qreal cy = m + h*0.5;
    qreal r = w * 0.38;

    // Circle
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(cx, cy), r, r);

    // 3 dots on the circle (at ~120 degree intervals)
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 160, 60));
    for (int i = 0; i < 3; ++i) {
        double angle = 2.0 * M_PI * i / 3.0 - M_PI / 2.0;
        QPointF pt(cx + r * std::cos(angle), cy + r * std::sin(angle));
        p.drawEllipse(pt, 3.0, 3.0);
    }

    return px;
}

QPixmap IconDraw::arc3Point(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(100, 180, 255), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Arc (half circle, open at bottom)
    QPainterPath path;
    QRectF arcRect(m + w*0.1, m + h*0.15, w*0.8, h*0.8);
    path.arcMoveTo(arcRect, 30);
    path.arcTo(arcRect, 30, 120);
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // 3 dots on the arc (start, mid, end)
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 160, 60));
    qreal cx = m + w*0.5;
    qreal cy = m + h*0.55;
    qreal rx = w*0.4;
    qreal ry = h*0.4;
    // Points at 30, 90, 150 degrees
    for (double deg : {30.0, 90.0, 150.0}) {
        double rad = deg * M_PI / 180.0;
        QPointF pt(cx + rx * std::cos(rad), cy - ry * std::sin(rad));
        p.drawEllipse(pt, 3.0, 3.0);
    }

    return px;
}

QPixmap IconDraw::filletSketch(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(255, 170, 60), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Small L-shape with rounded corner
    QPainterPath path;
    path.moveTo(m + w*0.15, m + h*0.2);
    path.lineTo(m + w*0.15, m + h*0.65);
    // Rounded corner
    path.cubicTo(m + w*0.15, m + h*0.82,
                 m + w*0.32, m + h*0.85,
                 m + w*0.5,  m + h*0.85);
    path.lineTo(m + w*0.85, m + h*0.85);

    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // Highlight the fillet arc
    p.setPen(QPen(QColor(255, 100, 40), 2.5));
    QPainterPath arc;
    arc.moveTo(m + w*0.15, m + h*0.65);
    arc.cubicTo(m + w*0.15, m + h*0.82,
                m + w*0.32, m + h*0.85,
                m + w*0.5,  m + h*0.85);
    p.drawPath(arc);

    return px;
}

QPixmap IconDraw::chamferSketch(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(255, 170, 60), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // L-shape with angled cut at the corner
    QPainterPath path;
    path.moveTo(m + w*0.15, m + h*0.2);
    path.lineTo(m + w*0.15, m + h*0.6);
    // Angled chamfer cut
    path.lineTo(m + w*0.45, m + h*0.85);
    path.lineTo(m + w*0.85, m + h*0.85);

    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // Highlight the chamfer line
    p.setPen(QPen(QColor(255, 100, 40), 2.5));
    p.drawLine(QPointF(m + w*0.15, m + h*0.6), QPointF(m + w*0.45, m + h*0.85));

    return px;
}

QPixmap IconDraw::constraintIcon(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(255, 200, 60), 2.0);

    int m = kMargin + 2;
    int w = size - 2*m;
    int h = size - 2*m;

    // Two lines meeting at a point with a constraint symbol (small square)
    p.drawLine(QPointF(m, m + h*0.8), QPointF(m + w*0.5, m + h*0.35));
    p.drawLine(QPointF(m + w*0.5, m + h*0.35), QPointF(m + w, m + h*0.8));

    // Right-angle square at the vertex
    qreal sq = w * 0.15;
    qreal vx = m + w*0.5;
    qreal vy = m + h*0.35;
    p.setPen(QPen(QColor(255, 200, 60), 1.5));
    p.setBrush(QColor(255, 200, 60, 60));
    p.drawRect(QRectF(vx - sq, vy - sq*0.3, sq*2, sq*1.3));

    // "C" letter inside the box
    QFont f("Monospace", static_cast<int>(size * 0.2), QFont::Bold);
    f.setStyleHint(QFont::Monospace);
    p.setFont(f);
    p.setPen(QColor(255, 200, 60));
    p.drawText(QRectF(vx - sq, vy - sq*0.3, sq*2, sq*1.3), Qt::AlignCenter, "C");

    return px;
}

// ─── Import icons ───────────────────────────────────────────────────────────

QPixmap IconDraw::importDxf(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(100, 200, 130, 150), 1.0));
    p.setBrush(QColor(100, 200, 130, 40));
    p.drawRoundedRect(QRectF(2, 2, size - 4, size - 4), 8, 8);

    initPainter(p, QColor(100, 200, 130), 1.5);

    int m = kMargin + 1;
    int w = size - 2*m;
    int h = size - 2*m;

    // Page outline with folded corner
    qreal fold = w * 0.2;
    QPainterPath page;
    page.moveTo(m + w*0.1, m + h*0.05);
    page.lineTo(m + w*0.65 - fold, m + h*0.05);
    page.lineTo(m + w*0.65, m + h*0.05 + fold);
    page.lineTo(m + w*0.65, m + h*0.7);
    page.lineTo(m + w*0.1, m + h*0.7);
    page.closeSubpath();
    p.setBrush(QColor(100, 200, 130, 25));
    p.drawPath(page);

    // Fold triangle
    QPainterPath foldPath;
    foldPath.moveTo(m + w*0.65 - fold, m + h*0.05);
    foldPath.lineTo(m + w*0.65 - fold, m + h*0.05 + fold);
    foldPath.lineTo(m + w*0.65, m + h*0.05 + fold);
    foldPath.closeSubpath();
    p.setBrush(QColor(100, 200, 130, 50));
    p.drawPath(foldPath);

    // Import arrow (pointing into sketch)
    p.setPen(QPen(QColor(100, 200, 130), 2.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(m + w*0.8, m + h*0.5), QPointF(m + w*0.8, m + h*0.95));
    QPointF tip(m + w*0.8, m + h*0.95);
    p.drawLine(tip, tip + QPointF(-4, -5));
    p.drawLine(tip, tip + QPointF(4, -5));

    // "DXF" text
    QFont f("Monospace", std::max(6, size / 5), QFont::Bold);
    f.setStyleHint(QFont::Monospace);
    p.setFont(f);
    p.setPen(QColor(100, 200, 130));
    p.drawText(QRectF(m, m + h*0.72, w*0.7, h*0.28), Qt::AlignCenter, "DXF");

    return px;
}

QPixmap IconDraw::importSvg(int size) {
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(180, 130, 255, 150), 1.0));
    p.setBrush(QColor(180, 130, 255, 40));
    p.drawRoundedRect(QRectF(2, 2, size - 4, size - 4), 8, 8);

    initPainter(p, QColor(180, 130, 255), 1.5);

    int m = kMargin + 1;
    int w = size - 2*m;
    int h = size - 2*m;

    // Page outline with folded corner
    qreal fold = w * 0.2;
    QPainterPath page;
    page.moveTo(m + w*0.1, m + h*0.05);
    page.lineTo(m + w*0.65 - fold, m + h*0.05);
    page.lineTo(m + w*0.65, m + h*0.05 + fold);
    page.lineTo(m + w*0.65, m + h*0.7);
    page.lineTo(m + w*0.1, m + h*0.7);
    page.closeSubpath();
    p.setBrush(QColor(180, 130, 255, 25));
    p.drawPath(page);

    // Fold triangle
    QPainterPath foldPath;
    foldPath.moveTo(m + w*0.65 - fold, m + h*0.05);
    foldPath.lineTo(m + w*0.65 - fold, m + h*0.05 + fold);
    foldPath.lineTo(m + w*0.65, m + h*0.05 + fold);
    foldPath.closeSubpath();
    p.setBrush(QColor(180, 130, 255, 50));
    p.drawPath(foldPath);

    // Import arrow (pointing into sketch)
    p.setPen(QPen(QColor(180, 130, 255), 2.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(m + w*0.8, m + h*0.5), QPointF(m + w*0.8, m + h*0.95));
    QPointF tip(m + w*0.8, m + h*0.95);
    p.drawLine(tip, tip + QPointF(-4, -5));
    p.drawLine(tip, tip + QPointF(4, -5));

    // "SVG" text
    QFont f("Monospace", std::max(6, size / 5), QFont::Bold);
    f.setStyleHint(QFont::Monospace);
    p.setFont(f);
    p.setPen(QColor(180, 130, 255));
    p.drawText(QRectF(m, m + h*0.72, w*0.7, h*0.28), Qt::AlignCenter, "SVG");

    return px;
}
