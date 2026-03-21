#pragma once
#include <QWidget>
#include <QPoint>
#include <QIcon>
#include <QString>
#include <functional>
#include <vector>

/// Radial marking menu shown on right-click-and-hold in the viewport.
/// Up to 8 items arranged at compass positions: N, NE, E, SE, S, SW, W, NW.
/// The user drags toward the desired item direction and releases to execute.
class MarkingMenu : public QWidget
{
    Q_OBJECT
public:
    explicit MarkingMenu(QWidget* parent = nullptr);

    struct MenuItem {
        QString text;
        QString shortcut;
        QIcon icon;
        std::function<void()> action;
    };

    /// Set the menu items arranged in a circle (max 8).
    void setItems(const std::vector<MenuItem>& items);

    /// Show the radial menu centered at the given global screen position.
    void showAt(const QPoint& globalPos);

    /// Hide and execute the item under the cursor (if any).
    void commitSelection();

signals:
    void itemTriggered(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    std::vector<MenuItem> m_items;
    QPoint m_center;           // center of the menu in widget coords
    int m_hoveredIndex = -1;
    int m_innerRadius  = 40;
    int m_outerRadius  = 130;

    /// Fade-in animation progress (0.0 .. 1.0).
    float m_animProgress = 0.0f;
    class QTimer* m_animTimer = nullptr;

    /// Determine which item sector the given widget-local position falls in.
    /// Returns -1 if in the dead zone (center) or outside the outer radius.
    int itemAtPos(const QPoint& pos) const;
};
