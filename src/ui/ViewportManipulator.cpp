#include "ViewportManipulator.h"
#include <QPainter>
#include <QPen>
#include <QFont>
#include <QFontMetrics>
#include <QtMath>
#include <cmath>
#include <algorithm>

// =============================================================================
// Construction / destruction
// =============================================================================

ViewportManipulator::ViewportManipulator(QObject* parent)
    : QObject(parent)
{
}

ViewportManipulator::~ViewportManipulator()
{
    // GPU resources are parented to the GL context and cleaned up automatically.
}

// =============================================================================
// Show / hide
// =============================================================================

void ViewportManipulator::showDistance(const QVector3D& origin,
                                      const QVector3D& direction,
                                      double currentValue,
                                      double minValue, double maxValue)
{
    m_type = ManipulatorType::Distance;
    m_origin = origin;
    m_direction = direction.normalized();
    m_currentValue = currentValue;
    m_minValue = minValue;
    m_maxValue = maxValue;
    m_directionSign = 1;
    m_visible = true;
    m_dragging = false;
    m_hovering = false;
    m_gpuInitialized = false;  // Force rebuild of arrow geometry
}

void ViewportManipulator::showAngle(const QVector3D& origin,
                                    const QVector3D& axis,
                                    double currentAngleDeg,
                                    double minAngle, double maxAngle)
{
    m_type = ManipulatorType::Angle;
    m_origin = origin;
    m_direction = axis.normalized();
    m_currentValue = currentAngleDeg;
    m_minValue = minAngle;
    m_maxValue = maxAngle;
    m_directionSign = 1;
    m_visible = true;
    m_dragging = false;
    m_hovering = false;
    m_gpuInitialized = false;
}

void ViewportManipulator::hide()
{
    m_visible = false;
    m_dragging = false;
    m_hovering = false;
}

bool ViewportManipulator::isVisible() const { return m_visible; }
bool ViewportManipulator::isDragging() const { return m_dragging; }
double ViewportManipulator::currentValue() const { return m_currentValue; }
int ViewportManipulator::directionSign() const { return m_directionSign; }

// =============================================================================
// Direction flip
// =============================================================================

void ViewportManipulator::flipDirection()
{
    m_directionSign = -m_directionSign;
    m_gpuInitialized = false;  // Rebuild geometry
    emit directionFlipped(m_directionSign);
}

// =============================================================================
// Projection helpers
// =============================================================================

QPointF ViewportManipulator::worldToScreen(const QVector3D& worldPt,
                                           const QMatrix4x4& view,
                                           const QMatrix4x4& proj,
                                           int viewportW, int viewportH) const
{
    QVector4D clip = proj * view * QVector4D(worldPt, 1.0f);
    if (std::abs(clip.w()) < 1e-6f)
        return {-1000, -1000};

    float ndcX = clip.x() / clip.w();
    float ndcY = clip.y() / clip.w();

    float screenX = (ndcX * 0.5f + 0.5f) * static_cast<float>(viewportW);
    float screenY = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(viewportH);
    return {static_cast<qreal>(screenX), static_cast<qreal>(screenY)};
}

double ViewportManipulator::screenProjection(const QPoint& screenPos,
                                             const QMatrix4x4& view,
                                             const QMatrix4x4& proj,
                                             int viewportW, int viewportH) const
{
    // Project origin and a point along the direction to screen space.
    QPointF screenOrigin = worldToScreen(m_origin, view, proj, viewportW, viewportH);
    QVector3D tipWorld = m_origin + m_direction * static_cast<float>(m_directionSign) * 1.0f;
    QPointF screenTip = worldToScreen(tipWorld, view, proj, viewportW, viewportH);

    // Screen-space direction vector of the manipulator axis
    QPointF axisDir(screenTip.x() - screenOrigin.x(),
                    screenTip.y() - screenOrigin.y());
    double axisLen = std::sqrt(axisDir.x() * axisDir.x() + axisDir.y() * axisDir.y());
    if (axisLen < 1e-6)
        return 0;

    // Normalize axis direction
    axisDir /= axisLen;

    // Vector from screen origin to the mouse position
    QPointF mouseVec(screenPos.x() - screenOrigin.x(),
                     screenPos.y() - screenOrigin.y());

    // Signed projection along the axis in screen pixels
    return mouseVec.x() * axisDir.x() + mouseVec.y() * axisDir.y();
}

bool ViewportManipulator::hitTestArrowTip(const QPoint& screenPos,
                                          const QMatrix4x4& view,
                                          const QMatrix4x4& proj,
                                          int viewportW, int viewportH,
                                          float threshold) const
{
    QVector3D tipWorld = m_origin + m_direction * static_cast<float>(m_directionSign * m_currentValue);
    QPointF screenTip = worldToScreen(tipWorld, view, proj, viewportW, viewportH);
    double dx = screenPos.x() - screenTip.x();
    double dy = screenPos.y() - screenTip.y();
    return (dx * dx + dy * dy) < static_cast<double>(threshold * threshold);
}

bool ViewportManipulator::hitTestFlipArrow(const QPoint& screenPos,
                                           const QMatrix4x4& view,
                                           const QMatrix4x4& proj,
                                           int viewportW, int viewportH,
                                           float threshold) const
{
    QPointF screenOrigin = worldToScreen(m_origin, view, proj, viewportW, viewportH);
    double dx = screenPos.x() - screenOrigin.x();
    double dy = screenPos.y() - screenOrigin.y();
    return (dx * dx + dy * dy) < static_cast<double>(threshold * threshold);
}

// =============================================================================
// Mouse event handlers
// =============================================================================

bool ViewportManipulator::handleMousePress(const QPoint& screenPos,
                                           const QMatrix4x4& view,
                                           const QMatrix4x4& proj,
                                           int viewportW, int viewportH)
{
    if (!m_visible)
        return false;

    // Check flip arrow click first (at the origin)
    if (hitTestFlipArrow(screenPos, view, proj, viewportW, viewportH, 18.0f)) {
        flipDirection();
        return true;
    }

    // Check arrow tip handle
    if (hitTestArrowTip(screenPos, view, proj, viewportW, viewportH, 24.0f)) {
        m_dragging = true;
        m_dragStartScreen = screenPos;
        m_dragStartValue = m_currentValue;
        m_dragStartProjection = screenProjection(screenPos, view, proj,
                                                 viewportW, viewportH);
        emit dragStarted();
        return true;
    }

    return false;
}

bool ViewportManipulator::handleMouseMove(const QPoint& screenPos,
                                          const QMatrix4x4& view,
                                          const QMatrix4x4& proj,
                                          int viewportW, int viewportH)
{
    if (!m_visible)
        return false;

    if (m_dragging) {
        // Compute new value from screen movement along the axis.
        double currentProjection = screenProjection(screenPos, view, proj,
                                                    viewportW, viewportH);
        double deltaPixels = currentProjection - m_dragStartProjection;

        // Convert screen pixels to world units.
        // Determine pixels-per-unit by projecting a 1mm segment of the direction.
        QPointF screenOrigin = worldToScreen(m_origin, view, proj, viewportW, viewportH);
        QVector3D oneUnitWorld = m_origin + m_direction * static_cast<float>(m_directionSign) * 1.0f;
        QPointF screenOneUnit = worldToScreen(oneUnitWorld, view, proj, viewportW, viewportH);

        double pixelsPerUnit = std::sqrt(
            std::pow(screenOneUnit.x() - screenOrigin.x(), 2.0) +
            std::pow(screenOneUnit.y() - screenOrigin.y(), 2.0));

        if (pixelsPerUnit < 1e-3)
            return true;

        double deltaUnits = deltaPixels / pixelsPerUnit;
        double newValue = m_dragStartValue + deltaUnits;

        // Clamp
        newValue = std::clamp(newValue, m_minValue, m_maxValue);

        if (std::abs(newValue - m_currentValue) > 1e-6) {
            m_currentValue = newValue;
            m_gpuInitialized = false;  // Rebuild arrow
            emit valueChanged(newValue);
        }
        return true;
    }

    // Hover detection -- check if mouse is near the arrow tip
    bool wasHovering = m_hovering;
    m_hovering = hitTestArrowTip(screenPos, view, proj, viewportW, viewportH, 24.0f)
              || hitTestFlipArrow(screenPos, view, proj, viewportW, viewportH, 18.0f);
    if (m_hovering != wasHovering)
        m_gpuInitialized = false;  // Rebuild to change color

    return false;  // Don't consume the event for hover
}

bool ViewportManipulator::handleMouseRelease()
{
    if (!m_visible || !m_dragging)
        return false;

    m_dragging = false;
    emit dragFinished(m_currentValue);
    return true;
}

// =============================================================================
// GPU buffer management
// =============================================================================

void ViewportManipulator::ensureGPUBuffers(QOpenGLFunctions_3_3_Core* gl)
{
    if (m_gpuInitialized)
        return;

    updateArrowGeometry(gl);
    m_gpuInitialized = true;
}

void ViewportManipulator::updateArrowGeometry(QOpenGLFunctions_3_3_Core* gl)
{
    // Build arrow shaft, arrowhead triangle, and base sphere proxy as line segments.
    // The geometry is built in world coordinates for simplicity.

    float dist = static_cast<float>(m_currentValue) * static_cast<float>(m_directionSign);
    QVector3D tip = m_origin + m_direction * dist;

    // --- Arrow shaft: a single line from origin to tip (with dashes) ---
    constexpr int kDashSegments = 32;
    std::vector<float> shaftVerts;
    shaftVerts.reserve(kDashSegments * 2 * 3);

    for (int i = 0; i < kDashSegments; ++i) {
        // Alternate dash/gap: draw even segments
        if (i % 2 == 0) {
            float t0 = static_cast<float>(i) / static_cast<float>(kDashSegments);
            float t1 = static_cast<float>(i + 1) / static_cast<float>(kDashSegments);
            QVector3D p0 = m_origin + (tip - m_origin) * t0;
            QVector3D p1 = m_origin + (tip - m_origin) * t1;
            shaftVerts.push_back(p0.x()); shaftVerts.push_back(p0.y()); shaftVerts.push_back(p0.z());
            shaftVerts.push_back(p1.x()); shaftVerts.push_back(p1.y()); shaftVerts.push_back(p1.z());
        }
    }

    // Also add a solid line for when it's dragging/hovering
    if (m_dragging || m_hovering) {
        shaftVerts.clear();
        shaftVerts.push_back(m_origin.x()); shaftVerts.push_back(m_origin.y()); shaftVerts.push_back(m_origin.z());
        shaftVerts.push_back(tip.x()); shaftVerts.push_back(tip.y()); shaftVerts.push_back(tip.z());
    }

    // Upload shaft
    if (!m_arrowVao.isCreated()) {
        m_arrowVao.create();
        m_arrowVbo.create();
    }
    m_arrowVao.bind();
    m_arrowVbo.bind();
    m_arrowVbo.allocate(shaftVerts.data(),
                        static_cast<int>(shaftVerts.size() * sizeof(float)));
    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    m_arrowVbo.release();
    m_arrowVao.release();

    // --- Arrowhead: a small triangle at the tip ---
    // Build a triangle perpendicular to the direction, with its point at the tip.
    float headLen = std::max(0.5f, std::abs(dist) * 0.08f);
    headLen = std::min(headLen, 3.0f);

    // Find two perpendicular vectors to the direction
    QVector3D perp1, perp2;
    if (std::abs(m_direction.x()) < 0.9f)
        perp1 = QVector3D::crossProduct(m_direction, QVector3D(1, 0, 0)).normalized();
    else
        perp1 = QVector3D::crossProduct(m_direction, QVector3D(0, 1, 0)).normalized();
    perp2 = QVector3D::crossProduct(m_direction, perp1).normalized();

    QVector3D headBase = tip - m_direction * static_cast<float>(m_directionSign) * headLen;
    float headRadius = headLen * 0.5f;

    // Triangle fan as line segments (hexagonal cone outline)
    constexpr int kHeadSegments = 6;
    std::vector<float> headVerts;
    headVerts.reserve((kHeadSegments + 1) * 2 * 3);

    for (int i = 0; i < kHeadSegments; ++i) {
        float angle = static_cast<float>(i) * 6.2832f / static_cast<float>(kHeadSegments);
        QVector3D basePoint = headBase + (perp1 * std::cos(angle) + perp2 * std::sin(angle)) * headRadius;
        // Line from base point to tip
        headVerts.push_back(basePoint.x()); headVerts.push_back(basePoint.y()); headVerts.push_back(basePoint.z());
        headVerts.push_back(tip.x()); headVerts.push_back(tip.y()); headVerts.push_back(tip.z());
        // Line around the base ring
        float nextAngle = static_cast<float>(i + 1) * 6.2832f / static_cast<float>(kHeadSegments);
        QVector3D nextPoint = headBase + (perp1 * std::cos(nextAngle) + perp2 * std::sin(nextAngle)) * headRadius;
        headVerts.push_back(basePoint.x()); headVerts.push_back(basePoint.y()); headVerts.push_back(basePoint.z());
        headVerts.push_back(nextPoint.x()); headVerts.push_back(nextPoint.y()); headVerts.push_back(nextPoint.z());
    }

    if (!m_headVao.isCreated()) {
        m_headVao.create();
        m_headVbo.create();
    }
    m_headVao.bind();
    m_headVbo.bind();
    m_headVbo.allocate(headVerts.data(),
                       static_cast<int>(headVerts.size() * sizeof(float)));
    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    m_headVbo.release();
    m_headVao.release();

    // --- Base sphere: a small wireframe circle at the origin ---
    constexpr int kBaseSegments = 12;
    float baseRadius = headRadius * 0.6f;
    std::vector<float> baseVerts;
    baseVerts.reserve(kBaseSegments * 2 * 3);

    for (int i = 0; i < kBaseSegments; ++i) {
        float a0 = static_cast<float>(i) * 6.2832f / static_cast<float>(kBaseSegments);
        float a1 = static_cast<float>(i + 1) * 6.2832f / static_cast<float>(kBaseSegments);
        QVector3D p0 = m_origin + (perp1 * std::cos(a0) + perp2 * std::sin(a0)) * baseRadius;
        QVector3D p1 = m_origin + (perp1 * std::cos(a1) + perp2 * std::sin(a1)) * baseRadius;
        baseVerts.push_back(p0.x()); baseVerts.push_back(p0.y()); baseVerts.push_back(p0.z());
        baseVerts.push_back(p1.x()); baseVerts.push_back(p1.y()); baseVerts.push_back(p1.z());
    }

    if (!m_baseVao.isCreated()) {
        m_baseVao.create();
        m_baseVbo.create();
    }
    m_baseVao.bind();
    m_baseVbo.bind();
    m_baseVbo.allocate(baseVerts.data(),
                       static_cast<int>(baseVerts.size() * sizeof(float)));
    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    m_baseVbo.release();
    m_baseVao.release();
}

// =============================================================================
// Draw (OpenGL)
// =============================================================================

void ViewportManipulator::draw(QOpenGLFunctions_3_3_Core* gl,
                               QOpenGLShaderProgram* edgeProgram,
                               const QMatrix4x4& view,
                               const QMatrix4x4& proj)
{
    if (!m_visible || !gl || !edgeProgram)
        return;

    ensureGPUBuffers(gl);

    QMatrix4x4 mvp = proj * view;  // model is identity

    edgeProgram->bind();
    edgeProgram->setUniformValue("uMVP", mvp);
    edgeProgram->setUniformValue("uDepthBias", 0.001f);  // Push in front

    // Arrow shaft color: yellow (#FFD700), brighter when hovering/dragging
    QVector3D shaftColor = (m_dragging || m_hovering)
        ? QVector3D(1.0f, 0.88f, 0.0f)   // Bright yellow
        : QVector3D(0.9f, 0.78f, 0.0f);  // Yellow

    edgeProgram->setUniformValue("uEdgeColor", shaftColor);
    edgeProgram->setUniformValue("uEdgeAlpha", 1.0f);

    gl->glEnable(GL_LINE_SMOOTH);
    gl->glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    gl->glLineWidth(m_dragging ? 3.0f : 2.0f);

    // Disable depth test so the manipulator is always visible
    gl->glDisable(GL_DEPTH_TEST);

    // Draw shaft
    float dist = static_cast<float>(m_currentValue) * static_cast<float>(m_directionSign);
    int shaftVertCount = (m_dragging || m_hovering)
        ? 2
        : (32 / 2) * 2;  // Half of kDashSegments * 2 vertices per dash
    if (std::abs(dist) > 1e-6f) {
        m_arrowVao.bind();
        gl->glDrawArrays(GL_LINES, 0, shaftVertCount);
        m_arrowVao.release();
    }

    // Draw arrowhead (same color)
    if (std::abs(dist) > 1e-6f) {
        gl->glLineWidth(2.0f);
        m_headVao.bind();
        gl->glDrawArrays(GL_LINES, 0, 6 * 4);  // kHeadSegments * 4 verts
        m_headVao.release();
    }

    // Draw base circle (blue for flip-arrow region)
    edgeProgram->setUniformValue("uEdgeColor", QVector3D(0.2f, 0.5f, 1.0f));
    gl->glLineWidth(2.5f);
    m_baseVao.bind();
    gl->glDrawArrays(GL_LINES, 0, 12 * 2);  // kBaseSegments * 2 verts
    m_baseVao.release();

    // Restore state
    gl->glEnable(GL_DEPTH_TEST);
    gl->glDisable(GL_LINE_SMOOTH);
    gl->glLineWidth(1.0f);

    edgeProgram->release();
}

// =============================================================================
// Draw overlay (QPainter -- value label and flip arrow icon)
// =============================================================================

void ViewportManipulator::drawOverlay(QPainter& painter,
                                      const QMatrix4x4& view,
                                      const QMatrix4x4& proj,
                                      int viewportW, int viewportH)
{
    if (!m_visible)
        return;

    painter.setRenderHint(QPainter::Antialiasing, true);

    // --- Value label at the arrow tip ---
    float dist = static_cast<float>(m_currentValue) * static_cast<float>(m_directionSign);
    QVector3D tipWorld = m_origin + m_direction * dist;
    QPointF tipScreen = worldToScreen(tipWorld, view, proj, viewportW, viewportH);

    QString valueText = QString::number(m_currentValue, 'f', 1) + " mm";

    QFont labelFont("Sans", 11, QFont::Bold);
    painter.setFont(labelFont);
    QFontMetrics fm(labelFont);
    QRect textRect = fm.boundingRect(valueText);

    // Position the label offset from the tip
    float labelOffX = 12.0f;
    float labelOffY = -16.0f;
    QPointF labelPos(tipScreen.x() + static_cast<qreal>(labelOffX),
                     tipScreen.y() + static_cast<qreal>(labelOffY));

    // Draw background rounded rect
    QRectF bgRect(labelPos.x() - 4, labelPos.y() - textRect.height() + 2,
                  textRect.width() + 8, textRect.height() + 4);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(30, 30, 30, 200));
    painter.drawRoundedRect(bgRect, 4, 4);

    // Draw text
    painter.setPen(QColor(255, 215, 0));  // Gold/yellow
    painter.drawText(labelPos, valueText);

    // --- Flip arrow icon at the origin ---
    QPointF originScreen = worldToScreen(m_origin, view, proj, viewportW, viewportH);

    // Draw a small double-arrow (up/down) icon
    painter.setPen(QPen(QColor(60, 140, 255), 2.5));

    // Up arrow
    float arrowSize = 10.0f;
    float arrowGap = 4.0f;
    QPointF upTop(originScreen.x(), originScreen.y() - arrowGap - arrowSize);
    QPointF upMid(originScreen.x(), originScreen.y() - arrowGap);
    painter.drawLine(upMid, upTop);
    painter.drawLine(upTop, QPointF(upTop.x() - 4, upTop.y() + 4));
    painter.drawLine(upTop, QPointF(upTop.x() + 4, upTop.y() + 4));

    // Down arrow
    QPointF downBot(originScreen.x(), originScreen.y() + arrowGap + arrowSize);
    QPointF downMid(originScreen.x(), originScreen.y() + arrowGap);
    painter.drawLine(downMid, downBot);
    painter.drawLine(downBot, QPointF(downBot.x() - 4, downBot.y() - 4));
    painter.drawLine(downBot, QPointF(downBot.x() + 4, downBot.y() - 4));

    // Draw a small "flip" label if hovering near origin
    if (m_hovering) {
        QFont smallFont("Sans", 8);
        painter.setFont(smallFont);
        painter.setPen(QColor(60, 140, 255, 200));
        painter.drawText(QPointF(originScreen.x() + 14, originScreen.y() + 4), "Flip");
    }
}
