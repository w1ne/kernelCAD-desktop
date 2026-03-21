#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QMouseEvent>
#include <QMenu>
#include <QString>

#include "../features/Feature.h"

#include <vector>
#include <optional>
#include <utility>

// Forward declaration
namespace document { struct TimelineGroup; }

// --------------------------------------------------------------------
// TimelineIconWidget -- compact 34x34 icon representing one feature
// --------------------------------------------------------------------
class TimelineIconWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TimelineIconWidget(const QString& featureId,
                                features::FeatureType type,
                                const QColor& iconColor,
                                const QString& name,
                                QWidget* parent = nullptr);

    QString featureId()   const { return m_featureId; }
    QString featureName() const { return m_name; }

    void setDimmed(bool d);
    void setSuppressed(bool s);
    void setError(bool e);
    void setEditing(bool e);
    void setSelected(bool s);

signals:
    void doubleClicked(const QString& featureId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void drawFeatureIcon(QPainter& p, const QRect& iconRect);

    QString m_featureId;
    features::FeatureType m_featureType;
    QColor  m_iconColor;
    QString m_name;
    bool    m_dimmed    = false;
    bool    m_suppressed = false;
    bool    m_error     = false;
    bool    m_editing   = false;
    bool    m_selected  = false;
    bool    m_hovered   = false;
};

// --------------------------------------------------------------------
// TimelineMarker -- thin bright-blue draggable vertical bar
// --------------------------------------------------------------------
class TimelineMarker : public QWidget
{
    Q_OBJECT
public:
    explicit TimelineMarker(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

// --------------------------------------------------------------------
// TimelinePanel -- horizontal, scrollable timeline with drag-reorder
// --------------------------------------------------------------------
class TimelinePanel : public QWidget
{
    Q_OBJECT
public:
    explicit TimelinePanel(QWidget* parent = nullptr);

    /// Extended entry info for richer timeline display.
    struct EntryInfo {
        QString id;
        QString displayName;
        QString tooltip;       ///< Rich tooltip text (HTML supported)
        QColor  iconColor;     ///< Feature-type-based icon colour
        bool    suppressed = false;
        bool    hasError   = false;
        std::optional<features::FeatureType> featureType;
    };

    /// Populate the panel with full entry info (preferred).
    void setEntriesEx(const std::vector<EntryInfo>& entries);

    /// Legacy overload: populate the panel with (id, name) pairs.
    void setEntries(const std::vector<std::pair<QString, QString>>& entries);

    /// Set groups metadata (read from document timeline).
    struct GroupInfo {
        QString groupId;
        QString name;
        int     startIndex = 0;
        int     endIndex   = 0;
        bool    isCollapsed  = false;
        bool    isSuppressed = false;
    };
    void setGroups(const std::vector<GroupInfo>& groups);

    /// Move the marker to sit before the entry at @p index.
    /// index == entryCount  =>  marker at the very end (nothing rolled back).
    void setMarkerPosition(int index);

    int markerPosition() const { return m_markerIndex; }

    /// Set the feature ID currently being edited (highlighted with a blue border).
    /// Pass an empty string to clear the editing highlight.
    void setEditingFeatureId(const QString& featureId);

    void refresh();

signals:
    void markerMoved(int index);
    void entryDoubleClicked(const QString& featureId);

    /// Emitted when the user drags to reorder. MainWindow validates via depGraph.
    void reorderRequested(int fromIndex, int toIndex);

    /// Emitted when the user right-clicks a selection and chooses "Group".
    void groupRequested(int startIndex, int endIndex);

    /// Emitted when the user requests ungrouping.
    void ungroupRequested(const QString& groupId);

    /// Emitted when the user toggles collapse on a group.
    void groupCollapseToggled(const QString& groupId);

    /// Emitted when the user changes group suppression.
    void groupSuppressToggled(const QString& groupId, bool suppress);

protected:
    void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// Colour hint for a feature name (simple heuristic).
    static QColor colorForName(const QString& name);

    void rebuildLayout();
    void updateDimming();
    int  entryIndexAtPos(const QPoint& pos) const;

    // Data ---------------------------------------------------------
    struct EntryData {
        QString id;
        QString name;
        QString tooltip;
        QColor  iconColor;
        bool    suppressed = false;
        bool    hasError   = false;
        std::optional<features::FeatureType> featureType;
    };

    std::vector<EntryData>              m_data;
    std::vector<GroupInfo>              m_groups;
    std::vector<TimelineIconWidget*>    m_entryWidgets;

    QScrollArea*    m_scrollArea  = nullptr;
    QWidget*        m_container   = nullptr;
    QHBoxLayout*    m_entryLayout = nullptr;
    TimelineMarker* m_marker      = nullptr;
    int             m_markerIndex = 0;
    QString         m_editingFeatureId;

    /// Drag insertion indicator: -1 = not showing
    int m_dragInsertIndex = -1;

    /// Draw the drag insertion indicator line on the container.
    void paintInsertionLine(QPainter& p);

    /// Draw group bracket lines above icons.
    void paintGroupBrackets(QPainter& p);

protected:
    void paintEvent(QPaintEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
};
