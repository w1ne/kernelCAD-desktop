#include <QtTest/QtTest>
#include "kernel/OCCTKernel.h"
#include <cmath>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <gp_Pnt.hxx>
#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QTemporaryDir>

class TestOCCTKernel : public QObject
{
    Q_OBJECT

private slots:
    void testMakeBox()
    {
        kernel::OCCTKernel k;
        auto shape = k.makeBox(10, 20, 30);
        QVERIFY(!shape.IsNull());
        QCOMPARE(k.faceCount(shape), 6);
        QCOMPARE(k.edgeCount(shape), 24);

        auto props = k.computeProperties(shape);
        QVERIFY(std::abs(props.volume - 6000.0) < 1.0);
    }

    void testMakeCylinder()
    {
        kernel::OCCTKernel k;
        auto shape = k.makeCylinder(10, 30);
        QVERIFY(!shape.IsNull());
        // Cylinder has 3 faces: top, bottom, lateral
        QCOMPARE(k.faceCount(shape), 3);

        auto props = k.computeProperties(shape);
        double expectedVolume = M_PI * 10.0 * 10.0 * 30.0; // ~9424.78
        QVERIFY(std::abs(props.volume - expectedVolume) < 10.0);
    }

    void testMakeSphere()
    {
        kernel::OCCTKernel k;
        auto shape = k.makeSphere(10.0);
        QVERIFY(!shape.IsNull());
        QVERIFY(k.faceCount(shape) >= 1);

        auto props = k.computeProperties(shape);
        double expectedVolume = (4.0 / 3.0) * M_PI * 10.0 * 10.0 * 10.0; // ~4188.79
        QVERIFY(std::abs(props.volume - expectedVolume) < 10.0);
    }

    void testBooleanUnion()
    {
        kernel::OCCTKernel k;
        auto box1 = k.makeBox(10, 10, 10);
        auto box2 = k.translate(k.makeBox(10, 10, 10), 5, 0, 0);
        auto result = k.booleanUnion(box1, box2);
        QVERIFY(!result.IsNull());
        QVERIFY(k.faceCount(result) > 6);
    }

    void testBooleanCut()
    {
        kernel::OCCTKernel k;
        auto box1 = k.makeBox(20, 20, 20);
        auto box2 = k.translate(k.makeBox(10, 10, 10), 5, 5, 5);
        auto result = k.booleanCut(box1, box2);
        QVERIFY(!result.IsNull());
        // Cutting a notch adds faces
        QVERIFY(k.faceCount(result) > 6);

        auto props = k.computeProperties(result);
        // 20^3 - 10^3 = 8000 - 1000 = 7000
        QVERIFY(std::abs(props.volume - 7000.0) < 10.0);
    }

    void testBooleanIntersect()
    {
        kernel::OCCTKernel k;
        auto box1 = k.makeBox(10, 10, 10);
        auto box2 = k.translate(k.makeBox(10, 10, 10), 5, 0, 0);
        auto result = k.booleanIntersect(box1, box2);
        QVERIFY(!result.IsNull());

        auto props = k.computeProperties(result);
        // Overlap is 5x10x10 = 500
        QVERIFY(std::abs(props.volume - 500.0) < 10.0);
    }

    void testExtrude()
    {
        kernel::OCCTKernel k;
        // Build a rectangular face to extrude
        gp_Pnt p1(0, 0, 0), p2(10, 0, 0), p3(10, 10, 0), p4(0, 10, 0);
        BRepBuilderAPI_MakeWire wire;
        wire.Add(BRepBuilderAPI_MakeEdge(p1, p2));
        wire.Add(BRepBuilderAPI_MakeEdge(p2, p3));
        wire.Add(BRepBuilderAPI_MakeEdge(p3, p4));
        wire.Add(BRepBuilderAPI_MakeEdge(p4, p1));
        TopoDS_Shape face = BRepBuilderAPI_MakeFace(wire.Wire());

        auto result = k.extrude(face, 20);
        QVERIFY(!result.IsNull());
        QCOMPARE(k.faceCount(result), 6);

        auto props = k.computeProperties(result);
        QVERIFY(std::abs(props.volume - 2000.0) < 10.0);  // 10*10*20
    }

    void testFillet()
    {
        kernel::OCCTKernel k;
        auto box = k.makeBox(20, 20, 20);
        auto filleted = k.fillet(box, {}, 2.0);  // fillet all edges
        QVERIFY(!filleted.IsNull());
        QVERIFY(k.faceCount(filleted) > 6);  // fillets add faces
    }

    void testChamfer()
    {
        kernel::OCCTKernel k;
        auto box = k.makeBox(20, 20, 20);
        auto chamfered = k.chamfer(box, {}, 2.0);  // chamfer all edges
        QVERIFY(!chamfered.IsNull());
        QVERIFY(k.faceCount(chamfered) > 6);  // chamfers add faces
    }

    void testShell()
    {
        kernel::OCCTKernel k;
        auto box = k.makeBox(20, 20, 20);
        // Shell with one face removed (top face, index 0)
        auto shelled = k.shell(box, 1.0, {0});
        QVERIFY(!shelled.IsNull());

        auto props = k.computeProperties(shelled);
        // Shelled volume should be less than solid
        QVERIFY(props.volume < 8000.0);
        QVERIFY(props.volume > 0.0);
    }

    void testMirror()
    {
        kernel::OCCTKernel k;
        auto box = k.makeBox(10, 10, 10);
        // Mirror about YZ plane at x=15 (plane origin at 15,0,0, normal 1,0,0)
        auto mirrored = k.mirror(box, 15, 0, 0, 1, 0, 0);
        QVERIFY(!mirrored.IsNull());

        auto props = k.computeProperties(mirrored);
        // Two non-overlapping boxes: 2 * 1000 = 2000
        QVERIFY(std::abs(props.volume - 2000.0) < 10.0);
    }

    void testCircularPattern()
    {
        kernel::OCCTKernel k;
        auto box = k.translate(k.makeBox(5, 5, 5), 20, 0, 0);
        // 4 copies around Z axis
        auto pattern = k.circularPattern(box, 0, 0, 0, 0, 0, 1, 4, 360.0);
        QVERIFY(!pattern.IsNull());

        auto props = k.computeProperties(pattern);
        // 4 * 125 = 500
        QVERIFY(std::abs(props.volume - 500.0) < 10.0);
    }

    void testTessellate()
    {
        kernel::OCCTKernel k;
        auto box = k.makeBox(10, 10, 10);
        auto mesh = k.tessellate(box);
        QVERIFY(!mesh.vertices.empty());
        QVERIFY(!mesh.indices.empty());
        QVERIFY(!mesh.normals.empty());
        QCOMPARE(mesh.vertices.size(), mesh.normals.size());
    }

    void testCoarserQualityRemeshesSharedShape()
    {
        kernel::OCCTKernel k;
        // Same shape object both times, as the document does: the fine pass must not
        // stick and make the draft pass return the fine triangulation.
        auto shape = k.makeCylinder(1, 10);
        auto fine = k.tessellate(shape, kernel::OCCTKernel::MeshQuality::fine());
        auto draft = k.tessellate(shape, kernel::OCCTKernel::MeshQuality::draft());
        QVERIFY(draft.indices.size() < fine.indices.size());
    }

    void testQualityControlsFacetDensity()
    {
        kernel::OCCTKernel k;
        // Separate shapes, so one meshing pass cannot serve both. The linear limit
        // is equal, so the angular limit alone decides the facet count.
        const kernel::OCCTKernel::MeshQuality loose{0.5, 0.5};
        const kernel::OCCTKernel::MeshQuality tight{0.5, 0.1};
        auto coarse = k.tessellate(k.makeCylinder(1, 10), loose);
        auto fine = k.tessellate(k.makeCylinder(1, 10), tight);
        QVERIFY(fine.indices.size() > 3 * coarse.indices.size());
    }

    void testRawTessellationIsOpenUntilWelded()
    {
        kernel::OCCTKernel k;
        auto raw = k.tessellate(k.makeBox(10, 20, 30), kernel::OCCTKernel::MeshQuality::standard());
        // Each face is meshed on its own, so shared corners and edges are duplicated
        // and the raw mesh has holes. Welding closes them.
        QVERIFY(kernel::trimesh::analyze(raw.vertices, raw.indices).boundaryEdges > 0);
        kernel::trimesh::weld(raw.vertices, raw.indices, 1e-4);
        QVERIFY(kernel::trimesh::analyze(raw.vertices, raw.indices).watertight());
    }

    void testExportSTLIsWatertight()
    {
        kernel::OCCTKernel k;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("cylinder.stl");

        kernel::trimesh::MeshReport report;
        QVERIFY(k.exportSTL(k.makeCylinder(10, 30), path.toStdString(),
                            kernel::OCCTKernel::MeshQuality::standard(), &report));
        QVERIFY(report.watertight());
        QCOMPARE(report.boundaryEdges, std::size_t(0));
        QCOMPARE(report.nonManifoldEdges, std::size_t(0));

        // Chords lie inside the true circle, so the mesh volume sits a little under
        // pi * r^2 * h. Standard quality keeps the error within 1%.
        const double exact = std::acos(-1.0) * 10 * 10 * 30;
        QVERIFY(std::abs(report.volume - exact) / exact < 0.01);

        // Binary STL: 80-byte header, 4-byte facet count, 50 bytes per facet.
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.size(), qint64(84 + 50 * report.triangleCount));
    }

    void testExportOBJMatchesReport()
    {
        kernel::OCCTKernel k;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("box.obj");

        kernel::trimesh::MeshReport report;
        QVERIFY(k.exportOBJ(k.makeBox(10, 20, 30), path.toStdString(),
                            kernel::OCCTKernel::MeshQuality::standard(), &report));
        QVERIFY(report.watertight());

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        std::size_t vertices = 0, faces = 0;
        while (!file.atEnd()) {
            const QByteArray line = file.readLine();
            if (line.startsWith("v ")) ++vertices;
            else if (line.startsWith("f ")) ++faces;
        }
        QCOMPARE(vertices, report.vertexCount);
        QCOMPARE(faces, report.triangleCount);
    }

    void testExport3MFMatchesReport()
    {
        kernel::OCCTKernel k;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("box.3mf");

        kernel::trimesh::MeshReport report;
        QVERIFY(k.export3MF(k.makeBox(10, 20, 30), path.toStdString(),
                            kernel::OCCTKernel::MeshQuality::standard(), &report));
        QVERIFY(report.watertight());

        // The model XML is stored uncompressed, so its counts can be read directly.
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray data = file.readAll();
        QCOMPARE(std::size_t(data.count("<vertex ")), report.vertexCount);
        QCOMPARE(std::size_t(data.count("<triangle ")), report.triangleCount);
    }

    void testFaceCount()
    {
        kernel::OCCTKernel k;
        auto box = k.makeBox(10, 10, 10);
        QCOMPARE(k.faceCount(box), 6);

        auto cyl = k.makeCylinder(5, 10);
        QCOMPARE(k.faceCount(cyl), 3);
    }

    void testEdgeCount()
    {
        kernel::OCCTKernel k;
        auto box = k.makeBox(10, 10, 10);
        QCOMPARE(k.edgeCount(box), 24);
    }

    void testExportSTEP()
    {
        kernel::OCCTKernel k;
        auto box = k.makeBox(30, 20, 10);
        QVERIFY(k.exportSTEP(box, "/tmp/test_kernel_export.step"));
    }

    void testImportSTEP()
    {
        kernel::OCCTKernel k;
        auto box = k.makeBox(30, 20, 10);
        QVERIFY(k.exportSTEP(box, "/tmp/test_kernel_import.step"));

        auto shapes = k.importSTEP("/tmp/test_kernel_import.step");
        QVERIFY(!shapes.empty());

        auto props = k.computeProperties(shapes[0]);
        QVERIFY(std::abs(props.volume - 6000.0) < 10.0);  // 30*20*10
    }

    void testComputeProperties()
    {
        kernel::OCCTKernel k;
        auto sphere = k.makeSphere(10.0);
        auto props = k.computeProperties(sphere);
        double expectedVolume = (4.0 / 3.0) * M_PI * 10.0 * 10.0 * 10.0;
        QVERIFY(std::abs(props.volume - expectedVolume) < 10.0);
        QVERIFY(std::abs(props.cogX) < 0.1);
        QVERIFY(std::abs(props.cogY) < 0.1);
        QVERIFY(std::abs(props.cogZ) < 0.1);
    }
};

QTEST_GUILESS_MAIN(TestOCCTKernel)
#include "test_OCCTKernel.moc"
