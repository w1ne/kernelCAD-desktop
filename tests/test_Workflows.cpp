#include <QtTest/QtTest>
#include "document/Document.h"
#include "document/Commands.h"
#include "kernel/BRepModel.h"
#include "features/ExtrudeFeature.h"
#include "features/FilletFeature.h"
#include "features/ChamferFeature.h"
#include "features/ShellFeature.h"
#include "features/SketchFeature.h"
#include "sketch/Sketch.h"
#include "document/Serializer.h"
#include <QTemporaryFile>
#include <QDir>
#include <cmath>

class TestWorkflows : public QObject
{
    Q_OBJECT

private slots:
    void testSketchExtrudeFillet()
    {
        document::Document doc;
        features::SketchParams skParams;
        skParams.planeId = "XY";
        std::string sketchId = doc.addSketch(skParams);
        QVERIFY(!sketchId.empty());

        auto* skFeat = doc.findSketch(sketchId);
        QVERIFY(skFeat);
        auto& sk = skFeat->sketch();
        auto p1 = sk.addPoint(0, 0);
        auto p2 = sk.addPoint(50, 0);
        auto p3 = sk.addPoint(50, 30);
        auto p4 = sk.addPoint(0, 30);
        sk.addLine(p1, p2);
        sk.addLine(p2, p3);
        sk.addLine(p3, p4);
        sk.addLine(p4, p1);

        auto profiles = sk.detectProfiles();
        QVERIFY(profiles.size() >= 1);

        features::ExtrudeParams extParams;
        extParams.sketchId = sketchId;
        extParams.profileId = "auto";
        extParams.distanceExpr = "20 mm";
        std::string bodyId = doc.addExtrude(extParams);
        QVERIFY(!bodyId.empty());
        QVERIFY(doc.brepModel().hasBody(bodyId));

        features::FilletParams filletParams;
        filletParams.targetBodyId = bodyId;
        filletParams.radiusExpr = "3 mm";
        doc.addFillet(filletParams);
        QCOMPARE(doc.timeline().count(), size_t(3));
    }

    void testBoxChamferShell()
    {
        document::Document doc;
        features::ExtrudeParams boxParams;
        boxParams.distanceExpr = "40 mm";
        std::string bodyId = doc.addExtrude(boxParams);
        QVERIFY(doc.brepModel().hasBody(bodyId));

        features::ChamferParams chamferParams;
        chamferParams.targetBodyId = bodyId;
        chamferParams.distanceExpr = "2 mm";
        doc.addChamfer(chamferParams);

        features::ShellParams shellParams;
        shellParams.targetBodyId = bodyId;
        shellParams.thicknessExpr = 1.5;
        doc.addShell(shellParams);
        QCOMPARE(doc.timeline().count(), size_t(3));
    }

    void testUndoRedo()
    {
        document::Document doc;
        features::ExtrudeParams params;
        params.distanceExpr = "30 mm";
        auto cmd = std::make_unique<document::AddExtrudeCommand>(params);
        doc.executeCommand(std::move(cmd));
        QCOMPARE(doc.brepModel().bodyIds().size(), size_t(1));

        doc.history().undo(doc);
        QCOMPARE(doc.brepModel().bodyIds().size(), size_t(0));

        doc.history().redo(doc);
        QCOMPARE(doc.brepModel().bodyIds().size(), size_t(1));

        doc.history().undo(doc);
        QCOMPARE(doc.brepModel().bodyIds().size(), size_t(0));
    }

    void testSaveLoadRoundTrip()
    {
        document::Document doc;
        doc.parameters().set("width", "50", "mm");

        features::SketchParams skParams;
        skParams.planeId = "XY";
        std::string sketchId = doc.addSketch(skParams);
        auto* skFeat = doc.findSketch(sketchId);
        auto& sk = skFeat->sketch();
        auto p1 = sk.addPoint(0, 0);
        auto p2 = sk.addPoint(50, 0);
        auto p3 = sk.addPoint(50, 30);
        auto p4 = sk.addPoint(0, 30);
        sk.addLine(p1, p2);
        sk.addLine(p2, p3);
        sk.addLine(p3, p4);
        sk.addLine(p4, p1);
        sk.addConstraint(sketch::ConstraintType::Horizontal, {sk.lines().begin()->first});

        features::ExtrudeParams extParams;
        extParams.sketchId = sketchId;
        extParams.profileId = "auto";
        extParams.distanceExpr = "20 mm";
        doc.addExtrude(extParams);

        size_t origCount = doc.timeline().count();
        size_t origBodies = doc.brepModel().bodyIds().size();

        QString path = QDir::tempPath() + "/kernelcad_test.kcd";
        QVERIFY(doc.save(path.toStdString()));

        document::Document doc2;
        QVERIFY(doc2.load(path.toStdString()));
        QCOMPARE(doc2.timeline().count(), origCount);
        QCOMPARE(doc2.brepModel().bodyIds().size(), origBodies);
        QVERIFY(doc2.parameters().has("width"));
        QCOMPARE(doc2.parameters().get("width"), 50.0);

        QFile::remove(path);
    }

    void testParameterDrivenDesign()
    {
        document::Document doc;
        doc.parameters().set("width", "100", "mm");
        doc.parameters().set("half_w", "width / 2", "mm");
        QCOMPARE(doc.parameters().get("half_w"), 50.0);

        doc.parameters().set("width", "200", "mm");
        QCOMPARE(doc.parameters().get("half_w"), 100.0);
        QCOMPARE(doc.parameters().evaluate("width + 10"), 210.0);
    }

    void testTimelineManipulation()
    {
        document::Document doc;
        features::ExtrudeParams p1; p1.distanceExpr = "10 mm";
        features::ExtrudeParams p2; p2.distanceExpr = "20 mm";
        features::ExtrudeParams p3; p3.distanceExpr = "30 mm";
        doc.addExtrude(p1);
        doc.addExtrude(p2);
        doc.addExtrude(p3);
        QCOMPARE(doc.timeline().count(), size_t(3));

        doc.timeline().entry(1).isSuppressed = true;
        doc.recompute();

        doc.timeline().setMarker(2);
        QVERIFY(doc.timeline().entry(2).isRolledBack);
        QVERIFY(!doc.timeline().entry(0).isRolledBack);

        doc.timeline().setMarker(3);
        QVERIFY(!doc.timeline().entry(2).isRolledBack);
    }

    void testSketchConstraintSolving()
    {
        sketch::Sketch sk;
        auto p1 = sk.addPoint(0, 0, true);
        auto p2 = sk.addPoint(47, 3);
        auto p3 = sk.addPoint(52, 28);
        auto p4 = sk.addPoint(-2, 31);

        auto l1 = sk.addLine(p1, p2);
        auto l2 = sk.addLine(p2, p3);
        auto l3 = sk.addLine(p3, p4);
        auto l4 = sk.addLine(p4, p1);

        sk.addConstraint(sketch::ConstraintType::Horizontal, {l1});
        sk.addConstraint(sketch::ConstraintType::Horizontal, {l3});
        sk.addConstraint(sketch::ConstraintType::Vertical, {l2});
        sk.addConstraint(sketch::ConstraintType::Vertical, {l4});
        sk.addConstraint(sketch::ConstraintType::Distance, {p1, p2}, 50.0);
        sk.addConstraint(sketch::ConstraintType::Distance, {p2, p3}, 30.0);

        auto result = sk.solve();
        QCOMPARE(result.status, sketch::SolveStatus::Solved);
        QVERIFY(std::abs(sk.point(p2).x - 50.0) < 0.1);
        QVERIFY(std::abs(sk.point(p3).y - 30.0) < 0.1);
    }

    void testAssemblyComponents()
    {
        document::Document doc;
        features::ExtrudeParams params;
        params.distanceExpr = "50 mm";
        doc.addExtrude(params);

        std::string childId = doc.components().createComponent("Part2");
        QVERIFY(!childId.empty());
        std::string occId = doc.components().rootComponent().addOccurrence(childId, "Part2:1");
        QVERIFY(!occId.empty());
        QCOMPARE(doc.components().rootComponent().occurrences().size(), size_t(1));
    }

    void testPhysicalProperties()
    {
        document::Document doc;
        features::ExtrudeParams params;
        params.distanceExpr = "50 mm";
        std::string bodyId = doc.addExtrude(params);

        auto props = doc.brepModel().getProperties(bodyId);
        QVERIFY(props.volume > 0);
        QVERIFY(props.surfaceArea > 0);
        QVERIFY(std::abs(props.volume - 125000.0) < 10.0);
    }

    void testDependencyGraphReorder()
    {
        document::Document doc;
        features::SketchParams skParams;
        skParams.planeId = "XY";
        std::string sketchId = doc.addSketch(skParams);

        auto* skFeat = doc.findSketch(sketchId);
        auto& sk = skFeat->sketch();
        auto p1 = sk.addPoint(0, 0);
        auto p2 = sk.addPoint(50, 0);
        auto p3 = sk.addPoint(50, 30);
        auto p4 = sk.addPoint(0, 30);
        sk.addLine(p1, p2);
        sk.addLine(p2, p3);
        sk.addLine(p3, p4);
        sk.addLine(p4, p1);

        features::ExtrudeParams extParams;
        extParams.sketchId = sketchId;
        extParams.profileId = "auto";
        extParams.distanceExpr = "20 mm";
        doc.addExtrude(extParams);

        // Can't move extrude before its sketch dependency
        QVERIFY(!doc.timeline().canReorder(1, 0, doc.depGraph()));
    }
};

QTEST_MAIN(TestWorkflows)
#include "test_Workflows.moc"
