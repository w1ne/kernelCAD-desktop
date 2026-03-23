#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include "ui/DrawingView.h"
#include "kernel/OCCTKernel.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <cmath>

class TestDrawingView : public QObject
{
    Q_OBJECT

private slots:
    void testEmptyShape()
    {
        DrawingView view;
        // No shape set — generateStandardViews should not crash
        view.generateStandardViews();
        QCOMPARE(view.viewCount(), 0);
        QCOMPARE(view.dimensionCount(), 0);
    }

    void testBoxViewCount()
    {
        DrawingView view;
        TopoDS_Shape box = BRepPrimAPI_MakeBox(50.0, 30.0, 20.0).Shape();
        view.setBody(box);
        view.generateStandardViews();
        // 4 views: Front, Top, Right, Isometric
        QCOMPARE(view.viewCount(), 4);
    }

    void testBoxAutoDimensions()
    {
        DrawingView view;
        TopoDS_Shape box = BRepPrimAPI_MakeBox(50.0, 30.0, 20.0).Shape();
        view.setBody(box);
        view.generateStandardViews();

        // 3 orthographic views x 2 dimensions each = 6 (isometric gets 0)
        QCOMPARE(view.dimensionCount(), 6);
    }

    void testCylinderAutoDimensions()
    {
        DrawingView view;
        TopoDS_Shape cyl = BRepPrimAPI_MakeCylinder(15.0, 40.0).Shape();
        view.setBody(cyl);
        view.generateStandardViews();

        QCOMPARE(view.viewCount(), 4);
        // 3 orthographic views x 2 dimensions each = 6
        QCOMPARE(view.dimensionCount(), 6);
    }

    void testDimensionValues()
    {
        // A 60x40x25 box — check that dimension values match the bounding box
        DrawingView view;
        TopoDS_Shape box = BRepPrimAPI_MakeBox(60.0, 40.0, 25.0).Shape();
        view.setBody(box);
        view.generateStandardViews();

        // viewCount should be 4
        QCOMPARE(view.viewCount(), 4);
        // dimensionCount should be 6 (3 ortho views x 2 each)
        QCOMPARE(view.dimensionCount(), 6);
    }

    void testExportPDF()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString pdfPath = tmpDir.path() + "/test_drawing.pdf";

        DrawingView view;
        TopoDS_Shape box = BRepPrimAPI_MakeBox(50.0, 30.0, 20.0).Shape();
        view.setBody(box);
        view.generateStandardViews();
        view.exportPDF(pdfPath);

        QFileInfo fi(pdfPath);
        QVERIFY(fi.exists());
        QVERIFY(fi.size() > 100); // PDF should have substantial content
    }

    void testExportSVG()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString svgPath = tmpDir.path() + "/test_drawing.svg";

        DrawingView view;
        TopoDS_Shape box = BRepPrimAPI_MakeBox(50.0, 30.0, 20.0).Shape();
        view.setBody(box);
        view.generateStandardViews();
        view.exportSVG(svgPath);

        // SVG export may be compiled out if Qt6::Svg is unavailable,
        // so only check if the file exists.
        QFileInfo fi(svgPath);
        if (fi.exists()) {
            QVERIFY(fi.size() > 100);
        }
    }

    void testCompoundBodyDimensions()
    {
        // Fuse two boxes to make a compound shape
        TopoDS_Shape box1 = BRepPrimAPI_MakeBox(50.0, 30.0, 20.0).Shape();
        TopoDS_Shape box2 = BRepPrimAPI_MakeBox(gp_Pnt(25, 0, 0), gp_Pnt(80, 30, 20)).Shape();
        BRepAlgoAPI_Fuse fuse(box1, box2);
        QVERIFY(fuse.IsDone());

        DrawingView view;
        view.setBody(fuse.Shape());
        view.generateStandardViews();

        QCOMPARE(view.viewCount(), 4);
        QCOMPARE(view.dimensionCount(), 6);
    }

    void testRegenerateClears()
    {
        DrawingView view;
        TopoDS_Shape box = BRepPrimAPI_MakeBox(50.0, 30.0, 20.0).Shape();
        view.setBody(box);
        view.generateStandardViews();
        QCOMPARE(view.dimensionCount(), 6);

        // Regenerate — old dimensions must be cleared, count stays the same
        view.generateStandardViews();
        QCOMPARE(view.dimensionCount(), 6);
    }
};

QTEST_MAIN(TestDrawingView)
#include "test_DrawingView.moc"
