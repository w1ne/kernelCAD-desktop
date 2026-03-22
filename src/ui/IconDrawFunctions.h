#pragma once
#include <QPixmap>

/// All icon draw functions extracted from IconFactory.
/// Each function creates and returns a QPixmap of the given size.
namespace IconDraw {

// Feature icons
QPixmap extrude(int size);
QPixmap revolve(int size);
QPixmap fillet(int size);
QPixmap chamfer(int size);
QPixmap hole(int size);
QPixmap shell(int size);
QPixmap draft(int size);
QPixmap sweep(int size);
QPixmap loft(int size);

// Sketch icons
QPixmap sketch(int size);
QPixmap line(int size);
QPixmap rectangle(int size);
QPixmap circle(int size);
QPixmap arc(int size);
QPixmap ellipse(int size);
QPixmap polygon(int size);
QPixmap slot(int size);
QPixmap spline(int size);
QPixmap centerRectangle(int size);
QPixmap circle3Point(int size);
QPixmap arc3Point(int size);
QPixmap filletSketch(int size);
QPixmap chamferSketch(int size);
QPixmap constraintIcon(int size);

// Primitive icons
QPixmap box(int size);
QPixmap cylinder(int size);
QPixmap sphere(int size);

// Pattern and transform icons
QPixmap mirror(int size);
QPixmap rectPattern(int size);
QPixmap circPattern(int size);

// Tool icons
QPixmap measure(int size);
QPixmap trim(int size);
QPixmap extend(int size);
QPixmap offset(int size);
QPixmap project(int size);
QPixmap construction(int size);
QPixmap select(int size);
QPixmap finish(int size);
QPixmap deleteIcon(int size);
QPixmap interference(int size);

// File icons
QPixmap undo(int size);
QPixmap redo(int size);
QPixmap save(int size);
QPixmap open(int size);
QPixmap newIcon(int size);

// Assembly icons
QPixmap joint(int size);
QPixmap component(int size);
QPixmap insert(int size);

// Construct group icons
QPixmap plane(int size);
QPixmap axis(int size);
QPixmap point(int size);

// Import icons
QPixmap importDxf(int size);
QPixmap importSvg(int size);

// Sketch constraint icons
QPixmap coincident(int size);
QPixmap parallel(int size);
QPixmap perpendicular(int size);
QPixmap tangentIcon(int size);
QPixmap equalIcon(int size);
QPixmap symmetric(int size);
QPixmap fix(int size);
QPixmap dimension(int size);

} // namespace IconDraw
