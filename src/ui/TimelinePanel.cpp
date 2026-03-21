#include "TimelinePanel.h"

#include <QContextMenuEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <map>

// ====================================================================
// TimelineIconWidget -- compact 34x34 feature icon
// ====================================================================

static constexpr int kIconSize = 34;
static constexpr int kIconInner = 20; // inner icon drawing area

TimelineIconWidget::TimelineIconWidget(const QString& featureId,
                                       features::FeatureType type,
                                       const QColor& iconColor,
                                       const QString& name,
                                       QWidget* parent)
    : QWidget(parent)
    , m_featureId(featureId)
    , m_featureType(type)
    , m_iconColor(iconColor)
    , m_name(name)
{
    setFixedSize(kIconSize, kIconSize);
    setCursor(Qt::OpenHandCursor);
    setToolTip(name);
}

void TimelineIconWidget::setDimmed(bool d)
{
    if (m_dimmed != d) { m_dimmed = d; update(); }
}

void TimelineIconWidget::setSuppressed(bool s)
{
    if (m_suppressed != s) { m_suppressed = s; update(); }
}

void TimelineIconWidget::setError(bool e)
{
    if (m_error != e) { m_error = e; update(); }
}

void TimelineIconWidget::setEditing(bool e)
{
    if (m_editing != e) { m_editing = e; update(); }
}

void TimelineIconWidget::setSelected(bool s)
{
    if (m_selected != s) { m_selected = s; update(); }
}

void TimelineIconWidget::mouseDoubleClickEvent(QMouseEvent* /*event*/)
{
    emit doubleClicked(m_featureId);
}

void TimelineIconWidget::enterEvent(QEnterEvent* /*event*/)
{
    m_hovered = true;
    update();
}

void TimelineIconWidget::leaveEvent(QEvent* /*event*/)
{
    m_hovered = false;
    update();
}

void TimelineIconWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const qreal opacity = m_dimmed ? 0.40 : 1.0;
    p.setOpacity(opacity);

    // 1. Background rounded rect
    QColor bg(0x3c, 0x3f, 0x41);  // #3c3f41
    if (m_hovered && !m_dimmed)
        bg = QColor(0x4a, 0x4d, 0x50);  // slightly lighter on hover
    if (m_selected)
        bg = QColor(50, 70, 100);

    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);

    // 2. Draw the feature-type icon in center (20x20 area)
    const int offset = (kIconSize - kIconInner) / 2;
    QRect iconRect(offset, offset, kIconInner, kIconInner);
    drawFeatureIcon(p, iconRect);

    // 3. Suppressed overlay: red "X"
    if (m_suppressed) {
        p.setOpacity(opacity * 0.85);
        p.setPen(QPen(QColor(220, 60, 60), 2.5));
        p.drawLine(6, 6, kIconSize - 6, kIconSize - 6);
        p.drawLine(kIconSize - 6, 6, 6, kIconSize - 6);
        p.setOpacity(opacity);
    }

    // 4. Error overlay: small orange warning triangle in bottom-right
    if (m_error) {
        p.setOpacity(1.0);  // always fully visible
        QPainterPath tri;
        tri.moveTo(kIconSize - 5, kIconSize - 12);
        tri.lineTo(kIconSize - 12, kIconSize - 3);
        tri.lineTo(kIconSize + 2, kIconSize - 3);
        tri.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 160, 40));
        p.drawPath(tri);
        // Exclamation mark
        p.setPen(QPen(QColor(40, 30, 10), 1.5));
        const qreal cx = kIconSize - 5;
        p.drawLine(QPointF(cx, kIconSize - 10), QPointF(cx, kIconSize - 6));
        p.drawPoint(QPointF(cx, kIconSize - 4.5));
        p.setOpacity(opacity);
    }

    // 6. Editing border: bright blue glow
    if (m_editing) {
        p.setOpacity(1.0);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0, 149, 255), 2.0));
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);
        p.setOpacity(opacity);
    }

    // 7. Selected border: light blue
    if (m_selected && !m_editing) {
        p.setOpacity(1.0);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(100, 160, 255), 1.5));
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);
        p.setOpacity(opacity);
    }
}

// Draw a feature-type icon scaled to fit in iconRect (20x20).
// Uses the same icon drawing logic as FeatureTree::featureIcon but scaled.
void TimelineIconWidget::drawFeatureIcon(QPainter& p, const QRect& iconRect)
{
    p.save();
    p.translate(iconRect.topLeft());
    const qreal s = iconRect.width() / 16.0;  // scale from 16x16 to 20x20
    p.scale(s, s);

    switch (m_featureType) {
    case features::FeatureType::Extrude: {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(80, 160, 255));
        p.drawRect(6, 5, 4, 9);
        QPainterPath tri;
        tri.moveTo(8, 1);
        tri.lineTo(3, 7);
        tri.lineTo(13, 7);
        tri.closeSubpath();
        p.drawPath(tri);
        break;
    }
    case features::FeatureType::Revolve: {
        p.setPen(QPen(QColor(80, 200, 80), 2));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRect(2, 2, 12, 12), 30 * 16, 270 * 16);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(80, 200, 80));
        QPainterPath tip;
        tip.moveTo(11, 2);
        tip.lineTo(14, 5);
        tip.lineTo(9, 5);
        tip.closeSubpath();
        p.drawPath(tip);
        break;
    }
    case features::FeatureType::Fillet: {
        p.setPen(QPen(QColor(255, 160, 60), 2.5));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRect(-2, -2, 16, 16), 0, 90 * 16);
        p.setPen(QPen(QColor(255, 160, 60), 1.2));
        p.drawLine(2, 14, 2, 6);
        p.drawLine(2, 14, 10, 14);
        break;
    }
    case features::FeatureType::Chamfer: {
        p.setPen(QPen(QColor(255, 140, 40), 1.5));
        p.drawLine(2, 14, 2, 7);
        p.drawLine(2, 7, 9, 14);
        p.drawLine(9, 14, 14, 14);
        break;
    }
    case features::FeatureType::Sketch: {
        p.setPen(QPen(QColor(180, 100, 220), 1.8));
        p.drawLine(3, 13, 13, 3);
        p.drawLine(13, 3, 11, 1);
        p.drawLine(11, 1, 1, 11);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(180, 100, 220));
        QPainterPath nib;
        nib.moveTo(1, 11);
        nib.lineTo(3, 13);
        nib.lineTo(1, 15);
        nib.closeSubpath();
        p.drawPath(nib);
        break;
    }
    case features::FeatureType::Hole: {
        p.setPen(QPen(QColor(220, 60, 60), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRect(2, 2, 12, 12));
        p.setBrush(QColor(220, 60, 60, 80));
        p.drawEllipse(QRect(5, 5, 6, 6));
        break;
    }
    case features::FeatureType::Shell: {
        p.setPen(QPen(QColor(80, 200, 200), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRect(2, 2, 12, 12);
        p.drawRect(5, 5, 6, 6);
        break;
    }
    case features::FeatureType::Mirror: {
        p.setPen(QPen(QColor(100, 180, 255), 1, Qt::DashLine));
        p.drawLine(8, 1, 8, 15);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(100, 180, 255));
        p.drawRect(2, 5, 4, 6);
        p.setBrush(QColor(100, 180, 255, 120));
        p.drawRect(10, 5, 4, 6);
        break;
    }
    case features::FeatureType::Sweep: {
        p.setPen(QPen(QColor(80, 200, 180), 2));
        p.setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(2, 12);
        path.cubicTo(4, 4, 12, 4, 14, 12);
        p.drawPath(path);
        p.setBrush(QColor(80, 200, 180));
        p.drawEllipse(QPoint(2, 12), 2, 2);
        break;
    }
    case features::FeatureType::Loft: {
        p.setPen(QPen(QColor(80, 200, 180), 1.5));
        p.drawLine(4, 3, 12, 3);
        p.drawLine(2, 13, 14, 13);
        p.setPen(QPen(QColor(80, 200, 180, 120), 1, Qt::DashLine));
        p.drawLine(4, 3, 2, 13);
        p.drawLine(12, 3, 14, 13);
        break;
    }
    case features::FeatureType::Thread: {
        p.setPen(QPen(QColor(180, 140, 80), 1.5));
        for (int y = 3; y <= 13; y += 3)
            p.drawLine(4, y, 12, y + 1);
        break;
    }
    case features::FeatureType::Combine: {
        p.setPen(QPen(QColor(220, 200, 60), 1.5));
        p.setBrush(QColor(220, 200, 60, 60));
        p.drawRect(1, 1, 9, 9);
        p.drawRect(6, 6, 9, 9);
        break;
    }
    case features::FeatureType::Move: {
        p.setPen(QPen(QColor(140, 170, 210), 2));
        p.drawLine(8, 2, 8, 14);
        p.drawLine(2, 8, 14, 8);
        break;
    }
    case features::FeatureType::Scale: {
        p.setPen(QPen(QColor(160, 120, 200), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRect(1, 1, 14, 14);
        p.drawRect(4, 4, 8, 8);
        break;
    }
    case features::FeatureType::Draft: {
        p.setPen(QPen(QColor(255, 160, 60), 1.5));
        p.setBrush(QColor(255, 160, 60, 40));
        QPolygonF trap;
        trap << QPointF(4, 2) << QPointF(12, 2) << QPointF(14, 14) << QPointF(2, 14);
        p.drawPolygon(trap);
        break;
    }
    case features::FeatureType::Joint: {
        p.setPen(QPen(QColor(80, 200, 80), 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRect(1, 1, 8, 8));
        p.drawEllipse(QRect(7, 7, 8, 8));
        break;
    }
    case features::FeatureType::Coil: {
        p.setPen(QPen(QColor(200, 160, 100), 2));
        p.setBrush(Qt::NoBrush);
        QPainterPath coil;
        coil.moveTo(4, 14);
        coil.cubicTo(2, 10, 14, 10, 12, 6);
        coil.cubicTo(10, 2, 4, 4, 6, 8);
        p.drawPath(coil);
        break;
    }
    case features::FeatureType::RectangularPattern:
    case features::FeatureType::CircularPattern:
    case features::FeatureType::PathPattern: {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(100, 220, 210));
        p.drawRect(2, 2, 4, 4);
        p.drawRect(8, 2, 4, 4);
        p.drawRect(2, 8, 4, 4);
        p.setBrush(QColor(100, 220, 210, 100));
        p.drawRect(8, 8, 4, 4);
        break;
    }
    default: {
        // Generic feature: gray diamond
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(160, 165, 170));
        QPolygonF diamond;
        diamond << QPointF(8, 1) << QPointF(15, 8) << QPointF(8, 15) << QPointF(1, 8);
        p.drawPolygon(diamond);
        break;
    }
    }

    p.restore();
}

// ====================================================================
// TimelineMarker -- thin bright-blue vertical bar
// ====================================================================

TimelineMarker::TimelineMarker(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(10);   // 4px visible bar + some padding for grab area
    setCursor(Qt::SizeHorCursor);
}

void TimelineMarker::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int cx = width() / 2;
    const QColor markerColor(0, 0x95, 0xFF);  // #0095FF

    // Vertical bar (4px wide)
    p.setPen(Qt::NoPen);
    p.setBrush(markerColor);
    p.drawRect(cx - 2, 8, 4, height() - 8);

    // Small downward-pointing triangle at top
    QPainterPath tri;
    tri.moveTo(cx, 8);
    tri.lineTo(cx - 5, 0);
    tri.lineTo(cx + 5, 0);
    tri.closeSubpath();
    p.drawPath(tri);
}

// ====================================================================
// TimelinePanel
// ====================================================================

TimelinePanel::TimelinePanel(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(48);
    setAcceptDrops(true);

    // Very dark background (#1e1e1e)
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0x1e, 0x1e, 0x1e));
    setPalette(pal);

    // Outer layout holds the scroll area
    auto* outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet(
        "QScrollArea { background: transparent; }"
        "QScrollBar:horizontal { height: 6px; background: #1e1e1e; }"
        "QScrollBar::handle:horizontal { background: #555; border-radius: 3px; min-width: 20px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }");

    m_container = new QWidget;
    m_container->setAutoFillBackground(false);
    m_entryLayout = new QHBoxLayout(m_container);
    m_entryLayout->setContentsMargins(6, 7, 6, 5);  // top padding for group brackets
    m_entryLayout->setSpacing(2);
    m_entryLayout->addStretch();

    m_scrollArea->setWidget(m_container);
    outerLayout->addWidget(m_scrollArea);

    // Marker (initially hidden until entries arrive)
    m_marker = new TimelineMarker(m_container);
    m_marker->installEventFilter(this);
    m_marker->hide();
}

// ----- public API ----------------------------------------------------

void TimelinePanel::setEntriesEx(const std::vector<EntryInfo>& entries)
{
    m_data.clear();
    m_data.reserve(entries.size());
    for (const auto& ei : entries) {
        EntryData ed;
        ed.id          = ei.id;
        ed.name        = ei.displayName;
        ed.tooltip     = ei.tooltip;
        ed.iconColor   = ei.iconColor;
        ed.suppressed  = ei.suppressed;
        ed.hasError    = ei.hasError;
        ed.featureType = ei.featureType;
        m_data.push_back(std::move(ed));
    }

    // Clamp marker
    m_markerIndex = std::min(m_markerIndex, static_cast<int>(m_data.size()));

    rebuildLayout();
}

void TimelinePanel::setEntries(const std::vector<std::pair<QString, QString>>& entries)
{
    m_data.clear();
    m_data.reserve(entries.size());
    for (const auto& [id, name] : entries) {
        EntryData ed;
        ed.id   = id;
        ed.name = name;
        ed.iconColor = colorForName(name);
        m_data.push_back(std::move(ed));
    }

    // Clamp marker
    m_markerIndex = std::min(m_markerIndex, static_cast<int>(m_data.size()));

    rebuildLayout();
}

void TimelinePanel::setGroups(const std::vector<GroupInfo>& groups)
{
    m_groups = groups;
    rebuildLayout();
}

void TimelinePanel::setMarkerPosition(int index)
{
    index = std::clamp(index, 0, static_cast<int>(m_data.size()));
    if (m_markerIndex != index) {
        m_markerIndex = index;
        rebuildLayout();
    }
}

void TimelinePanel::setEditingFeatureId(const QString& featureId)
{
    if (m_editingFeatureId != featureId) {
        m_editingFeatureId = featureId;
        // Update editing state on existing widgets
        for (auto* ew : m_entryWidgets) {
            ew->setEditing(ew->featureId() == m_editingFeatureId);
        }
    }
}

void TimelinePanel::refresh()
{
    rebuildLayout();
}

// ----- drag & drop ---------------------------------------------------

void TimelinePanel::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat("application/x-timeline-entry") ||
        event->mimeData()->hasFormat("application/x-timeline-marker")) {
        event->acceptProposedAction();
    }
}

void TimelinePanel::dragMoveEvent(QDragMoveEvent* event)
{
    event->acceptProposedAction();

    // Show drag insertion indicator for entry drags
    if (event->mimeData()->hasFormat("application/x-timeline-entry")) {
        QPoint posInContainer = m_container->mapFrom(this, event->position().toPoint());
        int newInsertIdx = entryIndexAtPos(posInContainer);
        newInsertIdx = std::clamp(newInsertIdx, 0, static_cast<int>(m_data.size()));
        if (newInsertIdx != m_dragInsertIndex) {
            m_dragInsertIndex = newInsertIdx;
            m_container->update();
            update();
        }
    }
}

void TimelinePanel::dragLeaveEvent(QDragLeaveEvent* event)
{
    Q_UNUSED(event);
    m_dragInsertIndex = -1;
    m_container->update();
    update();
}

void TimelinePanel::dropEvent(QDropEvent* event)
{
    // Clear insertion indicator
    m_dragInsertIndex = -1;
    m_container->update();

    if (event->mimeData()->hasFormat("application/x-timeline-marker")) {
        // Marker drag -- reposition marker
        QPoint posInContainer = m_container->mapFrom(this, event->position().toPoint());
        int newIndex = entryIndexAtPos(posInContainer);
        newIndex = std::clamp(newIndex, 0, static_cast<int>(m_data.size()));
        if (newIndex != m_markerIndex) {
            m_markerIndex = newIndex;
            rebuildLayout();
            emit markerMoved(m_markerIndex);
        }
        event->acceptProposedAction();
        return;
    }

    if (event->mimeData()->hasFormat("application/x-timeline-entry")) {
        int fromIndex = event->mimeData()->data("application/x-timeline-entry").toInt();
        QPoint posInContainer = m_container->mapFrom(this, event->position().toPoint());
        int toIndex = entryIndexAtPos(posInContainer);
        toIndex = std::clamp(toIndex, 0, static_cast<int>(m_data.size()) - 1);

        if (fromIndex != toIndex && fromIndex >= 0 &&
            fromIndex < static_cast<int>(m_data.size())) {
            emit reorderRequested(fromIndex, toIndex);
        }
        event->acceptProposedAction();
    }
}

// ----- paint event (drag insertion line + group brackets) -------------

void TimelinePanel::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Draw group brackets
    paintGroupBrackets(p);

    // Draw drag insertion indicator
    if (m_dragInsertIndex >= 0)
        paintInsertionLine(p);
}

void TimelinePanel::paintInsertionLine(QPainter& p)
{
    int xPos = -1;

    if (m_entryWidgets.empty()) {
        xPos = 10;
    } else if (m_dragInsertIndex >= static_cast<int>(m_entryWidgets.size())) {
        QWidget* last = m_entryWidgets.back();
        if (last->isVisible()) {
            QPoint pt = last->mapTo(this, QPoint(last->width() + 1, 0));
            xPos = pt.x();
        }
    } else {
        QWidget* w = m_entryWidgets[m_dragInsertIndex];
        if (w->isVisible()) {
            QPoint pt = w->mapTo(this, QPoint(-1, 0));
            xPos = pt.x();
        }
    }

    if (xPos >= 0) {
        p.setPen(QPen(QColor(0, 140, 255), 2));
        p.drawLine(xPos, 2, xPos, height() - 2);

        QPainterPath tri;
        tri.moveTo(xPos, 2);
        tri.lineTo(xPos - 4, 0);
        tri.lineTo(xPos + 4, 0);
        tri.closeSubpath();
        p.setBrush(QColor(0, 140, 255));
        p.setPen(Qt::NoPen);
        p.drawPath(tri);
    }
}

void TimelinePanel::paintGroupBrackets(QPainter& p)
{
    if (m_groups.empty() || m_entryWidgets.empty())
        return;

    for (const auto& grp : m_groups) {
        int si = grp.startIndex;
        int ei = grp.endIndex;
        if (si < 0 || ei < 0) continue;
        if (si >= static_cast<int>(m_entryWidgets.size())) continue;
        ei = std::min(ei, static_cast<int>(m_entryWidgets.size()) - 1);

        QWidget* first = m_entryWidgets[si];
        QWidget* last  = m_entryWidgets[ei];
        if (!first->isVisible() || !last->isVisible()) continue;

        QPoint p1 = first->mapTo(this, QPoint(0, 0));
        QPoint p2 = last->mapTo(this, QPoint(last->width(), 0));

        const int bracketY = 3;
        QColor bracketColor = grp.isSuppressed ? QColor(180, 100, 100) : QColor(0x5c, 0x8a, 0xbf);

        // Horizontal line
        p.setPen(QPen(bracketColor, 1.5));
        p.drawLine(p1.x(), bracketY, p2.x(), bracketY);

        // Small vertical ticks at ends
        p.drawLine(p1.x(), bracketY, p1.x(), bracketY + 3);
        p.drawLine(p2.x(), bracketY, p2.x(), bracketY + 3);

        // Group name centered above the line
        QFont f = font();
        f.setPixelSize(8);
        f.setBold(true);
        p.setFont(f);
        p.setPen(bracketColor);

        QString label = grp.isCollapsed ? (grp.name + " [...]") : grp.name;
        int textWidth = p.fontMetrics().horizontalAdvance(label);
        int centerX = (p1.x() + p2.x()) / 2 - textWidth / 2;
        // Draw text slightly above the bracket, but make sure it stays on-screen
        // We draw at bracketY - 1 which is at y=2, small but visible
        p.drawText(centerX, bracketY - 1, label);
    }
}

// ----- context menu (right-click to create group) --------------------

void TimelinePanel::contextMenuEvent(QContextMenuEvent* event)
{
    QPoint posInContainer = m_container->mapFrom(this, event->pos());
    int clickedIdx = entryIndexAtPos(posInContainer);
    if (clickedIdx < 0 || clickedIdx >= static_cast<int>(m_data.size())) {
        QWidget::contextMenuEvent(event);
        return;
    }

    // Check if clicked entry belongs to a group
    const GroupInfo* clickedGroup = nullptr;
    for (const auto& g : m_groups) {
        if (clickedIdx >= g.startIndex && clickedIdx <= g.endIndex) {
            clickedGroup = &g;
            break;
        }
    }

    QMenu menu(this);

    if (clickedGroup) {
        menu.addAction(clickedGroup->isCollapsed ? "Expand" : "Collapse", this,
                        [this, gid = clickedGroup->groupId]() {
            emit groupCollapseToggled(gid);
        });
        menu.addAction("Ungroup", this, [this, gid = clickedGroup->groupId]() {
            emit ungroupRequested(gid);
        });
        if (clickedGroup->isSuppressed) {
            menu.addAction("Unsuppress Group", this,
                            [this, gid = clickedGroup->groupId]() {
                emit groupSuppressToggled(gid, false);
            });
        } else {
            menu.addAction("Suppress Group", this,
                            [this, gid = clickedGroup->groupId]() {
                emit groupSuppressToggled(gid, true);
            });
        }
        menu.addSeparator();
    }

    // Group action
    if (m_data.size() >= 2) {
        int startIdx = std::max(0, clickedIdx - 1);
        int endIdx   = std::min(static_cast<int>(m_data.size()) - 1, clickedIdx);
        if (startIdx == endIdx && endIdx + 1 < static_cast<int>(m_data.size()))
            endIdx = startIdx + 1;

        menu.addAction(
            QString("Group entries %1-%2").arg(startIdx).arg(endIdx),
            this, [this, startIdx, endIdx]() {
                emit groupRequested(startIdx, endIdx);
            });
    }
    menu.exec(event->globalPos());
}

// ----- helpers -------------------------------------------------------

QColor TimelinePanel::colorForName(const QString& name)
{
    const uint hash = qHash(name);
    static const QColor palette[] = {
        {100, 180, 255},  // blue
        {130, 220, 130},  // green
        {255, 180,  80},  // orange
        {220, 120, 220},  // purple
        {255, 120, 120},  // red
        {100, 220, 210},  // teal
        {240, 210, 100},  // yellow
        {180, 140, 100},  // brown
    };
    return palette[hash % 8];
}

void TimelinePanel::rebuildLayout()
{
    // Remove all items from layout (except the stretch)
    while (m_entryLayout->count() > 0) {
        QLayoutItem* item = m_entryLayout->takeAt(0);
        if (item->widget() && item->widget() != m_marker)
            item->widget()->deleteLater();
        delete item;
    }
    m_entryWidgets.clear();
    m_marker->setParent(m_container);

    // Re-add entries with marker inserted at m_markerIndex
    for (int i = 0; i < static_cast<int>(m_data.size()); ++i) {
        if (i == m_markerIndex) {
            m_entryLayout->addWidget(m_marker);
            m_marker->show();
        }

        const auto& ed = m_data[i];

        // Determine feature type for icon drawing
        features::FeatureType ftype = ed.featureType.value_or(features::FeatureType::BaseFeature);
        QColor col = ed.iconColor.isValid() ? ed.iconColor : colorForName(ed.name);

        auto* ew = new TimelineIconWidget(ed.id, ftype, col, ed.name, m_container);

        // Check if this entry is in a suppressed group
        bool groupSuppressed = false;
        for (const auto& g : m_groups) {
            if (i >= g.startIndex && i <= g.endIndex && g.isSuppressed) {
                groupSuppressed = true;
                break;
            }
        }

        ew->setSuppressed(ed.suppressed || groupSuppressed);
        ew->setDimmed(i >= m_markerIndex);
        ew->setError(ed.hasError);
        ew->setEditing(ed.id == m_editingFeatureId);

        // Set rich tooltip if provided
        if (!ed.tooltip.isEmpty())
            ew->setToolTip(ed.tooltip);

        connect(ew, &TimelineIconWidget::doubleClicked, this, [this](const QString& fid) {
            emit entryDoubleClicked(fid);
        });

        const int entryIndex = i;
        ew->installEventFilter(this);
        ew->setProperty("_tl_index", entryIndex);

        // Handle collapsed groups: hide individual icons, show a placeholder
        bool hidden = false;
        for (const auto& g : m_groups) {
            if (i >= g.startIndex && i <= g.endIndex && g.isCollapsed) {
                if (i == g.startIndex) {
                    // Show first icon as representative with "..." badge
                    ew->setToolTip(g.name + " (collapsed)");
                    m_entryLayout->addWidget(ew);
                } else {
                    ew->setVisible(false);
                    ew->setParent(m_container);
                    hidden = true;
                }
                break;
            }
        }

        if (!hidden) {
            m_entryLayout->addWidget(ew);
        }

        m_entryWidgets.push_back(ew);
    }

    // Marker at end (after all entries)
    if (m_markerIndex >= static_cast<int>(m_data.size())) {
        m_entryLayout->addWidget(m_marker);
        m_marker->show();
    }

    if (m_data.empty())
        m_marker->hide();

    m_entryLayout->addStretch();
}

void TimelinePanel::updateDimming()
{
    for (int i = 0; i < static_cast<int>(m_entryWidgets.size()); ++i) {
        m_entryWidgets[i]->setDimmed(i >= m_markerIndex);
    }
}

int TimelinePanel::entryIndexAtPos(const QPoint& pos) const
{
    if (m_entryWidgets.empty())
        return 0;

    for (int i = 0; i < static_cast<int>(m_entryWidgets.size()); ++i) {
        QWidget* w = m_entryWidgets[i];
        if (!w->isVisible()) continue;
        int center = w->mapTo(m_container, QPoint(w->width() / 2, 0)).x();
        if (pos.x() < center)
            return i;
    }
    return static_cast<int>(m_entryWidgets.size());
}

// Override eventFilter to start drag on mouse-press for entry widgets and marker
bool TimelinePanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            // Check if it is the marker
            if (watched == m_marker) {
                auto* drag = new QDrag(this);
                auto* mimeData = new QMimeData;
                mimeData->setData("application/x-timeline-marker", QByteArray());
                drag->setMimeData(mimeData);
                drag->exec(Qt::MoveAction);
                return true;
            }
            // Check if it is an entry widget
            auto* ew = qobject_cast<TimelineIconWidget*>(watched);
            if (ew) {
                QVariant idx = ew->property("_tl_index");
                if (idx.isValid()) {
                    auto* drag = new QDrag(this);
                    auto* mimeData = new QMimeData;
                    mimeData->setData("application/x-timeline-entry",
                                      QByteArray::number(idx.toInt()));
                    drag->setMimeData(mimeData);
                    ew->setCursor(Qt::ClosedHandCursor);
                    drag->exec(Qt::MoveAction);
                    ew->setCursor(Qt::OpenHandCursor);
                    // Clear insertion indicator after drop
                    m_dragInsertIndex = -1;
                    update();
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
