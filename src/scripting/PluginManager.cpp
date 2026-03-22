#include "PluginManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

PluginManager::PluginManager(QObject* parent)
    : QObject(parent)
{
}

// ---------------------------------------------------------------------------
// Directory helpers
// ---------------------------------------------------------------------------

QStringList PluginManager::pluginDirectories()
{
    QStringList dirs;
    // User-global plugin directory
    dirs << QDir::homePath() + QStringLiteral("/.kernelcad/plugins");
    // Project-local plugin directory (relative to working dir)
    dirs << QDir::currentPath() + QStringLiteral("/plugins");
    return dirs;
}

void PluginManager::ensurePluginDirectory()
{
    QDir pluginDir(QDir::homePath() + QStringLiteral("/.kernelcad/plugins"));
    if (!pluginDir.exists())
        pluginDir.mkpath(QStringLiteral("."));
}

// ---------------------------------------------------------------------------
// Scanning
// ---------------------------------------------------------------------------

void PluginManager::scanPlugins()
{
    m_plugins.clear();

    for (const auto& dir : pluginDirectories()) {
        QDir pluginDir(dir);
        if (!pluginDir.exists())
            continue;

        const auto entries = pluginDir.entryList({QStringLiteral("*.py")}, QDir::Files);
        for (const auto& file : entries) {
            QString path = pluginDir.filePath(file);
            PluginInfo info = parsePluginFile(path);
            if (!info.name.isEmpty()) {
                info.parameters = parseParameters(path);
                m_plugins.push_back(info);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Metadata parsing
// ---------------------------------------------------------------------------

PluginInfo PluginManager::parsePluginFile(const QString& filePath)
{
    PluginInfo info;
    info.filePath = filePath;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return info;

    QTextStream stream(&file);
    bool inHeader = false;

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();

        if (line == QStringLiteral("# kernelcad-plugin")) {
            inHeader = true;
            continue;
        }

        if (!inHeader)
            continue;
        if (!line.startsWith(QStringLiteral("#")))
            break;  // end of header block

        line = line.mid(1).trimmed();  // strip leading '#'

        if (line.startsWith(QStringLiteral("name:")))
            info.name = line.mid(5).trimmed();
        else if (line.startsWith(QStringLiteral("description:")))
            info.description = line.mid(12).trimmed();
        else if (line.startsWith(QStringLiteral("author:")))
            info.author = line.mid(7).trimmed();
        else if (line.startsWith(QStringLiteral("version:")))
            info.version = line.mid(8).trimmed();
    }

    return info;
}

QJsonArray PluginManager::parseParameters(const QString& filePath)
{
    // Read the whole file and look for:
    //   parameters = [
    //       {...}, {...}, ...
    //   ]
    // We extract the JSON-like array between the first '[' after 'parameters =' and
    // the matching ']', then parse it (after converting Python True/False/None).

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QString source = QString::fromUtf8(file.readAll());
    file.close();

    // Find `parameters = [`
    static const QRegularExpression re(QStringLiteral(R"(^parameters\s*=\s*\[)"),
                                       QRegularExpression::MultilineOption);
    auto match = re.match(source);
    if (!match.hasMatch())
        return {};

    int startBracket = match.capturedEnd() - 1;  // position of '['

    // Find the matching ']'
    int depth = 0;
    int endBracket = -1;
    for (int i = startBracket; i < source.size(); ++i) {
        QChar ch = source[i];
        if (ch == QLatin1Char('['))
            ++depth;
        else if (ch == QLatin1Char(']')) {
            --depth;
            if (depth == 0) {
                endBracket = i;
                break;
            }
        }
    }
    if (endBracket < 0)
        return {};

    QString arrayText = source.mid(startBracket, endBracket - startBracket + 1);

    // Python -> JSON fixups
    arrayText.replace(QStringLiteral("True"),  QStringLiteral("true"));
    arrayText.replace(QStringLiteral("False"), QStringLiteral("false"));
    arrayText.replace(QStringLiteral("None"),  QStringLiteral("null"));
    // Replace single-quoted strings with double-quoted strings
    arrayText.replace(QLatin1Char('\''), QLatin1Char('"'));
    // Remove trailing commas before ] or } (valid Python, invalid JSON)
    static const QRegularExpression trailingComma(QStringLiteral(R"(,\s*([\]\}]))"));
    arrayText.replace(trailingComma, QStringLiteral("\\1"));

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(arrayText.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return {};

    return doc.array();
}

// ---------------------------------------------------------------------------
// Execution
// ---------------------------------------------------------------------------

QString PluginManager::executePlugin(const QString& filePath, const QJsonObject& params)
{
    QString paramsJson = QString::fromUtf8(
        QJsonDocument(params).toJson(QJsonDocument::Compact));

    // Determine the path to the python/ directory shipped alongside the binary.
    // Layout: <app_dir>/../../python  (in-tree build)
    //         <app_dir>/python        (installed)
    QString appDir = QCoreApplication::applicationDirPath();
    QString pythonApiPath = QDir(appDir + QStringLiteral("/../../python")).absolutePath();
    if (!QDir(pythonApiPath).exists())
        pythonApiPath = QDir(appDir + QStringLiteral("/python")).absolutePath();

    QString pluginDir = QFileInfo(filePath).absolutePath();

    // Escape single quotes in the JSON string so the inline Python is safe.
    QString escapedParams = paramsJson;
    escapedParams.replace(QLatin1Char('\''), QStringLiteral("\\'"));
    // Also escape backslashes in file paths for Windows compatibility
    QString escapedFilePath = filePath;
    escapedFilePath.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    QString escapedApiPath = pythonApiPath;
    escapedApiPath.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    QString escapedPluginDir = pluginDir;
    escapedPluginDir.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));

    QString wrapper = QStringLiteral(
        "import sys, json\n"
        "sys.path.insert(0, '%1')\n"
        "sys.path.insert(0, '%2')\n"
        "import importlib.util\n"
        "spec = importlib.util.spec_from_file_location('plugin', '%3')\n"
        "mod = importlib.util.module_from_spec(spec)\n"
        "spec.loader.exec_module(mod)\n"
        "params = json.loads('%4')\n"
        "result = mod.run(params)\n"
        "if result and hasattr(result, 'body_id'):\n"
        "    print(json.dumps({'ok': True, 'bodyId': result.body_id}))\n"
        "else:\n"
        "    print(json.dumps({'ok': True}))\n")
        .arg(escapedApiPath, escapedPluginDir, escapedFilePath, escapedParams);

    QProcess proc;
    proc.start(QStringLiteral("python3"), {QStringLiteral("-c"), wrapper});
    if (!proc.waitForStarted(5000))
        return QStringLiteral("Error: failed to start python3");

    proc.waitForFinished(30000);  // 30-second timeout

    QString output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    QString errors = QString::fromUtf8(proc.readAllStandardError()).trimmed();

    if (proc.exitCode() != 0)
        return QStringLiteral("Error: ") + errors;

    return output;
}

QString PluginManager::runPlugin(int index, const QJsonObject& params)
{
    if (index < 0 || index >= static_cast<int>(m_plugins.size()))
        return QStringLiteral("Invalid plugin index");

    const auto& plugin = m_plugins[static_cast<size_t>(index)];
    emit pluginStarted(plugin.name);

    QString result = executePlugin(plugin.filePath, params);

    bool success = !result.startsWith(QStringLiteral("Error:"));
    emit pluginFinished(plugin.name, success, result);

    return result;
}
