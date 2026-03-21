#include "MarkingMenu.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QtMath>
#include <cmath>

// =============================================================================
// Constants
// =============================================================================

static constexpr int   kMenuSize        = 300;   // widget width/height
static constexpr float kDeadZoneRadius  = 38.0f; // inner dead zone
static constexpr float kOuterRadius     = 125.0f;
static constexpr float kTextRadius      = 85.0f; // where label text is drawn
static constexpr float kAnimDurationMs  = 120.0f;
static constexpr int   kAnimTickMs      = 16;

// Sector angles: each item occupies 360/8 = 45 degrees.
// Item 0 (N) is centered at -90 deg (straight up).  Clockwise: NE, E, SE, S, SW, W, NW.
static constexpr float kSectorSpan = 45.0f;

/// Sector center angles in degrees (math convention: 0 = right, CCW positive).
/// We use clockwise-from-north: N=90, NE=45, E=0, SE=-45, S=-90, SW=-135, W=180, NW=135.
/// But QPainter and atan2 use math convention, so we store math-convention angles.
static const float kSectorCenterDeg[8] = {
    90.0f,   // N
    45.0f,   // NE
    0.0f,    // E
    -45.0f,  // SE
    -90.0f,  // S
    -135.0f, // SW
    180.0f,  // W
    135.0f,  // NW
};

// =============================================================================
// Construction
// =============================================================================

MarkingMenu::MarkingMenu(QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Popup | Qt::NoDropShadowWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setFixedSize(kMenuSize, kMenuSize);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_center = QPoint(kMenuSize / 2, kMenuSize / 2);
    m_innerRadius = static_cast<int>(kDeadZoneRadius);
    m_outerRadius = static_cast<int>(kOuterRadius);

    // Fade-in animation timer
    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(kAnimTickMs);
    connect(m_animTimer, &QTimer::timeout, this, [this]() {
        m_animProgress += static_cast<float>(kAnimTickMs) / kAnimDurationMs;
        if (m_animProgress >= 1.0f) {
            m_animProgress = 1.0f;
            m_animTimer->stop();
        }
        update();
    });
}

// =============================================================================
// Public API
// =============================================================================

void MarkingMenu::setItems(const std::vector<MenuItem>& items)
{
    m_items = items;
    // Clamp to 8
    if (m_items.size() > 8)
        m_items.resize(8);
}

void MarkingMenu::showAt(const QPoint& globalPos)
{
    // Position so that the center of this widget is at globalPos
    QPoint topLeft = globalPos - QPoint(kMenuSize / 2, kMenuSize / 2);
    move(topLeft);

    m_hoveredIndex = -1;
    m_animProgress = 0.0f;
    m_animTimer->start();

    show();
    setFocus();
    grabMouse();
}

void MarkingMenu::commitSelection()
{
    releaseMouse();
    int idx = m_hoveredIndex;
    hide();

    if (idx >= 0 && idx < static_cast<int>(m_items.size())) {
        if (m_items[idx].action)
            m_items[idx].action();
        emit itemTriggered(idx);
    }
}

// =============================================================================
// Item hit testing
// =============================================================================

int MarkingMenu::itemAtPos(const QPoint& pos) const
{
    float dx = static_cast<float>(pos.x() - m_center.x());
    float dy = static_cast<float>(pos.y() - m_center.y());
    float dist = std::sqrt(dx * dx + dy * dy);

    // Dead zone
    if (dist < kDeadZoneRadius)
        return -1;

    // Outside menu
    if (dist > kOuterRadius + 20.0f)
        return -1;

    // Angle in degrees (math convention: 0=right, CCW positive)
    float angleDeg = qRadiansToDegrees(std::atan2(-dy, dx)); // negate dy (screen Y is down)

    // Find closest sector
    float bestDist = 999.0f;
    int bestIdx = -1;
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        float diff = angleDeg - kSectorCenterDeg[i];
        // Normalize to [-180, 180]
        while (diff > 180.0f) diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;
        float absDiff = std::abs(diff);
        if (absDiff < kSectorSpan / 2.0f && absDiff < bestDist) {
            bestDist = absDiff;
            bestIdx = i;
        }
    }
    return bestIdx;
}

// =============================================================================
// Paint
// =============================================================================

void MarkingMenu::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const float alpha = m_animProgress;
    const float scale = 0.85f + 0.15f * alpha; // grow from 85% to 100%

    p.save();
    p.translate(m_center);
    p.scale(scale, scale);
    p.translate(-m_center);

    const int globalAlpha = static_cast<int>(alpha * 255.0f);

    // ── Background circle ──────────────────────────────────────────────
    QColor bgColor(0x2d, 0x2d, 0x2d, static_cast<int>(0.85f * globalAlpha));
    p.setPen(Qt::NoPen);
    p.setBrush(bgColor);
    p.drawEllipse(m_center, m_outerRadius, m_outerRadius);

    // ── Sector dividers and highlights ─────────────────────────────────
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        float startAngle = kSectorCenterDeg[i] - kSectorSpan / 2.0f;

        if (i == m_hoveredIndex) {
            // Highlight sector
            QPainterPath sector;
            QRectF outerRect(m_center.x() - m_outerRadius, m_center.y() - m_outerRadius,
                             m_outerRadius * 2, m_outerRadius * 2);
            QRectF innerRect(m_center.x() - m_innerRadius, m_center.y() - m_innerRadius,
                             m_innerRadius * 2, m_innerRadius * 2);

            sector.arcMoveTo(outerRect, startAngle);
            sector.arcTo(outerRect, startAngle, kSectorSpan);
            sector.arcTo(innerRect, startAngle + kSectorSpan, -kSectorSpan);
            sector.closeSubpath();

            QColor hlColor(0x2a, 0x82, 0xda, static_cast<int>(0.90f * globalAlpha));
            p.setBrush(hlColor);
            p.setPen(Qt::NoPen);
            p.drawPath(sector);
        }
    }

    // ── Sector dividing lines ──────────────────────────────────────────
    {
        QPen divPen(QColor(80, 80, 80, globalAlpha), 1.0);
        p.setPen(divPen);
        for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
            float edgeAngle = kSectorCenterDeg[i] - kSectorSpan / 2.0f;
            float rad = qDegreesToRadians(edgeAngle);
            float cosA = std::cos(rad);
            float sinA = -std::sin(rad); // screen Y flipped
            QPointF inner(m_center.x() + m_innerRadius * cosA,
                          m_center.y() + m_innerRadius * sinA);
            QPointF outer(m_center.x() + m_outerRadius * cosA,
                          m_center.y() + m_outerRadius * sinA);
            p.drawLine(inner, outer);
        }
    }

    // ── Center dead-zone circle ────────────────────────────────────────
    {
        QColor centerColor(0x22, 0x22, 0x22, globalAlpha);
        p.setPen(QPen(QColor(90, 90, 90, globalAlpha), 1.5));
        p.setBrush(centerColor);
        p.drawEllipse(m_center, m_innerRadius, m_innerRadius);
    }

    // ── Labels ─────────────────────────────────────────────────────────
    {
        QFont labelFont = font();
        labelFont.setPixelSize(12);
        labelFont.setWeight(QFont::Medium);
        p.setFont(labelFont);

        for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
            float angleDeg = kSectorCenterDeg[i];
            float rad = qDegreesToRadians(angleDeg);
            float cosA = std::cos(rad);
            float sinA = -std::sin(rad);

            QPointF textCenter(m_center.x() + kTextRadius * cosA,
                               m_center.y() + kTextRadius * sinA);

            // Text color: white, brighter if hovered
            QColor textColor = (i == m_hoveredIndex)
                ? QColor(255, 255, 255, globalAlpha)
                : QColor(200, 200, 200, globalAlpha);
            p.setPen(textColor);

            // Draw centered text
            QRectF textRect(textCenter.x() - 50, textCenter.y() - 10, 100, 20);
            p.drawText(textRect, Qt::AlignCenter, m_items[i].text);

            // Draw shortcut hint below the label (smaller, dimmer)
            if (!m_items[i].shortcut.isEmpty()) {
                QFont hintFont = labelFont;
                hintFont.setPixelSize(9);
                p.setFont(hintFont);
                QColor hintColor(150, 150, 150, globalAlpha);
                p.setPen(hintColor);
                QRectF hintRect(textCenter.x() - 50, textCenter.y() + 8, 100, 14);
                p.drawText(hintRect, Qt::AlignCenter, m_items[i].shortcut);
                p.setFont(labelFont);
            }
        }
    }

    // ── Outer ring border ──────────────────────────────────────────────
    {
        QPen ringPen(QColor(70, 70, 70, globalAlpha), 1.5);
        p.setPen(ringPen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(m_center, m_outerRadius, m_outerRadius);
    }

    p.restore();
}

// =============================================================================
// Mouse interaction
// =============================================================================

void MarkingMenu::mouseMoveEvent(QMouseEvent* event)
{
    int newHover = itemAtPos(event->pos());
    if (newHover != m_hoveredIndex) {
        m_hoveredIndex = newHover;
        update();
    }
}

void MarkingMenu::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton || event->button() == Qt::LeftButton) {
        commitSelection();
    }
}

void MarkingMenu::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        releaseMouse();
        hide();
    }
}
