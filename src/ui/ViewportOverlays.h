#pragma once
#include <QMatrix4x4>
#include <QVector3D>
#include <QPoint>
#include <QPointF>

class QPainter;
class CameraController;

namespace ViewportOverlays {

/// Draw the ViewCube overlay in the top-right corner using a QPainter.
/// @param painter        active QPainter on the viewport widget
/// @param viewRot        3x3 camera rotation as 4x4 (upper-left block)
/// @param widgetWidth    viewport width in pixels
/// @param widgetHeight   viewport height in pixels
/// @param cubeSize       cube area in pixels (default 100)
/// @param margin         margin from top-right corner (default 10)
/// @param hoveredFace    index of the currently hovered face (-1 = none)
/// @param isPerspective  true if camera is in perspective mode
void drawViewCube(QPainter& painter,
                  const QMatrix4x4& viewRot,
                  int widgetWidth, int widgetHeight,
                  int cubeSize, int margin,
                  int hoveredFace,
                  bool isPerspective);

/// Handle a click inside the ViewCube area.
/// @returns the face index (0-5) that was clicked, or -1 if no face was hit.
/// The caller is responsible for calling CameraController::setStandardView()
/// with the appropriate direction/up vectors.
/// @param[out] outDirection  the direction vector for the clicked face
/// @param[out] outUp         the up vector for the clicked face
int handleViewCubeClick(const QPoint& pos,
                        const QMatrix4x4& viewRot,
                        int widgetWidth, int widgetHeight,
                        int cubeSize, int margin,
                        QVector3D& outDirection,
                        QVector3D& outUp);

/// Hit-test which ViewCube face the mouse is over.
/// @returns face index (0-5) or -1 if not over any face.
int hitTestViewCubeFace(const QPoint& pos,
                        const QMatrix4x4& viewRot,
                        int widgetWidth, int widgetHeight,
                        int cubeSize, int margin);

/// Returns true if pos is inside the ViewCube circular area.
bool isInViewCubeArea(const QPoint& pos,
                      int widgetWidth, int widgetHeight,
                      int cubeSize, int margin);

} // namespace ViewportOverlays
