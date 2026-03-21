#include <QtTest/QtTest>
#include "document/DependencyGraph.h"
#include <algorithm>
#include <unordered_set>

class TestDependencyGraph : public QObject
{
    Q_OBJECT

private slots:

    // ── Node operations ─────────────────────────────────────────────────────

    void testAddNode()
    {
        document::DependencyGraph g;
        g.addNode("A");
        QVERIFY(g.hasNode("A"));
        QVERIFY(!g.hasNode("B"));
    }

    void testAddNodeIdempotent()
    {
        document::DependencyGraph g;
        g.addNode("A");
        g.addNode("A"); // should not crash or duplicate
        QVERIFY(g.hasNode("A"));
        auto topo = g.topologicalSort();
        QCOMPARE(topo.size(), size_t(1));
    }

    void testRemoveNode()
    {
        document::DependencyGraph g;
        g.addNode("A");
        g.addNode("B");
        g.addEdge("A", "B");
        g.removeNode("A");
        QVERIFY(!g.hasNode("A"));
        QVERIFY(g.hasNode("B"));
        // B should have no dependencies left
        auto deps = g.dependenciesOf("B");
        QVERIFY(deps.empty());
    }

    void testRemoveNodeCleansEdges()
    {
        document::DependencyGraph g;
        g.addNode("A");
        g.addNode("B");
        g.addNode("C");
        g.addEdge("A", "B");
        g.addEdge("B", "C");

        g.removeNode("B");

        // A should have no dependents
        auto aDeps = g.dependentsOf("A");
        QVERIFY(aDeps.empty());

        // C should have no dependencies
        auto cDeps = g.dependenciesOf("C");
        QVERIFY(cDeps.empty());
    }

    // ── Edge operations ─────────────────────────────────────────────────────

    void testAddEdge()
    {
        document::DependencyGraph g;
        g.addEdge("A", "B"); // auto-creates nodes

        QVERIFY(g.hasNode("A"));
        QVERIFY(g.hasNode("B"));

        auto depsOfB = g.dependenciesOf("B");
        QCOMPARE(depsOfB.size(), size_t(1));
        QCOMPARE(depsOfB[0], std::string("A"));

        auto deptsOfA = g.dependentsOf("A");
        QCOMPARE(deptsOfA.size(), size_t(1));
        QCOMPARE(deptsOfA[0], std::string("B"));
    }

    void testRemoveEdge()
    {
        document::DependencyGraph g;
        g.addEdge("A", "B");
        g.removeEdge("A", "B");

        auto depsOfB = g.dependenciesOf("B");
        QVERIFY(depsOfB.empty());

        auto deptsOfA = g.dependentsOf("A");
        QVERIFY(deptsOfA.empty());
    }

    // ── propagateDirty ──────────────────────────────────────────────────────

    void testPropagateDirtyLinearChain()
    {
        // A -> B -> C
        document::DependencyGraph g;
        g.addEdge("A", "B");
        g.addEdge("B", "C");

        auto dirty = g.propagateDirty({"A"});
        QCOMPARE(dirty.size(), size_t(3));

        std::unordered_set<std::string> dirtySet(dirty.begin(), dirty.end());
        QVERIFY(dirtySet.count("A"));
        QVERIFY(dirtySet.count("B"));
        QVERIFY(dirtySet.count("C"));

        // Should be in topological order: A before B before C
        auto posA = std::find(dirty.begin(), dirty.end(), "A");
        auto posB = std::find(dirty.begin(), dirty.end(), "B");
        auto posC = std::find(dirty.begin(), dirty.end(), "C");
        QVERIFY(posA < posB);
        QVERIFY(posB < posC);
    }

    void testPropagateDirtyMiddleOfChain()
    {
        // A -> B -> C
        document::DependencyGraph g;
        g.addEdge("A", "B");
        g.addEdge("B", "C");

        auto dirty = g.propagateDirty({"B"});
        QCOMPARE(dirty.size(), size_t(2));

        std::unordered_set<std::string> dirtySet(dirty.begin(), dirty.end());
        QVERIFY(!dirtySet.count("A")); // A is upstream, not dirty
        QVERIFY(dirtySet.count("B"));
        QVERIFY(dirtySet.count("C"));
    }

    void testPropagateDirtyDiamond()
    {
        //   A
        //  / \       .
        // B   C
        //  \ /       .
        //   D
        document::DependencyGraph g;
        g.addEdge("A", "B");
        g.addEdge("A", "C");
        g.addEdge("B", "D");
        g.addEdge("C", "D");

        auto dirty = g.propagateDirty({"A"});
        QCOMPARE(dirty.size(), size_t(4));

        std::unordered_set<std::string> dirtySet(dirty.begin(), dirty.end());
        QVERIFY(dirtySet.count("A"));
        QVERIFY(dirtySet.count("B"));
        QVERIFY(dirtySet.count("C"));
        QVERIFY(dirtySet.count("D"));

        // D must come after B and C in the order
        auto posB = std::find(dirty.begin(), dirty.end(), "B");
        auto posC = std::find(dirty.begin(), dirty.end(), "C");
        auto posD = std::find(dirty.begin(), dirty.end(), "D");
        QVERIFY(posB < posD);
        QVERIFY(posC < posD);
    }

    void testPropagateDirtyMultipleRoots()
    {
        // A -> C
        // B -> C
        document::DependencyGraph g;
        g.addEdge("A", "C");
        g.addEdge("B", "C");

        // Only dirty A -- should get A and C, but not B
        auto dirty = g.propagateDirty({"A"});
        QCOMPARE(dirty.size(), size_t(2));
        std::unordered_set<std::string> dirtySet(dirty.begin(), dirty.end());
        QVERIFY(dirtySet.count("A"));
        QVERIFY(dirtySet.count("C"));
        QVERIFY(!dirtySet.count("B"));
    }

    void testPropagateDirtyLeafNode()
    {
        // A -> B -> C
        document::DependencyGraph g;
        g.addEdge("A", "B");
        g.addEdge("B", "C");

        auto dirty = g.propagateDirty({"C"});
        QCOMPARE(dirty.size(), size_t(1));
        QCOMPARE(dirty[0], std::string("C"));
    }

    void testPropagateDirtyEmpty()
    {
        document::DependencyGraph g;
        g.addNode("A");
        auto dirty = g.propagateDirty({});
        QVERIFY(dirty.empty());
    }

    // ── Topological sort ────────────────────────────────────────────────────

    void testTopologicalSortLinear()
    {
        // A -> B -> C
        document::DependencyGraph g;
        g.addEdge("A", "B");
        g.addEdge("B", "C");

        auto order = g.topologicalSort();
        QCOMPARE(order.size(), size_t(3));
        QCOMPARE(order[0], std::string("A"));
        QCOMPARE(order[1], std::string("B"));
        QCOMPARE(order[2], std::string("C"));
    }

    void testTopologicalSortDiamond()
    {
        document::DependencyGraph g;
        g.addEdge("A", "B");
        g.addEdge("A", "C");
        g.addEdge("B", "D");
        g.addEdge("C", "D");

        auto order = g.topologicalSort();
        QCOMPARE(order.size(), size_t(4));

        // A must be first, D must be last
        QCOMPARE(order[0], std::string("A"));
        QCOMPARE(order[3], std::string("D"));
    }

    void testTopologicalSortDisjoint()
    {
        document::DependencyGraph g;
        g.addNode("X");
        g.addNode("Y");
        g.addEdge("A", "B");

        auto order = g.topologicalSort();
        QCOMPARE(order.size(), size_t(4));
        // A before B
        auto posA = std::find(order.begin(), order.end(), "A");
        auto posB = std::find(order.begin(), order.end(), "B");
        QVERIFY(posA < posB);
    }

    void testTopologicalSortCycleReturnsEmpty()
    {
        document::DependencyGraph g;
        g.addNode("A");
        g.addNode("B");
        // Manually force a cycle by directly adding edges in both directions
        g.addEdge("A", "B");
        g.addEdge("B", "A");

        auto order = g.topologicalSort();
        QVERIFY(order.empty());
    }

    // ── Cycle detection ─────────────────────────────────────────────────────

    void testWouldCreateCycleSelfLoop()
    {
        document::DependencyGraph g;
        g.addNode("A");
        QVERIFY(g.wouldCreateCycle("A", "A"));
    }

    void testWouldCreateCycleDirect()
    {
        document::DependencyGraph g;
        g.addEdge("A", "B");
        // Adding B -> A would create A -> B -> A cycle
        QVERIFY(g.wouldCreateCycle("B", "A"));
    }

    void testWouldCreateCycleTransitive()
    {
        document::DependencyGraph g;
        g.addEdge("A", "B");
        g.addEdge("B", "C");
        // Adding C -> A would create A -> B -> C -> A cycle
        QVERIFY(g.wouldCreateCycle("C", "A"));
    }

    void testWouldNotCreateCycle()
    {
        document::DependencyGraph g;
        g.addEdge("A", "B");
        g.addEdge("B", "C");
        // Adding A -> C is redundant but not a cycle
        QVERIFY(!g.wouldCreateCycle("A", "C"));
    }

    void testWouldNotCreateCycleDisjoint()
    {
        document::DependencyGraph g;
        g.addNode("A");
        g.addNode("B");
        QVERIFY(!g.wouldCreateCycle("A", "B"));
    }

    // ── Clear ───────────────────────────────────────────────────────────────

    void testClear()
    {
        document::DependencyGraph g;
        g.addEdge("A", "B");
        g.addEdge("B", "C");
        g.clear();
        QVERIFY(!g.hasNode("A"));
        QVERIFY(!g.hasNode("B"));
        QVERIFY(!g.hasNode("C"));
        auto order = g.topologicalSort();
        QVERIFY(order.empty());
    }
};

QTEST_MAIN(TestDependencyGraph)
#include "test_DependencyGraph.moc"
