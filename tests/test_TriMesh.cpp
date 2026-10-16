#include <QtTest/QtTest>
#include <QByteArray>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>
#include <cmath>
#include <cstring>
#include <utility>

#include "kernel/TriMesh.h"

using kernel::trimesh::analyze;
using kernel::trimesh::MeshReport;
using kernel::trimesh::weld;

namespace {

// Outward-facing tetrahedron with unit legs; its volume is 1/6.
const std::vector<float> kTetraVertices = { 0, 0, 0,  1, 0, 0,  0, 1, 0,  0, 0, 1 };
const std::vector<uint32_t> kTetraIndices = { 0, 2, 1,  0, 1, 3,  0, 3, 2,  1, 2, 3 };

} // namespace

class TestTriMesh : public QObject
{
    Q_OBJECT

private slots:
    void testWeldMergesDuplicateVertices()
    {
        // Two triangles share the diagonal (1,0,0)-(0,1,0); each carries its own copies of those corners.
        std::vector<float> v = { 0, 0, 0,  1, 0, 0,  0, 1, 0,  1, 0, 0,  1, 1, 0,  0, 1, 0 };
        std::vector<uint32_t> i = { 0, 1, 2,  3, 4, 5 };
        weld(v, i, 1e-6);
        QCOMPARE(v.size() / 3, std::size_t(4));
        QCOMPARE(i.size() / 3, std::size_t(2));

        const MeshReport r = analyze(v, i);
        QCOMPARE(r.boundaryEdges, std::size_t(4));  // the square's outline
        QCOMPARE(r.orientationErrors, std::size_t(0));
    }

    void testWeldDropsCollapsedTriangles()
    {
        std::vector<float> v = { 0, 0, 0,  0, 0, 0,  1, 0, 0 };
        std::vector<uint32_t> i = { 0, 1, 2 };
        weld(v, i, 1e-6);
        QVERIFY(i.empty());
        QCOMPARE(v.size() / 3, std::size_t(2));
    }

    void testWeldRespectsTolerance()
    {
        std::vector<uint32_t> noIndices;

        std::vector<float> apart = { 0, 0, 0,  1e-4f, 0, 0 };
        weld(apart, noIndices, 1e-5);  // farther apart than the tolerance: kept apart
        QCOMPARE(apart.size() / 3, std::size_t(2));

        std::vector<float> joined = { 0, 0, 0,  1e-4f, 0, 0 };
        weld(joined, noIndices, 1e-3);  // within the tolerance: merged
        QCOMPARE(joined.size() / 3, std::size_t(1));
    }

    void testOpenQuadHasBoundaryAndNoVolume()
    {
        const std::vector<float> v = { 0, 0, 0,  1, 0, 0,  1, 1, 0,  0, 1, 0 };
        const std::vector<uint32_t> i = { 0, 1, 2,  0, 2, 3 };
        const MeshReport r = analyze(v, i);
        QCOMPARE(r.boundaryEdges, std::size_t(4));
        QCOMPARE(r.orientationErrors, std::size_t(0));
        QVERIFY(!r.watertight());
    }

    void testTetrahedronIsWatertight()
    {
        const MeshReport r = analyze(kTetraVertices, kTetraIndices);
        QCOMPARE(r.triangleCount, std::size_t(4));
        QCOMPARE(r.boundaryEdges, std::size_t(0));
        QCOMPARE(r.nonManifoldEdges, std::size_t(0));
        QCOMPARE(r.orientationErrors, std::size_t(0));
        QCOMPARE(r.degenerateTriangles, std::size_t(0));
        QVERIFY(std::abs(r.volume - 1.0 / 6.0) < 1e-9);
        QVERIFY(r.watertight());
    }

    void testInvertedTetrahedronFailsWatertight()
    {
        // Reversing every facet keeps shared edges consistent but turns the normals inward.
        std::vector<uint32_t> flipped;
        for (std::size_t t = 0; t < kTetraIndices.size(); t += 3) {
            flipped.push_back(kTetraIndices[t]);
            flipped.push_back(kTetraIndices[t + 2]);
            flipped.push_back(kTetraIndices[t + 1]);
        }
        const MeshReport r = analyze(kTetraVertices, flipped);
        QCOMPARE(r.orientationErrors, std::size_t(0));
        QVERIFY(r.volume < 0.0);
        QVERIFY(!r.watertight());
    }

    void testMismatchedFacetIsCounted()
    {
        // Reversing one facet makes each of its three shared edges walk the same way twice.
        std::vector<uint32_t> bad = kTetraIndices;
        std::swap(bad[1], bad[2]);
        const MeshReport r = analyze(kTetraVertices, bad);
        QCOMPARE(r.orientationErrors, std::size_t(3));
        QVERIFY(!r.watertight());
    }

    void testBinarySTLLayout()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("tetra.stl");
        QVERIFY(kernel::trimesh::writeBinarySTL(path.toStdString(), kTetraVertices, kTetraIndices));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.size(), qint64(84 + 50 * 4));

        const QByteArray data = file.readAll();
        QCOMPARE(static_cast<quint8>(data.at(80)), quint8(4));  // little-endian facet count

        // The first facet is (0,2,1), whose outward normal points along -Z.
        float normal[3];
        std::memcpy(normal, data.constData() + 84, sizeof(normal));
        QVERIFY(std::abs(normal[0]) < 1e-6f);
        QVERIFY(std::abs(normal[1]) < 1e-6f);
        QVERIFY(std::abs(normal[2] + 1.0f) < 1e-6f);
    }

    void testOBJListsWeldedMesh()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("tetra.obj");
        QVERIFY(kernel::trimesh::writeOBJ(path.toStdString(), kTetraVertices, kTetraIndices));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QStringList lines =
            QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);

        int vertexLines = 0, faceLines = 0;
        for (const QString& line : lines) {
            if (line.startsWith(QLatin1String("v "))) ++vertexLines;
            if (line.startsWith(QLatin1String("f "))) ++faceLines;
        }
        QCOMPARE(vertexLines, 4);
        QCOMPARE(faceLines, 4);
        QVERIFY(lines.contains(QLatin1String("f 1 3 2")));  // facet (0,2,1), 1-based
    }
};

QTEST_GUILESS_MAIN(TestTriMesh)
#include "test_TriMesh.moc"
