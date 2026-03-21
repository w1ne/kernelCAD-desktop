#pragma once
#include <QOpenGLWidget>

/// 3D viewport — OpenGL surface for rendering B-Rep tessellations.
/// Uses Qt6 OpenGLWidgets for native GPU rendering.
class Viewport3D : public QOpenGLWidget
{
    Q_OBJECT
public:
    explicit Viewport3D(QWidget* parent = nullptr);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
};
