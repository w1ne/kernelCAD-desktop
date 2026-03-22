#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QJsonArray>
#include <QJsonObject>

#include "scripting/PluginManager.h"

class TestPluginManager : public QObject
{
    Q_OBJECT

private slots:
    void testPluginDirectories();
    void testParseValidPlugin();
    void testParseInvalidPlugin();
    void testParsePluginParameters();
    void testScanEmptyDirectory();
    void testScanWithPlugins();
    void testRunPluginInvalidIndex();
    void testEnsurePluginDirectory();

private:
    /// Write a test plugin file into the given directory and return its path.
    QString writePlugin(const QString& dir, const QString& filename,
                        const QString& content);
};

QString TestPluginManager::writePlugin(const QString& dir, const QString& filename,
                                        const QString& content)
{
    QDir().mkpath(dir);
    QString path = dir + "/" + filename;
    QFile f(path);
    f.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&f);
    out << content;
    return path;
}

void TestPluginManager::testPluginDirectories()
{
    QStringList dirs = PluginManager::pluginDirectories();
    QVERIFY(dirs.size() >= 2);
    QVERIFY(dirs[0].contains(".kernelcad/plugins"));
    QVERIFY(dirs[1].contains("plugins"));
}

void TestPluginManager::testParseValidPlugin()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    QString pluginSrc =
        "# kernelcad-plugin\n"
        "# name: Test Plugin\n"
        "# description: A test\n"
        "# author: Tester\n"
        "# version: 0.1\n"
        "\n"
        "def run(params):\n"
        "    pass\n";

    QString path = writePlugin(tmpDir.path(), "test_plugin.py", pluginSrc);

    // Use a PluginManager to parse — we can't call the private method directly,
    // but we can scan the temp directory.
    PluginManager mgr;

    // We'll scan the temp directory by temporarily changing working directory
    // to one that has the temp path as project plugins dir.
    // Instead, we'll create a subdir called "plugins" and set CWD.
    QString pluginsDir = tmpDir.path() + "/plugins";
    QDir().mkpath(pluginsDir);
    QFile::copy(path, pluginsDir + "/test_plugin.py");

    // Save and change working dir
    QString origDir = QDir::currentPath();
    QDir::setCurrent(tmpDir.path());

    mgr.scanPlugins();

    QDir::setCurrent(origDir);

    // Find our plugin (there may be user plugins too)
    bool found = false;
    for (const auto& p : mgr.plugins()) {
        if (p.name == "Test Plugin") {
            QCOMPARE(p.description, QString("A test"));
            QCOMPARE(p.author, QString("Tester"));
            QCOMPARE(p.version, QString("0.1"));
            found = true;
            break;
        }
    }
    QVERIFY(found);
}

void TestPluginManager::testParseInvalidPlugin()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // A .py file without the kernelcad-plugin header should be ignored
    QString pluginSrc =
        "# Just a regular Python script\n"
        "def run(params):\n"
        "    pass\n";

    QString pluginsDir = tmpDir.path() + "/plugins";
    writePlugin(pluginsDir, "not_a_plugin.py", pluginSrc);

    QString origDir = QDir::currentPath();
    QDir::setCurrent(tmpDir.path());

    PluginManager mgr;
    mgr.scanPlugins();

    QDir::setCurrent(origDir);

    // The invalid file should not appear among scanned plugins
    for (const auto& p : mgr.plugins()) {
        QVERIFY(p.filePath != pluginsDir + "/not_a_plugin.py");
    }
}

void TestPluginManager::testParsePluginParameters()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    QString pluginSrc =
        "# kernelcad-plugin\n"
        "# name: ParamTest\n"
        "# description: Tests parameter parsing\n"
        "# author: Tester\n"
        "# version: 1.0\n"
        "\n"
        "def run(params):\n"
        "    pass\n"
        "\n"
        "parameters = [\n"
        "    {'name': 'width', 'type': 'float', 'default': 10.0, 'min': 1.0, 'max': 100.0, 'label': 'Width'},\n"
        "    {'name': 'count', 'type': 'int', 'default': 5, 'min': 1, 'max': 50, 'label': 'Count'},\n"
        "]\n";

    QString pluginsDir = tmpDir.path() + "/plugins";
    writePlugin(pluginsDir, "param_test.py", pluginSrc);

    QString origDir = QDir::currentPath();
    QDir::setCurrent(tmpDir.path());

    PluginManager mgr;
    mgr.scanPlugins();

    QDir::setCurrent(origDir);

    bool found = false;
    for (const auto& p : mgr.plugins()) {
        if (p.name == "ParamTest") {
            found = true;
            QCOMPARE(p.parameters.size(), 2);

            QJsonObject p0 = p.parameters[0].toObject();
            QCOMPARE(p0["name"].toString(), QString("width"));
            QCOMPARE(p0["type"].toString(), QString("float"));
            QCOMPARE(p0["default"].toDouble(), 10.0);

            QJsonObject p1 = p.parameters[1].toObject();
            QCOMPARE(p1["name"].toString(), QString("count"));
            QCOMPARE(p1["type"].toString(), QString("int"));
            QCOMPARE(p1["default"].toInt(), 5);
            break;
        }
    }
    QVERIFY(found);
}

void TestPluginManager::testScanEmptyDirectory()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    QString pluginsDir = tmpDir.path() + "/plugins";
    QDir().mkpath(pluginsDir);

    QString origDir = QDir::currentPath();
    QDir::setCurrent(tmpDir.path());

    PluginManager mgr;
    mgr.scanPlugins();

    QDir::setCurrent(origDir);

    // Only user-global plugins (if any) should be found; the local dir is empty
    // We just verify no crash occurred and the scan completed
    QVERIFY(true);
}

void TestPluginManager::testScanWithPlugins()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    QString pluginsDir = tmpDir.path() + "/plugins";

    writePlugin(pluginsDir, "a.py",
        "# kernelcad-plugin\n# name: Alpha\n\ndef run(p): pass\n");
    writePlugin(pluginsDir, "b.py",
        "# kernelcad-plugin\n# name: Beta\n\ndef run(p): pass\n");
    writePlugin(pluginsDir, "skip.py",
        "# Not a plugin\ndef run(p): pass\n");

    QString origDir = QDir::currentPath();
    QDir::setCurrent(tmpDir.path());

    PluginManager mgr;
    mgr.scanPlugins();

    QDir::setCurrent(origDir);

    // Count how many of our test plugins were found
    int foundCount = 0;
    for (const auto& p : mgr.plugins()) {
        if (p.name == "Alpha" || p.name == "Beta")
            ++foundCount;
    }
    QCOMPARE(foundCount, 2);
}

void TestPluginManager::testRunPluginInvalidIndex()
{
    PluginManager mgr;
    QString result = mgr.runPlugin(-1, {});
    QCOMPARE(result, QString("Invalid plugin index"));

    result = mgr.runPlugin(999, {});
    QCOMPARE(result, QString("Invalid plugin index"));
}

void TestPluginManager::testEnsurePluginDirectory()
{
    // Just verify no crash — the directory may already exist
    PluginManager::ensurePluginDirectory();
    QVERIFY(QDir(QDir::homePath() + "/.kernelcad/plugins").exists());
}

QTEST_MAIN(TestPluginManager)
#include "test_PluginManager.moc"
