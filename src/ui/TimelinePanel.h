#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QMouseEvent>
#include <QMenu>
#include <QString>

#include <vector>
#include <utility>

// Forward declaration
namespace document { struct TimelineGroup; }

// --------------------------------------------------------------------
// TimelineEntryWidget -- a small labeled box representing one feature
// --------------------------------------------------------------------
class TimelineEntryWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TimelineEntryWidget(const QString& featureId,
                                 const QString& featureName,
                                 const QColor&  iconColor,
                                 QWidget* parent = nullptr);

    QString featureId()   const { return m_featureId; }
    QString featureName() const { return m_featureName; }

    void setDimmed(bool dimmed);
    void setSuppressed(bool suppressed);
    void setSelected(bool selected);
    void setEditing(bool editing);

signals:
    void doubleClicked(const QString& featureId);

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_featureId;
    QString m_featureName;
    QColor  m_iconColor;
    bool    m_dimmed     = false;
    bool    m_suppressed = false;
    bool    m_selected   = false;
    bool    m_editing    = false;
};

// --------------------------------------------------------------------
// TimelineGroupWidget -- a container box around grouped entries
// --------------------------------------------------------------------
class TimelineGroupWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TimelineGroupWidget(const QString& groupId,
                                 const QString& groupName,
                                 bool collapsed,
                                 bool suppressed,
                                 QWidget* parent = nullptr);

    QString groupId()   const { return m_groupId; }
    QString groupName() const { return m_groupName; }
    bool    isCollapsed() const { return m_collapsed; }

    /// The inner layout where child entry widgets are placed.
    QHBoxLayout* innerLayout() { return m_innerLayout; }

signals:
    void requestUngroup(const QString& groupId);
    void requestToggleCollapse(const QString& groupId);
    void requestSuppressGroup(const QString& groupId, bool suppress);

protected:
    void paintEvent(QPaintEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    QString      m_groupId;
    QString      m_groupName;
    bool         m_collapsed   = false;
    bool         m_suppressed  = false;
    QHBoxLayout* m_innerLayout = nullptr;
};

// --------------------------------------------------------------------
// TimelineMarker -- draggable vertical-line marker
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

    /// Populate the panel with (id, name) pairs.
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
        bool    suppressed = false;
    };

    std::vector<EntryData>            m_data;
    std::vector<GroupInfo>            m_groups;
    std::vector<TimelineEntryWidget*> m_entryWidgets;

    QScrollArea*    m_scrollArea  = nullptr;
    QWidget*        m_container   = nullptr;
    QHBoxLayout*    m_entryLayout = nullptr;
    TimelineMarker* m_marker      = nullptr;
    int             m_markerIndex = 0;
    QString         m_editingFeatureId;
};
