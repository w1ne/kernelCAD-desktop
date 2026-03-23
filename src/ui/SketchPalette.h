#pragma once
#include <QWidget>
#include <QString>
#include <QPoint>

class QCheckBox;
class QLabel;
class QPushButton;
class QVBoxLayout;
class SketchEditor;

namespace sketch { class Sketch; }

/// Floating palette that appears over the viewport during sketch editing.
/// Shows display toggles, sketch statistics, and a Finish Sketch button.
class SketchPalette : public QWidget
{
    Q_OBJECT
public:
    explicit SketchPalette(QWidget* parent = nullptr);

    /// Show the palette for the given sketch and editor.
    void showForSketch(sketch::Sketch* sketch, SketchEditor* editor);

    /// Hide and detach from the current sketch.
    void dismiss();

    /// Update the sketch statistics (entity counts, DOF).
    void refresh();

    /// Reposition the palette on the right side of the parent viewport.
    void repositionOverParent();

signals:
    void finishSketchClicked();
    void settingChanged(const QString& name, bool value);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void buildUI();
    void applyStyleSheet();

    sketch::Sketch* m_sketch = nullptr;
    SketchEditor*   m_editor = nullptr;

    QVBoxLayout* m_rootLayout = nullptr;

    // Options
    QCheckBox* m_snapToGrid     = nullptr;
    QCheckBox* m_showInference  = nullptr;

    // Display
    QCheckBox* m_showPoints       = nullptr;
    QCheckBox* m_showDimensions   = nullptr;
    QCheckBox* m_showConstraints  = nullptr;
    QCheckBox* m_showConstruction = nullptr;

    // Sketch Info
    QLabel* m_pointCount      = nullptr;
    QLabel* m_lineCount       = nullptr;
    QLabel* m_constraintCount = nullptr;
    QLabel* m_dofLabel        = nullptr;

    // Finish
    QPushButton* m_finishButton = nullptr;

    // Drag support
    bool m_dragging = false;
    QPoint m_dragOffset;
};
