#include <QtTest/QtTest>
#include "document/Timeline.h"
#include "document/DependencyGraph.h"
#include "features/Feature.h"
#include <memory>

// Lightweight Feature stub for testing -- avoids pulling in OCCT via ExtrudeFeature.
namespace {
class StubFeature : public features::Feature
{
public:
    explicit StubFeature(std::string id) : m_id(std::move(id)) {}
    features::FeatureType type() const override { return features::FeatureType::BaseFeature; }
    std::string id()   const override { return m_id; }
    std::string name() const override { return "Stub"; }
private:
    std::string m_id;
};
} // anonymous namespace

class TestTimeline : public QObject
{
    Q_OBJECT

private:
    std::shared_ptr<features::Feature> makeFeature(const std::string& id)
    {
        return std::make_shared<StubFeature>(id);
    }

private slots:
    void testAppend()
    {
        document::Timeline tl;
        QCOMPARE(tl.count(), size_t(0));
        tl.append(makeFeature("e1"));
        QCOMPARE(tl.count(), size_t(1));
        QCOMPARE(tl.entry(0).id, std::string("e1"));
    }

    void testMarkerAdvances()
    {
        document::Timeline tl;
        tl.append(makeFeature("e1"));
        tl.append(makeFeature("e2"));
        // Marker should be at end after appending
        QCOMPARE(tl.markerPosition(), size_t(2));
    }

    void testRemove()
    {
        document::Timeline tl;
        tl.append(makeFeature("e1"));
        tl.append(makeFeature("e2"));
        tl.remove("e1");
        QCOMPARE(tl.count(), size_t(1));
        QCOMPARE(tl.entry(0).id, std::string("e2"));
    }

    void testRemoveNonexistent()
    {
        document::Timeline tl;
        tl.append(makeFeature("e1"));
        tl.remove("nonexistent");
        QCOMPARE(tl.count(), size_t(1));
    }

    void testSetMarker()
    {
        document::Timeline tl;
        tl.append(makeFeature("e1"));
        tl.append(makeFeature("e2"));
        tl.append(makeFeature("e3"));
        tl.setMarker(1);
        QCOMPARE(tl.markerPosition(), size_t(1));
        // Entries at and after marker should be rolled back
        QVERIFY(!tl.entry(0).isRolledBack);
        QVERIFY(tl.entry(1).isRolledBack);
        QVERIFY(tl.entry(2).isRolledBack);
    }

    void testSetMarkerAtEnd()
    {
        document::Timeline tl;
        tl.append(makeFeature("e1"));
        tl.append(makeFeature("e2"));
        tl.setMarker(2);
        QCOMPARE(tl.markerPosition(), size_t(2));
        QVERIFY(!tl.entry(0).isRolledBack);
        QVERIFY(!tl.entry(1).isRolledBack);
    }

    void testSetMarkerBeyondEnd()
    {
        document::Timeline tl;
        tl.append(makeFeature("e1"));
        tl.setMarker(100);
        // Should clamp to count()
        QCOMPARE(tl.markerPosition(), tl.count());
    }

    void testReorder()
    {
        document::Timeline tl;
        tl.append(makeFeature("e1"));
        tl.append(makeFeature("e2"));
        tl.append(makeFeature("e3"));
        bool ok = tl.reorder(0, 2);
        QVERIFY(ok);
        QCOMPARE(tl.entry(0).id, std::string("e2"));
        QCOMPARE(tl.entry(1).id, std::string("e1"));
    }

    void testCanReorderBounds()
    {
        document::Timeline tl;
        tl.append(makeFeature("e1"));
        QVERIFY(!tl.canReorder(5, 0));  // out of bounds
        QVERIFY(!tl.canReorder(0, 0));  // same position
    }

    void testReorderWithDependencyGraph()
    {
        document::Timeline tl;
        tl.append(makeFeature("A"));
        tl.append(makeFeature("B"));
        tl.append(makeFeature("C"));

        document::DependencyGraph dg;
        dg.addEdge("A", "B"); // B depends on A

        // Moving B before A should be disallowed
        QVERIFY(!tl.canReorder(1, 0, dg));
        // Moving A after B should be disallowed (A would be placed after its dependent)
        QVERIFY(!tl.canReorder(0, 3, dg));
        // Moving C to the front should be fine (no deps)
        QVERIFY(tl.canReorder(2, 0, dg));
    }

    void testDirtyFrom()
    {
        document::Timeline tl;
        tl.append(makeFeature("e1"));
        tl.append(makeFeature("e2"));
        tl.append(makeFeature("e3"));
        auto dirty = tl.dirtyFrom(1);
        QCOMPARE(dirty.size(), size_t(2));
        QCOMPARE(dirty[0], size_t(1));
        QCOMPARE(dirty[1], size_t(2));
    }

    void testDirtySkipsSuppressed()
    {
        document::Timeline tl;
        tl.append(makeFeature("e1"));
        tl.append(makeFeature("e2"));
        tl.append(makeFeature("e3"));
        tl.entry(1).isSuppressed = true;
        auto dirty = tl.dirtyFrom(0);
        QCOMPARE(dirty.size(), size_t(2)); // e1 and e3, not e2
    }

    void testDirtyRespectsMarker()
    {
        document::Timeline tl;
        tl.append(makeFeature("e1"));
        tl.append(makeFeature("e2"));
        tl.append(makeFeature("e3"));
        tl.setMarker(2); // roll back e3
        auto dirty = tl.dirtyFrom(0);
        // Only e1, e2 should be dirty (e3 is rolled back, past marker)
        QCOMPARE(dirty.size(), size_t(2));
    }

    void testSuppress()
    {
        document::Timeline tl;
        tl.append(makeFeature("e1"));
        tl.entry(0).isSuppressed = true;
        QVERIFY(tl.entry(0).isSuppressed);
    }

    void testInsert()
    {
        document::Timeline tl;
        tl.append(makeFeature("e1"));
        tl.append(makeFeature("e3"));

        document::TimelineEntry entry;
        entry.id = "e2";
        entry.name = "Stub";
        entry.feature = makeFeature("e2");
        tl.insert(1, std::move(entry));

        QCOMPARE(tl.count(), size_t(3));
        QCOMPARE(tl.entry(0).id, std::string("e1"));
        QCOMPARE(tl.entry(1).id, std::string("e2"));
        QCOMPARE(tl.entry(2).id, std::string("e3"));
    }

    void testRemoveUpdatesMarker()
    {
        document::Timeline tl;
        tl.append(makeFeature("e1"));
        tl.append(makeFeature("e2"));
        tl.append(makeFeature("e3"));
        // Marker is at 3 (end). Remove one entry, marker should clamp.
        tl.remove("e2");
        QVERIFY(tl.markerPosition() <= tl.count());
    }
};

QTEST_MAIN(TestTimeline)
#include "test_Timeline.moc"
