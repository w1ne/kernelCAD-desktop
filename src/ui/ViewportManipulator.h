#pragma once
#include <QObject>
#include <QVector3D>
#include <QMatrix4x4>
#include <QPoint>
#include <QPointF>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>

class QPainter;

enum class ManipulatorType { Distance, Angle, Direction };

/// Viewport drag manipulator for interactive feature editing.
/// Draws a draggable arrow (distance) or arc (angle) in the 3D viewport
/// and emits value changes as the user drags.
class ViewportManipulator : public QObject
{
    Q_OBJECT
public:
    explicit ViewportManipulator(QObject* parent = nullptr);
    ~ViewportManipulator() override;

    /// Show a distance manipulator (arrow along direction).
    void showDistance(const QVector3D& origin, const QVector3D& direction,
                     double currentValue, double minValue = 0, double maxValue = 1000);

    /// Show an angle manipulator (arc around axis).
    void showAngle(const QVector3D& origin, const QVector3D& axis,
                   double currentAngleDeg, double minAngle = 0, double maxAngle = 360);

    void hide();
    bool isVisible() const;
    bool isDragging() const;

    /// Call from Viewport3D mouse events to check if manipulator handles the input.
    bool handleMousePress(const QPoint& screenPos,
                          const QMatrix4x4& view, const QMatrix4x4& proj,
                          int viewportW, int viewportH);
    bool handleMouseMove(const QPoint& screenPos,
                         const QMatrix4x4& view, const QMatrix4x4& proj,
                         int viewportW, int viewportH);
    bool handleMouseRelease();

    /// Draw the manipulator geometry using OpenGL.
    /// Call from Viewport3D::paintGL after drawing bodies.
    void draw(QOpenGLFunctions_3_3_Core* gl,
              QOpenGLShaderProgram* edgeProgram,
              const QMatrix4x4& view, const QMatrix4x4& proj);

    /// Draw the value label and flip arrow overlay using QPainter.
    /// Call after QPainter is set up on the viewport widget.
    void drawOverlay(QPainter& painter,
                     const QMatrix4x4& view, const QMatrix4x4& proj,
                     int viewportW, int viewportH);

    /// Toggle direction (positive <-> negative) for the direction flip arrow.
    void flipDirection();

    /// Get the current manipulated value.
    double currentValue() const;

    /// Get the current direction sign (+1 or -1).
    int directionSign() const;

signals:
    void valueChanged(double newValue);
    void dragStarted();
    void dragFinished(double finalValue);
    void directionFlipped(int newSign);

private:
    /// Project a 3D world point to 2D screen coordinates.
    QPointF worldToScreen(const QVector3D& worldPt,
                          const QMatrix4x4& view, const QMatrix4x4& proj,
                          int viewportW, int viewportH) const;

    /// Compute the screen-space signed distance along the manipulator axis.
    double screenProjection(const QPoint& screenPos,
                            const QMatrix4x4& view, const QMatrix4x4& proj,
                            int viewportW, int viewportH) const;

    /// Test whether a screen point is near the arrow tip handle.
    bool hitTestArrowTip(const QPoint& screenPos,
                         const QMatrix4x4& view, const QMatrix4x4& proj,
                         int viewportW, int viewportH,
                         float threshold = 20.0f) const;

    /// Test whether a screen point is near the flip arrow icon.
    bool hitTestFlipArrow(const QPoint& screenPos,
                          const QMatrix4x4& view, const QMatrix4x4& proj,
                          int viewportW, int viewportH,
                          float threshold = 18.0f) const;

    // State
    bool m_visible = false;
    bool m_dragging = false;
    bool m_hovering = false;     ///< Mouse is over the handle
    ManipulatorType m_type = ManipulatorType::Distance;

    // Geometry
    QVector3D m_origin;
    QVector3D m_direction;       ///< Normalized direction vector
    int m_directionSign = 1;     ///< +1 = positive, -1 = negative

    // Value
    double m_currentValue = 0;
    double m_minValue = 0;
    double m_maxValue = 1000;

    // Drag state
    QPoint m_dragStartScreen;
    double m_dragStartValue = 0;
    double m_dragStartProjection = 0;

    // GPU buffers for the arrow geometry (lazily initialized)
    bool m_gpuInitialized = false;
    QOpenGLVertexArrayObject m_arrowVao;
    QOpenGLBuffer m_arrowVbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject m_headVao;
    QOpenGLBuffer m_headVbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject m_baseVao;
    QOpenGLBuffer m_baseVbo{QOpenGLBuffer::VertexBuffer};

    void ensureGPUBuffers(QOpenGLFunctions_3_3_Core* gl);
    void updateArrowGeometry(QOpenGLFunctions_3_3_Core* gl);
};
