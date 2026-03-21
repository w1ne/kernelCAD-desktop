#include "Viewport3D.h"
#include <QOpenGLFunctions>
#include <QMouseEvent>

Viewport3D::Viewport3D(QWidget* parent) : QOpenGLWidget(parent) {}

void Viewport3D::initializeGL()
{
    auto* f = QOpenGLContext::currentContext()->functions();
    f->glClearColor(0.18f, 0.18f, 0.18f, 1.0f);
    f->glEnable(GL_DEPTH_TEST);
}

void Viewport3D::resizeGL(int w, int h)
{
    auto* f = QOpenGLContext::currentContext()->functions();
    f->glViewport(0, 0, w, h);
}

void Viewport3D::paintGL()
{
    auto* f = QOpenGLContext::currentContext()->functions();
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // TODO: render body meshes via shader program
}

void Viewport3D::mousePressEvent(QMouseEvent*) {}
void Viewport3D::mouseMoveEvent(QMouseEvent*)  {}
void Viewport3D::wheelEvent(QWheelEvent*)      {}
