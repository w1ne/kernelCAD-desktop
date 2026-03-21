#include <QtTest/QtTest>
#include "sketch/Sketch.h"
#include "sketch/SketchSolver.h"
#include <cmath>

class TestSketchSolver : public QObject
{
    Q_OBJECT

private slots:
    // ── Basic entity creation ───────────────────────────────────────────
    void testAddPoint()
    {
        sketch::Sketch sk;
        auto id = sk.addPoint(10.0, 20.0);
        QVERIFY(!id.empty());
        QCOMPARE(sk.point(id).x, 10.0);
        QCOMPARE(sk.point(id).y, 20.0);
    }

    void testAddLine()
    {
        sketch::Sketch sk;
        auto lid = sk.addLine(0, 0, 10, 0);
        QVERIFY(!lid.empty());
        QCOMPARE(sk.lines().size(), size_t(1));
        // Should have created 2 points
        QCOMPARE(sk.points().size(), size_t(2));
    }

    void testAddCircle()
    {
        sketch::Sketch sk;
        auto cid = sk.addCircle(5.0, 5.0, 10.0);
        QVERIFY(!cid.empty());
        QCOMPARE(sk.circle(cid).radius, 10.0);
    }

    // ── Coordinate transforms ───────────────────────────────────────────
    void testSketchToWorldXY()
    {
        sketch::Sketch sk;
        sk.setPlane(0, 0, 0, 1, 0, 0, 0, 1, 0); // XY plane
        double wx, wy, wz;
        sk.sketchToWorld(3.0, 4.0, wx, wy, wz);
        QCOMPARE(wx, 3.0);
        QCOMPARE(wy, 4.0);
        QCOMPARE(wz, 0.0);
    }

    void testSketchToWorldXZ()
    {
        sketch::Sketch sk;
        sk.setPlane(0, 0, 0, 1, 0, 0, 0, 0, 1); // XZ plane
        double wx, wy, wz;
        sk.sketchToWorld(3.0, 4.0, wx, wy, wz);
        QCOMPARE(wx, 3.0);
        QCOMPARE(wy, 0.0);
        QCOMPARE(wz, 4.0);
    }

    void testWorldToSketchRoundtrip()
    {
        sketch::Sketch sk;
        sk.setPlane(10, 20, 30, 1, 0, 0, 0, 1, 0);
        double wx, wy, wz;
        sk.sketchToWorld(5.0, 7.0, wx, wy, wz);
        double sx, sy;
        sk.worldToSketch(wx, wy, wz, sx, sy);
        QVERIFY(std::abs(sx - 5.0) < 1e-10);
        QVERIFY(std::abs(sy - 7.0) < 1e-10);
    }

    // ── Constraint solving ──────────────────────────────────────────────
    void testSolveEmpty()
    {
        sketch::Sketch sk;
        auto result = sk.solve();
        QCOMPARE(result.status, sketch::SolveStatus::Solved);
    }

    void testSolveCoincident()
    {
        sketch::Sketch sk;
        auto p1 = sk.addPoint(0.0, 0.0);
        auto p2 = sk.addPoint(5.0, 3.0);
        sk.addConstraint(sketch::ConstraintType::Coincident, {p1, p2});
        auto result = sk.solve();
        QCOMPARE(result.status, sketch::SolveStatus::Solved);
        // After solving, both points should be at the same location
        QVERIFY(std::abs(sk.point(p1).x - sk.point(p2).x) < 1e-8);
        QVERIFY(std::abs(sk.point(p1).y - sk.point(p2).y) < 1e-8);
    }

    void testSolveHorizontal()
    {
        sketch::Sketch sk;
        auto p1 = sk.addPoint(0.0, 0.0, true); // fixed
        auto p2 = sk.addPoint(10.0, 5.0);
        auto lid = sk.addLine(p1, p2);
        sk.addConstraint(sketch::ConstraintType::Horizontal, {lid});
        auto result = sk.solve();
        QCOMPARE(result.status, sketch::SolveStatus::Solved);
        // End point should have same Y as start
        QVERIFY(std::abs(sk.point(p2).y - sk.point(p1).y) < 1e-8);
    }

    void testSolveVertical()
    {
        sketch::Sketch sk;
        auto p1 = sk.addPoint(0.0, 0.0, true);
        auto p2 = sk.addPoint(5.0, 10.0);
        auto lid = sk.addLine(p1, p2);
        sk.addConstraint(sketch::ConstraintType::Vertical, {lid});
        auto result = sk.solve();
        QCOMPARE(result.status, sketch::SolveStatus::Solved);
        QVERIFY(std::abs(sk.point(p2).x - sk.point(p1).x) < 1e-8);
    }

    void testSolveDistance()
    {
        sketch::Sketch sk;
        auto p1 = sk.addPoint(0.0, 0.0, true);
        auto p2 = sk.addPoint(7.0, 0.0);
        sk.addConstraint(sketch::ConstraintType::Distance, {p1, p2}, 10.0);
        auto result = sk.solve();
        QCOMPARE(result.status, sketch::SolveStatus::Solved);
        double dx = sk.point(p2).x - sk.point(p1).x;
        double dy = sk.point(p2).y - sk.point(p1).y;
        double dist = std::sqrt(dx*dx + dy*dy);
        QVERIFY(std::abs(dist - 10.0) < 1e-8);
    }

    void testSolveRectangle()
    {
        // Create a fully constrained 50x30 rectangle
        sketch::Sketch sk;
        auto p1 = sk.addPoint(0, 0, true);    // fixed origin
        auto p2 = sk.addPoint(50, 0);
        auto p3 = sk.addPoint(50, 30);
        auto p4 = sk.addPoint(0, 30);

        auto l1 = sk.addLine(p1, p2);
        auto l2 = sk.addLine(p2, p3);
        auto l3 = sk.addLine(p3, p4);
        auto l4 = sk.addLine(p4, p1);

        // Horizontal/vertical
        sk.addConstraint(sketch::ConstraintType::Horizontal, {l1});
        sk.addConstraint(sketch::ConstraintType::Horizontal, {l3});
        sk.addConstraint(sketch::ConstraintType::Vertical, {l2});
        sk.addConstraint(sketch::ConstraintType::Vertical, {l4});

        // Distances
        sk.addConstraint(sketch::ConstraintType::Distance, {p1, p2}, 50.0);
        sk.addConstraint(sketch::ConstraintType::Distance, {p2, p3}, 30.0);

        auto result = sk.solve();
        QCOMPARE(result.status, sketch::SolveStatus::Solved);

        // Verify rectangle dimensions
        QVERIFY(std::abs(sk.point(p2).x - 50.0) < 1e-6);
        QVERIFY(std::abs(sk.point(p2).y - 0.0) < 1e-6);
        QVERIFY(std::abs(sk.point(p3).x - 50.0) < 1e-6);
        QVERIFY(std::abs(sk.point(p3).y - 30.0) < 1e-6);
    }

    void testSolveParallel()
    {
        sketch::Sketch sk;
        auto p1 = sk.addPoint(0, 0, true);
        auto p2 = sk.addPoint(10, 0, true);
        auto p3 = sk.addPoint(0, 5);
        auto p4 = sk.addPoint(10, 7);
        auto l1 = sk.addLine(p1, p2);
        auto l2 = sk.addLine(p3, p4);
        sk.addConstraint(sketch::ConstraintType::Parallel, {l1, l2});
        auto result = sk.solve();
        QCOMPARE(result.status, sketch::SolveStatus::Solved);
        // l2 should now be horizontal (parallel to l1)
        QVERIFY(std::abs(sk.point(p4).y - sk.point(p3).y) < 1e-8);
    }

    void testSolvePerpendicular()
    {
        sketch::Sketch sk;
        auto p1 = sk.addPoint(0, 0, true);
        auto p2 = sk.addPoint(10, 0, true);
        auto p3 = sk.addPoint(5, 0, true);
        auto p4 = sk.addPoint(5, 8);
        auto l1 = sk.addLine(p1, p2);
        auto l2 = sk.addLine(p3, p4);
        sk.addConstraint(sketch::ConstraintType::Perpendicular, {l1, l2});
        auto result = sk.solve();
        QCOMPARE(result.status, sketch::SolveStatus::Solved);
        // l2 should be vertical (perp to horizontal l1)
        QVERIFY(std::abs(sk.point(p4).x - sk.point(p3).x) < 1e-8);
    }

    void testSolveRadius()
    {
        sketch::Sketch sk;
        auto cpt = sk.addPoint(0, 0, true);
        auto cid = sk.addCircle(cpt, 5.0);
        sk.addConstraint(sketch::ConstraintType::Radius, {cid}, 15.0);
        auto result = sk.solve();
        QCOMPARE(result.status, sketch::SolveStatus::Solved);
        QVERIFY(std::abs(sk.circle(cid).radius - 15.0) < 1e-8);
    }

    // ── DOF counting ────────────────────────────────────────────────────
    void testFreeDOF()
    {
        sketch::Sketch sk;
        sk.addPoint(0, 0);  // 2 DOF
        sk.addPoint(5, 5);  // 2 DOF
        QCOMPARE(sk.freeDOF(), 4);
    }

    void testFreeDOFWithConstraint()
    {
        sketch::Sketch sk;
        auto p1 = sk.addPoint(0, 0);
        auto p2 = sk.addPoint(5, 5);
        sk.addConstraint(sketch::ConstraintType::Coincident, {p1, p2}); // -2
        QCOMPARE(sk.freeDOF(), 2);
    }

    void testFullyConstrained()
    {
        sketch::Sketch sk;
        sk.addPoint(0, 0, true); // fixed = no DOF
        QVERIFY(sk.isFullyConstrained());
    }

    // ── Profile detection ───────────────────────────────────────────────
    void testDetectRectangleProfile()
    {
        sketch::Sketch sk;
        auto p1 = sk.addPoint(0, 0);
        auto p2 = sk.addPoint(10, 0);
        auto p3 = sk.addPoint(10, 5);
        auto p4 = sk.addPoint(0, 5);
        sk.addLine(p1, p2);
        sk.addLine(p2, p3);
        sk.addLine(p3, p4);
        sk.addLine(p4, p1);

        auto profiles = sk.detectProfiles();
        QCOMPARE(profiles.size(), size_t(1));
        QCOMPARE(profiles[0].size(), size_t(4));
    }

    void testDetectCircleProfile()
    {
        sketch::Sketch sk;
        sk.addCircle(0, 0, 10);
        auto profiles = sk.detectProfiles();
        QCOMPARE(profiles.size(), size_t(1));
        QCOMPARE(profiles[0].size(), size_t(1));
    }

    void testConstructionLineExcludedFromProfile()
    {
        sketch::Sketch sk;
        auto p1 = sk.addPoint(0, 0);
        auto p2 = sk.addPoint(10, 0);
        sk.addLine(p1, p2, true); // construction line
        auto profiles = sk.detectProfiles();
        QCOMPARE(profiles.size(), size_t(0));
    }
};

QTEST_MAIN(TestSketchSolver)
#include "test_SketchSolver.moc"
