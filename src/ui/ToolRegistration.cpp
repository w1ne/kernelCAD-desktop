#include "ToolRegistration.h"
#include "ToolRegistry.h"
#include "IconFactory.h"
#include "CommandController.h"
#include "MainWindow.h"
#include "Viewport3D.h"
#include "SketchEditor.h"
#include "../document/Document.h"
#include "../kernel/BRepModel.h"
#include "../kernel/BRepQuery.h"
#include <QStatusBar>
#include <QIcon>

// Helper to reduce boilerplate in registerAllTools
static void regTool(ToolRegistry& reg,
                    const std::string& id, const QString& name,
                    const QString& shortcut, const QString& group,
                    const QString& tab, const QString& menuPath,
                    const QIcon& icon, const QString& tooltip,
                    std::function<void()> action,
                    int sortOrder,
                    bool isDropdownExtra = false,
                    bool showInCtx = false, const QString& ctxFor = {},
                    const QString& helpParams = {}, const QString& helpReturns = {},
                    const QString& helpHint = {})
{
    ToolDefinition t;
    t.id = id;
    t.name = name;
    t.shortcut = shortcut;
    t.group = group;
    t.tab = tab;
    t.menuPath = menuPath;
    t.icon = icon;
    t.tooltip = tooltip;
    t.helpParams = helpParams;
    t.helpReturns = helpReturns;
    t.helpHint = helpHint;
    t.action = std::move(action);
    t.showInContextMenu = showInCtx;
    t.contextMenuFor = ctxFor;
    t.sortOrder = sortOrder;
    t.isDropdownExtra = isDropdownExtra;
    reg.registerTool(std::move(t));
}

void registerAllTools(MainWindow* mw, CommandController* cmd)
{
    auto& reg = ToolRegistry::instance();
    reg.clear();

    // ════════════════════════════════════════════════════════════════════════
    // SOLID tab
    // ════════════════════════════════════════════════════════════════════════

    // ── Create group ────────────────────────────────────────────────────
    regTool(reg, "createSketch", "Sketch", "S", "Create", "SOLID", "Sketch",
            IconFactory::createIcon("sketch"), QObject::tr("Sketch (S) \u2014 Create a 2D sketch"),
            [cmd]() { cmd->onCreateSketch(); }, 10,
            false, true, "empty",
            "{plane:\"XY\"|\"XZ\"|\"YZ\"}", "{sketchId,featureId}",
            "Next: add geometry, then sketchSolve.");

    regTool(reg, "createBox", "Box", "", "Create", "SOLID", "Model",
            IconFactory::createIcon("box"), QObject::tr("Box \u2014 Create a parametric box"),
            [cmd]() { cmd->onCreateBox(); }, 20,
            false, true, "empty",
            "{dx,dy,dz}", "{featureId,bodyId}", "All dims in mm.");

    regTool(reg, "createCylinder", "Cylinder", "", "Create", "SOLID", "Model",
            IconFactory::createIcon("cylinder"), QObject::tr("Cylinder \u2014 Create a parametric cylinder"),
            [cmd]() { cmd->onCreateCylinder(); }, 30,
            false, false, {},
            "{radius,height}", "{featureId,bodyId}", "Quick cylinder.");

    regTool(reg, "createSphere", "Sphere", "", "Create", "SOLID", "Model",
            IconFactory::createIcon("sphere"), QObject::tr("Sphere \u2014 Create a parametric sphere"),
            [cmd]() { cmd->onCreateSphere(); }, 40,
            false, false, {},
            "{radius}", "{featureId,bodyId}", "Quick sphere.");

    // Dropdown extras for Create
    regTool(reg, "createTorus", "Torus", "", "Create", "SOLID", "Model",
            {}, QObject::tr("Torus \u2014 Create a parametric torus"),
            [cmd]() { cmd->onCreateTorus(); }, 50, true);

    regTool(reg, "createCoil", "Coil", "", "Create", "SOLID", "Model",
            {}, QObject::tr("Coil"),
            [mw]() { mw->statusBar()->showMessage(QObject::tr("Coil \u2014 not yet implemented")); }, 60, true);

    regTool(reg, "createPipe", "Pipe", "", "Create", "SOLID", "Model",
            {}, QObject::tr("Pipe \u2014 Create a hollow cylinder"),
            [cmd]() { cmd->onCreatePipe(); }, 70, true);

    // ── Form group ──────────────────────────────────────────────────────
    regTool(reg, "extrude", "Extrude", "E", "Form", "SOLID", "Model",
            IconFactory::createIcon("extrude"), QObject::tr("Extrude (E) \u2014 Extrude a sketch or face"),
            [cmd]() { cmd->onExtrudeSketch(); }, 10,
            false, true, "face",
            "{sketchId,distance,symmetric?}", "{featureId,bodyId}",
            "Pushes 2D sketch into 3D.");

    regTool(reg, "revolve", "Revolve", "", "Form", "SOLID", "Model",
            IconFactory::createIcon("revolve"), QObject::tr("Revolve \u2014 Revolve around an axis"),
            [cmd]() { cmd->onRevolveSketch(); }, 20);

    regTool(reg, "sweep", "Sweep", "", "Form", "SOLID", "Model",
            IconFactory::createIcon("sweep"), QObject::tr("Sweep \u2014 Sweep a profile along a path"),
            [cmd]() { cmd->onSweepSketch(); }, 30);

    regTool(reg, "loft", "Loft", "", "Form", "SOLID", "Model",
            IconFactory::createIcon("loft"), QObject::tr("Loft \u2014 Solid between profiles"),
            [cmd]() { cmd->onLoftTest(); }, 40);

    // Dropdown extras for Form
    regTool(reg, "extrudeFromFace", "Extrude from Face", "", "Form", "SOLID", "",
            {}, QObject::tr("Extrude from Face"),
            [cmd]() { cmd->onExtrudeSketch(); }, 50, true);

    regTool(reg, "revolveFromSketch", "Revolve from Sketch", "", "Form", "SOLID", "",
            {}, QObject::tr("Revolve from Sketch"),
            [cmd]() { cmd->onRevolveSketch(); }, 60, true);

    // ── Modify group ────────────────────────────────────────────────────
    regTool(reg, "fillet", "Fillet", "F", "Modify", "SOLID", "Model",
            IconFactory::createIcon("fillet"), QObject::tr("Fillet (F) \u2014 Round selected edges"),
            [cmd]() { cmd->onFillet(); }, 10,
            false, true, "edge",
            "{bodyId,radius,edgeIds?}", "{featureId,bodyId}",
            "Rounds edges. Omit edgeIds for all.");

    regTool(reg, "chamfer", "Chamfer", "", "Modify", "SOLID", "Model",
            IconFactory::createIcon("chamfer"), QObject::tr("Chamfer \u2014 Bevel selected edges"),
            [cmd]() { cmd->onChamfer(); }, 20,
            false, true, "edge",
            "{bodyId,distance,edgeIds?}", "{featureId,bodyId}", "Bevels edges.");

    regTool(reg, "shell", "Shell", "", "Modify", "SOLID", "Model",
            IconFactory::createIcon("shell"), QObject::tr("Shell \u2014 Hollow a body"),
            [cmd]() { cmd->onShell(); }, 30,
            false, true, "face",
            "{bodyId,thickness}", "{featureId,bodyId}",
            "Hollows the body with given wall thickness.");

    regTool(reg, "draft", "Draft", "D", "Modify", "SOLID", "Model",
            IconFactory::createIcon("draft"), QObject::tr("Draft (D) \u2014 Apply draft angle to faces"),
            [cmd]() { cmd->onDraft(); }, 40,
            false, true, "face");

    regTool(reg, "hole", "Hole", "H", "Modify", "SOLID", "Model",
            IconFactory::createIcon("hole"), QObject::tr("Hole (H) \u2014 Create a hole on a face"),
            [cmd]() { cmd->onAddHole(); }, 50,
            false, true, "face",
            "{bodyId,x,y,z,dx,dy,dz,diameter,depth}", "{featureId,bodyId}",
            "Drill a hole at position along direction.");

    // Dropdown extras for Modify
    regTool(reg, "pressPull", "Press/Pull", "Q", "Modify", "SOLID", "Model",
            {}, QObject::tr("Press/Pull (Q) \u2014 Push/pull faces"),
            [cmd]() { cmd->onPressPull(); }, 60, true,
            true, "face");

    regTool(reg, "scale", "Scale", "", "Modify", "SOLID", "",
            {}, QObject::tr("Scale"),
            [mw]() { mw->statusBar()->showMessage(QObject::tr("Scale \u2014 not yet implemented")); }, 70, true);

    regTool(reg, "combine", "Combine", "", "Modify", "SOLID", "",
            {}, QObject::tr("Combine"),
            [mw]() { mw->statusBar()->showMessage(QObject::tr("Combine \u2014 not yet implemented")); }, 80, true);

    regTool(reg, "replaceFace", "Replace Face", "", "Modify", "SOLID", "",
            {}, QObject::tr("Replace Face"),
            [mw]() { mw->statusBar()->showMessage(QObject::tr("Replace Face \u2014 not yet implemented")); }, 90, true);

    regTool(reg, "splitFace", "Split Face", "", "Modify", "SOLID", "",
            {}, QObject::tr("Split Face"),
            [mw]() { mw->statusBar()->showMessage(QObject::tr("Split Face \u2014 not yet implemented")); }, 100, true);

    regTool(reg, "splitBody", "Split Body", "", "Modify", "SOLID", "",
            {}, QObject::tr("Split Body"),
            [mw]() { mw->statusBar()->showMessage(QObject::tr("Split Body \u2014 not yet implemented")); }, 110, true);

    regTool(reg, "offsetFaces", "Offset Faces", "", "Modify", "SOLID", "",
            {}, QObject::tr("Offset Faces"),
            [mw]() { mw->statusBar()->showMessage(QObject::tr("Offset Faces \u2014 not yet implemented")); }, 120, true);

    regTool(reg, "deleteFace", "Delete Face", "", "Modify", "SOLID", "",
            {}, QObject::tr("Delete Face"),
            [mw]() { mw->statusBar()->showMessage(QObject::tr("Delete Face \u2014 not yet implemented")); }, 130, true);

    regTool(reg, "thread", "Thread", "", "Modify", "SOLID", "",
            {}, QObject::tr("Thread"),
            [mw]() { mw->statusBar()->showMessage(QObject::tr("Thread \u2014 not yet implemented")); }, 140, true);

    regTool(reg, "thicken", "Thicken", "", "Modify", "SOLID", "",
            {}, QObject::tr("Thicken"),
            [mw]() { mw->statusBar()->showMessage(QObject::tr("Thicken \u2014 not yet implemented")); }, 150, true);

    regTool(reg, "moveCopy", "Move/Copy", "", "Modify", "SOLID", "",
            {}, QObject::tr("Move/Copy"),
            [mw]() { mw->statusBar()->showMessage(QObject::tr("Move/Copy \u2014 not yet implemented")); }, 160, true);

    regTool(reg, "appearance", "Appearance", "", "Modify", "SOLID", "",
            {}, QObject::tr("Appearance"),
            [mw]() { mw->statusBar()->showMessage(QObject::tr("Appearance \u2014 not yet implemented")); }, 170, true);

    regTool(reg, "emboss", "Emboss", "", "Modify", "SOLID", "",
            IconFactory::createIcon("extrude"), QObject::tr("Emboss/Deboss \u2014 Extrude a profile into or out of a face"),
            [cmd]() { cmd->onExtrudeSketch(); }, 180, true,
            true, "face");

    // ── Pattern group ───────────────────────────────────────────────────
    regTool(reg, "mirror", "Mirror", "", "Pattern", "SOLID", "Model",
            IconFactory::createIcon("mirror"), QObject::tr("Mirror \u2014 Mirror across a plane"),
            [cmd]() { cmd->onMirrorLastBody(); }, 10,
            false, true, "body",
            "{bodyId,planeNormalX,Y,Z}", "{featureId,bodyId}",
            "Mirror about a plane through origin.");

    regTool(reg, "rectPattern", "Rect Pattern", "", "Pattern", "SOLID", "Model",
            IconFactory::createIcon("rect_pattern"), QObject::tr("Rect Pattern \u2014 Rectangular array"),
            [cmd]() { cmd->onRectangularPattern(); }, 20,
            false, true, "body");

    regTool(reg, "circPattern", "Circ Pattern", "", "Pattern", "SOLID", "Model",
            IconFactory::createIcon("circ_pattern"), QObject::tr("Circ Pattern \u2014 Circular array"),
            [cmd]() { cmd->onCircularPattern(); }, 30,
            false, true, "body",
            "{bodyId,count,angle}", "{featureId,bodyId}", "Repeats body around Z axis.");

    // Dropdown extra for Pattern
    regTool(reg, "pathPattern", "Path Pattern", "", "Pattern", "SOLID", "",
            {}, QObject::tr("Path Pattern"),
            [mw]() { mw->statusBar()->showMessage(QObject::tr("Path Pattern \u2014 not yet implemented")); }, 40, true);

    // ── Construct group ─────────────────────────────────────────────────
    regTool(reg, "constructPlane", "Plane", "", "Construct", "SOLID", "",
            IconFactory::createIcon("plane"), QObject::tr("Construct Plane \u2014 Create a construction plane"),
            [cmd]() { cmd->onConstructPlane(); }, 10);

    regTool(reg, "constructAxis", "Axis", "", "Construct", "SOLID", "",
            IconFactory::createIcon("axis"), QObject::tr("Construct Axis \u2014 Create a construction axis"),
            [cmd]() { cmd->onConstructAxis(); }, 20);

    regTool(reg, "constructPoint", "Point", "", "Construct", "SOLID", "",
            IconFactory::createIcon("point"), QObject::tr("Construct Point \u2014 Create a construction point"),
            [cmd]() { cmd->onConstructPoint(); }, 30);

    // Dropdown extras for Construct
    regTool(reg, "offsetPlane", "Offset Plane", "", "Construct", "SOLID", "",
            {}, QObject::tr("Offset Plane"),
            [cmd]() { cmd->onConstructPlane(); }, 40, true);

    regTool(reg, "planeAtAngle", "Plane at Angle", "", "Construct", "SOLID", "",
            {}, QObject::tr("Plane at Angle"),
            [mw]() { mw->statusBar()->showMessage(QObject::tr("Plane at Angle \u2014 not yet implemented")); }, 50, true);

    regTool(reg, "planeThrough3Points", "Plane Through 3 Points", "", "Construct", "SOLID", "",
            {}, QObject::tr("Plane Through 3 Points"),
            [mw]() { mw->statusBar()->showMessage(QObject::tr("Plane Through 3 Points \u2014 not yet implemented")); }, 60, true);

    regTool(reg, "axisThrough2Points", "Axis Through 2 Points", "", "Construct", "SOLID", "",
            {}, QObject::tr("Axis Through 2 Points"),
            [cmd]() { cmd->onConstructAxis(); }, 70, true);

    regTool(reg, "pointAtVertex", "Point at Vertex", "", "Construct", "SOLID", "",
            {}, QObject::tr("Point at Vertex"),
            [cmd]() { cmd->onConstructPoint(); }, 80, true);

    // ── Inspect group ───────────────────────────────────────────────────
    regTool(reg, "measure", "Measure", "M", "Inspect", "SOLID", "Tools",
            IconFactory::createIcon("measure"), QObject::tr("Measure (M) \u2014 Measure distances"),
            [cmd]() { cmd->onMeasure(); }, 10,
            false, true, "face",
            "{}", "{}", "Measure distances between entities.");

    // Dropdown extras for Inspect
    regTool(reg, "physicalProperties", "Physical Properties", "", "Inspect", "SOLID", "",
            {}, QObject::tr("Physical Properties"),
            [mw]() { mw->statusBar()->showMessage(QObject::tr("Physical Properties \u2014 not yet implemented")); }, 20, true);

    regTool(reg, "faceCount", "Face Count", "", "Inspect", "SOLID", "",
            {}, QObject::tr("Face Count"),
            [mw]() {
                auto& brep = mw->m_document->brepModel();
                auto ids = brep.bodyIds();
                int totalFaces = 0;
                for (const auto& id : ids) {
                    auto bq = brep.query(id);
                    totalFaces += bq.faceCount();
                }
                mw->statusBar()->showMessage(QObject::tr("Total faces: %1").arg(totalFaces));
            }, 30, true);

    regTool(reg, "edgeCount", "Edge Count", "", "Inspect", "SOLID", "",
            {}, QObject::tr("Edge Count"),
            [mw]() {
                auto& brep = mw->m_document->brepModel();
                auto ids = brep.bodyIds();
                int totalEdges = 0;
                for (const auto& id : ids) {
                    auto bq = brep.query(id);
                    totalEdges += bq.edgeCount();
                }
                mw->statusBar()->showMessage(QObject::tr("Total edges: %1").arg(totalEdges));
            }, 40, true);

    // ════════════════════════════════════════════════════════════════════════
    // SKETCH tab
    // ════════════════════════════════════════════════════════════════════════

    // ── Draw group ──────────────────────────────────────────────────────
    regTool(reg, "sketchLine", "Line", "L", "Draw", "SKETCH", "Sketch",
            IconFactory::createIcon("line"), QObject::tr("Line (L)"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::DrawLine); }, 10);

    regTool(reg, "sketchRectangle", "Rectangle", "R", "Draw", "SKETCH", "Sketch",
            IconFactory::createIcon("rectangle"), QObject::tr("Rectangle (R)"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::DrawRectangle); }, 20);

    regTool(reg, "sketchCircle", "Circle", "C", "Draw", "SKETCH", "Sketch",
            IconFactory::createIcon("circle"), QObject::tr("Circle (C)"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::DrawCircle); }, 30);

    regTool(reg, "sketchArc", "Arc", "A", "Draw", "SKETCH", "Sketch",
            IconFactory::createIcon("arc"), QObject::tr("Arc (A)"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::DrawArc); }, 40);

    regTool(reg, "sketchSpline", "Spline", "S", "Draw", "SKETCH", "Sketch",
            IconFactory::createIcon("spline"), QObject::tr("Spline (S)"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::DrawSpline); }, 50);

    regTool(reg, "sketchEllipse", "Ellipse", "", "Draw", "SKETCH", "Sketch",
            IconFactory::createIcon("ellipse"), QObject::tr("Ellipse"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::DrawEllipse); }, 60);

    regTool(reg, "sketchPolygon", "Polygon", "", "Draw", "SKETCH", "Sketch",
            IconFactory::createIcon("polygon"), QObject::tr("Polygon"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::DrawPolygon); }, 70);

    regTool(reg, "sketchSlot", "Slot", "", "Draw", "SKETCH", "Sketch",
            IconFactory::createIcon("slot"), QObject::tr("Slot"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::DrawSlot); }, 80);

    // Dropdown extras for Draw
    regTool(reg, "sketchCenterRect", "Center Rect", "", "Draw", "SKETCH", "",
            IconFactory::createIcon("center_rectangle"), QObject::tr("Center Rectangle"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::DrawRectangleCenter); }, 90, true);

    regTool(reg, "sketchCircle3Point", "3pt Circle", "", "Draw", "SKETCH", "",
            IconFactory::createIcon("circle_3point"), QObject::tr("3-Point Circle"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::DrawCircle3Point); }, 100, true);

    regTool(reg, "sketchArc3Point", "3pt Arc", "", "Draw", "SKETCH", "",
            IconFactory::createIcon("arc_3point"), QObject::tr("3-Point Arc"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::DrawArc3Point); }, 110, true);

    // ── Constrain group ─────────────────────────────────────────────────
    regTool(reg, "constrainCoincident", "Coincident", "", "Constrain", "SKETCH", "",
            IconFactory::createIcon("coincident"), QObject::tr("Coincident \u2014 Merge two points"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::ConstrainCoincident); }, 10);

    regTool(reg, "constrainParallel", "Parallel", "", "Constrain", "SKETCH", "",
            IconFactory::createIcon("parallel_c"), QObject::tr("Parallel \u2014 Make lines parallel"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::ConstrainParallel); }, 20);

    regTool(reg, "constrainPerpendicular", "Perpendicular", "", "Constrain", "SKETCH", "",
            IconFactory::createIcon("perpendicular"), QObject::tr("Perpendicular \u2014 Make lines perpendicular"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::ConstrainPerpendicular); }, 30);

    regTool(reg, "constrainTangent", "Tangent", "", "Constrain", "SKETCH", "",
            IconFactory::createIcon("tangent_c"), QObject::tr("Tangent \u2014 Make curves tangent"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::ConstrainTangent); }, 40);

    regTool(reg, "constrainEqual", "Equal", "", "Constrain", "SKETCH", "",
            IconFactory::createIcon("equal_c"), QObject::tr("Equal \u2014 Equal length or radius"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::ConstrainEqual); }, 50);

    regTool(reg, "constrainSymmetric", "Symmetric", "", "Constrain", "SKETCH", "",
            IconFactory::createIcon("symmetric_c"), QObject::tr("Symmetric \u2014 Mirror about a line"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::ConstrainSymmetric); }, 60);

    regTool(reg, "constrainFix", "Fix", "", "Constrain", "SKETCH", "",
            IconFactory::createIcon("fix"), QObject::tr("Fix \u2014 Lock a point in place"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::AddConstraint); }, 70);

    regTool(reg, "dimension", "Dimension", "D", "Constrain", "SKETCH", "",
            IconFactory::createIcon("dimension"), QObject::tr("Dimension (D) \u2014 Set a parametric dimension"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::Dimension); }, 80);

    regTool(reg, "autoConstraint", "Auto", "K", "Constrain", "SKETCH", "",
            IconFactory::createIcon("constraint"), QObject::tr("Auto Constraint (K) \u2014 Infer constraint type"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::AddConstraint); }, 90);

    // ── Sketch Modify group ─────────────────────────────────────────────
    regTool(reg, "sketchTrim", "Trim", "T", "Modify", "SKETCH", "",
            IconFactory::createIcon("trim"), QObject::tr("Trim (T)"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::Trim); }, 10);

    regTool(reg, "sketchExtend", "Extend", "E", "Modify", "SKETCH", "",
            IconFactory::createIcon("extend"), QObject::tr("Extend (E)"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::Extend); }, 20);

    regTool(reg, "sketchOffset", "Offset", "O", "Modify", "SKETCH", "",
            IconFactory::createIcon("offset"), QObject::tr("Offset (O)"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::Offset); }, 30);

    regTool(reg, "sketchFillet", "Fillet", "F", "Modify", "SKETCH", "",
            IconFactory::createIcon("fillet"), QObject::tr("Sketch Fillet (F)"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::SketchFillet); }, 40);

    regTool(reg, "sketchChamfer", "Chamfer", "G", "Modify", "SKETCH", "",
            IconFactory::createIcon("chamfer"), QObject::tr("Sketch Chamfer (G)"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::SketchChamfer); }, 50);

    // ── Reference group ─────────────────────────────────────────────────
    regTool(reg, "projectEdge", "Project", "P", "Reference", "SKETCH", "",
            IconFactory::createIcon("project"), QObject::tr("Project Edge (P)"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::ProjectEdge); }, 10);

    regTool(reg, "constructionMode", "Construction", "X", "Reference", "SKETCH", "",
            IconFactory::createIcon("construction"), QObject::tr("Construction Mode (X)"),
            []() { /* toggled via keyboard X in sketch editor */ }, 20);

    regTool(reg, "importDxf", "Import DXF", "", "Reference", "SKETCH", "Sketch",
            IconFactory::createIcon("import_dxf"), QObject::tr("Import DXF into sketch"),
            [mw]() { mw->onImportDxfToSketch(); }, 30);

    regTool(reg, "importSvg", "Import SVG", "", "Reference", "SKETCH", "Sketch",
            IconFactory::createIcon("import_svg"), QObject::tr("Import SVG into sketch"),
            [mw]() { mw->onImportSvgToSketch(); }, 40);

    // ── Control group ───────────────────────────────────────────────────
    regTool(reg, "sketchSelect", "Select", "", "Control", "SKETCH", "",
            IconFactory::createIcon("select"), QObject::tr("Select"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->setTool(SketchTool::None); }, 10);

    regTool(reg, "finishSketch", "Finish", "", "Control", "SKETCH", "",
            IconFactory::createIcon("finish"), QObject::tr("Finish Sketch"),
            [mw]() { if (mw->sketchEditor()) mw->sketchEditor()->finishEditing(); }, 20);

    // ════════════════════════════════════════════════════════════════════════
    // ASSEMBLY tab
    // ════════════════════════════════════════════════════════════════════════

    // ── Assemble group ──────────────────────────────────────────────────
    regTool(reg, "joint", "Joint", "J", "Assemble", "ASSEMBLY", "Assembly",
            IconFactory::createIcon("joint"), QObject::tr("Joint (J) \u2014 Click two faces to create a joint"),
            [cmd]() { cmd->onAddJoint(); }, 10);

    regTool(reg, "newComponent", "New Component", "", "Assemble", "ASSEMBLY", "Assembly",
            IconFactory::createIcon("component"), QObject::tr("New Component \u2014 Add a new component"),
            [cmd]() { cmd->onNewComponent(); }, 20);

    regTool(reg, "insertComponent", "Insert .kcd", "", "Assemble", "ASSEMBLY", "Assembly",
            IconFactory::createIcon("insert"), QObject::tr("Insert Component \u2014 Import a .kcd file"),
            [cmd]() { cmd->onInsertComponent(); }, 30);

    regTool(reg, "jointSlider", "Slider Joint", "", "Assemble", "ASSEMBLY", "Assembly",
            IconFactory::createIcon("joint"), QObject::tr("Slider Joint \u2014 1 DOF linear motion"),
            [cmd]() { cmd->onAddSliderJoint(); }, 40, true);

    regTool(reg, "jointCylindrical", "Cylindrical Joint", "", "Assemble", "ASSEMBLY", "Assembly",
            IconFactory::createIcon("joint"), QObject::tr("Cylindrical Joint \u2014 rotation + translation along axis"),
            [cmd]() { cmd->onAddCylindricalJoint(); }, 50, true);

    regTool(reg, "jointPinSlot", "Pin-Slot Joint", "", "Assemble", "ASSEMBLY", "Assembly",
            IconFactory::createIcon("joint"), QObject::tr("Pin-Slot Joint \u2014 rotation + perpendicular translation"),
            [cmd]() { cmd->onAddPinSlotJoint(); }, 60, true);

    regTool(reg, "jointBall", "Ball Joint", "", "Assemble", "ASSEMBLY", "Assembly",
            IconFactory::createIcon("joint"), QObject::tr("Ball Joint \u2014 3 DOF rotation"),
            [cmd]() { cmd->onAddBallJoint(); }, 70, true);

    // ── Assembly Modify group ───────────────────────────────────────────
    regTool(reg, "unstitch", "Unstitch", "", "Modify", "ASSEMBLY", "Assembly",
            IconFactory::createIcon("stitch"), QObject::tr("Unstitch \u2014 Separate a body into individual faces"),
            [cmd]() { cmd->onUnstitch(); }, 80, true,
            true, "body");

    // ── Assembly Inspect group ──────────────────────────────────────────
    regTool(reg, "checkInterference", "Interference", "", "Inspect", "ASSEMBLY", "Assembly",
            IconFactory::createIcon("interference"), QObject::tr("Check Interference"),
            [cmd]() { cmd->onCheckInterference(); }, 10);

    // ════════════════════════════════════════════════════════════════════════
    // Non-ribbon tools (File, Edit, View -- for command palette & menus)
    // ════════════════════════════════════════════════════════════════════════

    regTool(reg, "newDocument", "New Document", "Ctrl+N", "", "", "File",
            {}, QObject::tr("New Document"),
            [mw]() { mw->onNewDocument(); }, 10);

    regTool(reg, "openDocument", "Open...", "Ctrl+O", "", "", "File",
            {}, QObject::tr("Open Document"),
            [mw]() { mw->onOpenDocument(); }, 20);

    regTool(reg, "saveDocument", "Save", "Ctrl+S", "", "", "File",
            {}, QObject::tr("Save Document"),
            [mw]() { mw->onSaveDocument(); }, 30);

    regTool(reg, "importFile", "Import File...", "Ctrl+I", "", "", "File",
            {}, QObject::tr("Import STEP/IGES"),
            [mw]() { mw->onImportFile(); }, 40);

    regTool(reg, "importStl", "Import STL as Body...", "", "", "", "File",
            {}, QObject::tr("Import STL as Body"),
            [mw]() { mw->onImportSTL(); }, 50);

    regTool(reg, "exportStep", "Export STEP...", "", "", "", "File",
            {}, QObject::tr("Export STEP"),
            [mw]() { mw->onExportSTEP(); }, 60);

    regTool(reg, "exportStl", "Export STL...", "", "", "", "File",
            {}, QObject::tr("Export STL"),
            [mw]() { mw->onExportSTL(); }, 70);

    regTool(reg, "undo", "Undo", "Ctrl+Z", "", "", "Edit",
            {}, QObject::tr("Undo"),
            [mw]() { mw->onUndo(); }, 10);

    regTool(reg, "redo", "Redo", "Ctrl+Y", "", "", "Edit",
            {}, QObject::tr("Redo"),
            [mw]() { mw->onRedo(); }, 20);

    regTool(reg, "deleteFeature", "Delete", "Del", "", "", "Edit",
            {}, QObject::tr("Delete the selected feature"),
            [cmd]() { cmd->onDeleteSelectedFeature(); }, 30,
            false, true, "face");

    regTool(reg, "editSketch", "Edit Sketch", "", "", "", "Sketch",
            {}, QObject::tr("Edit Sketch"),
            [cmd]() { cmd->onEditSketch(); }, 20);

    // ── View tools (command palette only) ───────────────────────────────
    regTool(reg, "fitAll", "Fit All", "Home", "", "", "View",
            {}, QObject::tr("Fit All"),
            [mw]() { mw->m_viewport->fitAll(); }, 10);

    regTool(reg, "viewFront", "Front View", "Num1", "", "", "View",
            {}, QObject::tr("Front View"),
            [mw]() { mw->m_viewport->setStandardView(StandardView::Front); }, 20);

    regTool(reg, "viewBack", "Back View", "", "", "", "View",
            {}, QObject::tr("Back View"),
            [mw]() { mw->m_viewport->setStandardView(StandardView::Back); }, 30);

    regTool(reg, "viewLeft", "Left View", "", "", "", "View",
            {}, QObject::tr("Left View"),
            [mw]() { mw->m_viewport->setStandardView(StandardView::Left); }, 40);

    regTool(reg, "viewRight", "Right View", "Num3", "", "", "View",
            {}, QObject::tr("Right View"),
            [mw]() { mw->m_viewport->setStandardView(StandardView::Right); }, 50);

    regTool(reg, "viewTop", "Top View", "Num7", "", "", "View",
            {}, QObject::tr("Top View"),
            [mw]() { mw->m_viewport->setStandardView(StandardView::Top); }, 60);

    regTool(reg, "viewBottom", "Bottom View", "", "", "", "View",
            {}, QObject::tr("Bottom View"),
            [mw]() { mw->m_viewport->setStandardView(StandardView::Bottom); }, 70);

    regTool(reg, "viewIsometric", "Isometric View", "Num0", "", "", "View",
            {}, QObject::tr("Isometric View"),
            [mw]() { mw->m_viewport->setStandardView(StandardView::Isometric); }, 80);

    regTool(reg, "toggleGrid", "Toggle Grid", "G", "", "", "View",
            {}, QObject::tr("Toggle Grid"),
            [mw]() { mw->m_viewport->setShowGrid(!mw->m_viewport->showGrid()); }, 90);

    regTool(reg, "toggleOrigin", "Toggle Origin", "O", "", "", "View",
            {}, QObject::tr("Toggle Origin"),
            [mw]() { mw->m_viewport->setShowOrigin(!mw->m_viewport->showOrigin()); }, 100);

    regTool(reg, "sectionX", "Section X", "", "", "", "View",
            {}, QObject::tr("Section X"),
            [mw]() { mw->onSectionX(); }, 110);

    regTool(reg, "sectionY", "Section Y", "", "", "", "View",
            {}, QObject::tr("Section Y"),
            [mw]() { mw->onSectionY(); }, 120);

    regTool(reg, "sectionZ", "Section Z", "", "", "", "View",
            {}, QObject::tr("Section Z"),
            [mw]() { mw->onSectionZ(); }, 130);

    regTool(reg, "clearSection", "Clear Section", "", "", "", "View",
            {}, QObject::tr("Clear Section"),
            [mw]() { mw->onClearSection(); }, 140);

    // Context-menu only tools (not in ribbon)
    regTool(reg, "createSketchOnFace", "Create Sketch on Face", "", "", "", "",
            {}, QObject::tr("Create Sketch on Face"),
            [cmd]() { cmd->onCreateSketch(); }, 20,
            false, true, "face");

    regTool(reg, "measureEdge", "Measure", "M", "", "", "",
            {}, QObject::tr("Measure"),
            [cmd]() { cmd->onMeasure(); }, 50,
            false, true, "edge");

    regTool(reg, "deleteEdge", "Delete", "", "", "", "",
            {}, QObject::tr("Delete"),
            [cmd]() { cmd->onDeleteSelectedFeature(); }, 60,
            false, true, "edge");

    regTool(reg, "measureBody", "Measure", "", "", "", "",
            {}, QObject::tr("Measure"),
            [cmd]() { cmd->onMeasure(); }, 50,
            false, true, "body");

    regTool(reg, "shellBody", "Shell", "", "", "", "",
            {}, QObject::tr("Shell"),
            [cmd]() { cmd->onShell(); }, 40,
            false, true, "body");

    regTool(reg, "holeBody", "Hole", "", "", "", "",
            {}, QObject::tr("Hole"),
            [cmd]() { cmd->onAddHole(); }, 45,
            false, true, "body");

    regTool(reg, "deleteBody", "Delete", "", "", "", "",
            {}, QObject::tr("Delete"),
            [cmd]() { cmd->onDeleteSelectedFeature(); }, 60,
            false, true, "body");
}
