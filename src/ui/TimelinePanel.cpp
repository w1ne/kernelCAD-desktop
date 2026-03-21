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
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <map>

// ====================================================================
// TimelineEntryWidget
// ====================================================================

TimelineEntryWidget::TimelineEntryWidget(const QString& featureId,
                                         const QString& featureName,
                                         const QColor&  iconColor,
                                         QWidget* parent)
    : QWidget(parent)
    , m_featureId(featureId)
    , m_featureName(featureName)
    , m_iconColor(iconColor)
{
    setFixedSize(90, 56);
    setCursor(Qt::OpenHandCursor);
    // Default tooltip -- callers may override with setToolTip()
    setToolTip(featureName);
}

void TimelineEntryWidget::setDimmed(bool dimmed)
{
    if (m_dimmed != dimmed) {
        m_dimmed = dimmed;
        update();
    }
}

void TimelineEntryWidget::setSuppressed(bool suppressed)
{
    if (m_suppressed != suppressed) {
        m_suppressed = suppressed;
        update();
    }
}

void TimelineEntryWidget::setSelected(bool selected)
{
    if (m_selected != selected) {
        m_selected = selected;
        update();
    }
}

void TimelineEntryWidget::setEditing(bool editing)
{
    if (m_editing != editing) {
        m_editing = editing;
        update();
    }
}

void TimelineEntryWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    emit doubleClicked(m_featureId);
}

void TimelineEntryWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const qreal opacity = m_dimmed ? 0.35 : 1.0;
    p.setOpacity(opacity);

    // Background rounded rect
    QColor bg(50, 52, 58);
    if (m_selected)
        bg = QColor(70, 90, 120);
    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 5, 5);

    // Selection border
    if (m_selected) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(100, 160, 255), 2));
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 5, 5);
    }

    // Editing border (bright blue, thicker)
    if (m_editing) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0, 160, 255), 2.5));
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 5, 5);
    }

    // Icon placeholder -- colored rectangle at top, using feature-type colour
    QRect iconRect(6, 6, 18, 18);
    p.setBrush(m_iconColor);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(iconRect, 3, 3);

    // Feature name label
    QFont f = font();
    f.setPixelSize(11);
    if (m_suppressed) {
        f.setStrikeOut(true);
        p.setPen(QColor(160, 160, 160));
    } else {
        p.setPen(QColor(220, 220, 225));
    }
    p.setFont(f);

    QRect textRect(4, 28, width() - 8, height() - 30);
    QString elidedName = p.fontMetrics().elidedText(m_featureName, Qt::ElideRight, textRect.width());
    p.drawText(textRect, Qt::AlignLeft | Qt::AlignTop, elidedName);
}

// ====================================================================
// TimelineGroupWidget
// ====================================================================

TimelineGroupWidget::TimelineGroupWidget(const QString& groupId,
                                         const QString& groupName,
                                         bool collapsed,
                                         bool suppressed,
                                         QWidget* parent)
    : QWidget(parent)
    , m_groupId(groupId)
    , m_groupName(groupName)
    , m_collapsed(collapsed)
    , m_suppressed(suppressed)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(4, 14, 4, 4);
    outerLayout->setSpacing(2);

    m_innerLayout = new QHBoxLayout;
    m_innerLayout->setContentsMargins(0, 0, 0, 0);
    m_innerLayout->setSpacing(4);
    outerLayout->addLayout(m_innerLayout);

    setMinimumHeight(70);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void TimelineGroupWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Group border
    QColor borderColor = m_suppressed ? QColor(180, 100, 100) : QColor(80, 130, 200);
    p.setPen(QPen(borderColor, 1.5, Qt::DashLine));
    p.setBrush(QColor(40, 42, 48, 120));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 6, 6);

    // Group name label at top
    QFont f = font();
    f.setPixelSize(10);
    f.setBold(true);
    if (m_suppressed) f.setStrikeOut(true);
    p.setFont(f);
    p.setPen(borderColor);

    QString label = m_collapsed ? (m_groupName + " [...]") : m_groupName;
    p.drawText(QRect(6, 2, width() - 12, 12), Qt::AlignLeft | Qt::AlignVCenter, label);
}

void TimelineGroupWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    menu.addAction(m_collapsed ? "Expand" : "Collapse", this, [this]() {
        emit requestToggleCollapse(m_groupId);
    });
    menu.addAction("Ungroup", this, [this]() {
        emit requestUngroup(m_groupId);
    });
    if (m_suppressed) {
        menu.addAction("Unsuppress Group", this, [this]() {
            emit requestSuppressGroup(m_groupId, false);
        });
    } else {
        menu.addAction("Suppress Group", this, [this]() {
            emit requestSuppressGroup(m_groupId, true);
        });
    }
    menu.exec(event->globalPos());
}

// ====================================================================
// TimelineMarker
// ====================================================================

TimelineMarker::TimelineMarker(QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(14, 60);
    setCursor(Qt::SizeHorCursor);
}

void TimelineMarker::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int cx = width() / 2;

    // Vertical line
    p.setPen(QPen(QColor(0, 160, 255), 2));
    p.drawLine(cx, 10, cx, height());

    // Triangle at top
    QPainterPath tri;
    tri.moveTo(cx, 10);
    tri.lineTo(cx - 6, 0);
    tri.lineTo(cx + 6, 0);
    tri.closeSubpath();
    p.setBrush(QColor(0, 160, 255));
    p.setPen(Qt::NoPen);
    p.drawPath(tri);
}

// ====================================================================
// TimelinePanel
// ====================================================================

TimelinePanel::TimelinePanel(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(80);
    setAcceptDrops(true);

    // Dark background
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(32, 33, 36));
    setPalette(pal);

    // Outer layout holds the scroll area
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(4, 4, 4, 4);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet(
        "QScrollArea { background: transparent; }"
        "QScrollBar:horizontal { height: 8px; background: #222; }"
        "QScrollBar::handle:horizontal { background: #555; border-radius: 4px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }");

    m_container = new QWidget;
    m_container->setAutoFillBackground(false);
    m_entryLayout = new QHBoxLayout(m_container);
    m_entryLayout->setContentsMargins(2, 2, 2, 2);
    m_entryLayout->setSpacing(4);
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
        ed.id         = ei.id;
        ed.name       = ei.displayName;
        ed.tooltip    = ei.tooltip;
        ed.iconColor  = ei.iconColor;
        ed.suppressed = ei.suppressed;
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
            m_container->update();  // trigger repaint to draw line
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
            // Delegate to MainWindow for dependency graph validation
            emit reorderRequested(fromIndex, toIndex);
        }
        event->acceptProposedAction();
    }
}

// ----- paint event (for drag insertion line) -------------------------

void TimelinePanel::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    if (m_dragInsertIndex < 0)
        return;

    // Draw a vertical blue insertion line at the calculated position
    // We need to figure out the x-position in our coordinate system
    int xPos = -1;

    if (m_entryWidgets.empty()) {
        xPos = 10;
    } else if (m_dragInsertIndex >= static_cast<int>(m_entryWidgets.size())) {
        // After the last widget
        QWidget* last = m_entryWidgets.back();
        if (last->isVisible()) {
            QPoint p = last->mapTo(this, QPoint(last->width() + 2, 0));
            xPos = p.x();
        }
    } else {
        QWidget* w = m_entryWidgets[m_dragInsertIndex];
        if (w->isVisible()) {
            QPoint p = w->mapTo(this, QPoint(-2, 0));
            xPos = p.x();
        }
    }

    if (xPos >= 0) {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(0, 140, 255), 3));
        p.drawLine(xPos, 4, xPos, height() - 4);

        // Small triangle at top
        QPainterPath tri;
        tri.moveTo(xPos, 4);
        tri.lineTo(xPos - 5, 0);
        tri.lineTo(xPos + 5, 0);
        tri.closeSubpath();
        p.setBrush(QColor(0, 140, 255));
        p.setPen(Qt::NoPen);
        p.drawPath(tri);
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

    QMenu menu(this);
    // Simple: group the clicked entry with its neighbor(s)
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
    // Simple deterministic colour based on name hash
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

    // Build a lookup: entry index -> group info
    auto groupForIndex = [this](int idx) -> const GroupInfo* {
        for (const auto& g : m_groups) {
            if (idx >= g.startIndex && idx <= g.endIndex)
                return &g;
        }
        return nullptr;
    };

    // Track which groups have already been rendered (to avoid duplicates)
    std::map<QString, TimelineGroupWidget*> renderedGroups;

    // Re-add entries with marker inserted at m_markerIndex
    for (int i = 0; i < static_cast<int>(m_data.size()); ++i) {
        if (i == m_markerIndex) {
            m_entryLayout->addWidget(m_marker);
            m_marker->show();
        }

        const auto& ed = m_data[i];
        const GroupInfo* grp = groupForIndex(i);

        // Use the stored icon colour (falls back to colorForName if not set)
        QColor col = ed.iconColor.isValid() ? ed.iconColor : colorForName(ed.name);
        auto* ew = new TimelineEntryWidget(ed.id, ed.name, col, m_container);
        ew->setSuppressed(ed.suppressed || (grp && grp->isSuppressed));
        ew->setDimmed(i >= m_markerIndex);
        ew->setEditing(ed.id == m_editingFeatureId);

        // Set rich tooltip if provided
        if (!ed.tooltip.isEmpty())
            ew->setToolTip(ed.tooltip);

        connect(ew, &TimelineEntryWidget::doubleClicked, this, [this](const QString& fid) {
            emit entryDoubleClicked(fid);
        });

        const int entryIndex = i;
        ew->installEventFilter(this);
        ew->setProperty("_tl_index", entryIndex);

        if (grp) {
            // Check if this group's widget has been created already
            auto gIt = renderedGroups.find(grp->groupId);
            TimelineGroupWidget* gw = nullptr;
            if (gIt == renderedGroups.end()) {
                gw = new TimelineGroupWidget(grp->groupId, grp->name,
                                             grp->isCollapsed, grp->isSuppressed,
                                             m_container);

                // Connect group signals
                connect(gw, &TimelineGroupWidget::requestUngroup,
                        this, &TimelinePanel::ungroupRequested);
                connect(gw, &TimelineGroupWidget::requestToggleCollapse,
                        this, &TimelinePanel::groupCollapseToggled);
                connect(gw, &TimelineGroupWidget::requestSuppressGroup,
                        this, &TimelinePanel::groupSuppressToggled);

                m_entryLayout->addWidget(gw);
                renderedGroups[grp->groupId] = gw;
            } else {
                gw = gIt->second;
            }

            // If collapsed, only show the group container (hide individual entries)
            if (grp->isCollapsed) {
                ew->setVisible(false);
                ew->setParent(m_container); // keep alive but hidden
            } else {
                gw->innerLayout()->addWidget(ew);
            }
        } else {
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
            auto* ew = qobject_cast<TimelineEntryWidget*>(watched);
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
