#pragma once
#include <QIcon>
#include <QPixmap>
#include <QString>

/// Programmatic icon factory -- generates all toolbar icons via QPainter.
/// No image files needed; icons are drawn as clean vector shapes,
/// bright on dark background (dark-theme friendly).
class IconFactory {
public:
    /// Create an icon by name. Size is the pixel dimension (square).
    static QIcon createIcon(const QString& name, int size = 32);

private:
    static QPixmap drawExtrude(int size);
    static QPixmap drawRevolve(int size);
    static QPixmap drawFillet(int size);
    static QPixmap drawChamfer(int size);
    static QPixmap drawHole(int size);
    static QPixmap drawShell(int size);
    static QPixmap drawDraft(int size);
    static QPixmap drawSketch(int size);
    static QPixmap drawLine(int size);
    static QPixmap drawRectangle(int size);
    static QPixmap drawCircle(int size);
    static QPixmap drawArc(int size);
    static QPixmap drawEllipse(int size);
    static QPixmap drawPolygon(int size);
    static QPixmap drawSlot(int size);
    static QPixmap drawBox(int size);
    static QPixmap drawCylinder(int size);
    static QPixmap drawSphere(int size);
    static QPixmap drawMirror(int size);
    static QPixmap drawRectPattern(int size);
    static QPixmap drawCircPattern(int size);
    static QPixmap drawMeasure(int size);
    static QPixmap drawUndo(int size);
    static QPixmap drawRedo(int size);
    static QPixmap drawSave(int size);
    static QPixmap drawOpen(int size);
    static QPixmap drawNew(int size);
    static QPixmap drawSweep(int size);
    static QPixmap drawLoft(int size);
    static QPixmap drawJoint(int size);
    static QPixmap drawComponent(int size);
    static QPixmap drawInsert(int size);
    static QPixmap drawTrim(int size);
    static QPixmap drawExtend(int size);
    static QPixmap drawOffset(int size);
    static QPixmap drawProject(int size);
    static QPixmap drawConstruction(int size);
    static QPixmap drawSelect(int size);
    static QPixmap drawFinish(int size);
    static QPixmap drawDelete(int size);
    static QPixmap drawInterference(int size);

    // Construct group icons
    static QPixmap drawPlane(int size);
    static QPixmap drawAxis(int size);
    static QPixmap drawPoint(int size);

    // Additional sketch tool icons
    static QPixmap drawSpline(int size);
    static QPixmap drawCenterRectangle(int size);
    static QPixmap drawCircle3Point(int size);
    static QPixmap drawArc3Point(int size);
    static QPixmap drawFilletSketch(int size);
    static QPixmap drawChamferSketch(int size);
    static QPixmap drawConstraintIcon(int size);

    // Sketch constraint icons
    static QPixmap drawCoincident(int size);
    static QPixmap drawParallel(int size);
    static QPixmap drawPerpendicular(int size);
    static QPixmap drawTangentIcon(int size);
    static QPixmap drawEqualIcon(int size);
    static QPixmap drawSymmetric(int size);
    static QPixmap drawFix(int size);
    static QPixmap drawDimension(int size);
};
