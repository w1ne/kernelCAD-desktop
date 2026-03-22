#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <vector>

struct PluginInfo {
    QString name;
    QString description;
    QString author;
    QString version;
    QString filePath;
    QJsonArray parameters;  // parameter definitions parsed from the plugin file
};

class PluginManager : public QObject {
    Q_OBJECT
public:
    explicit PluginManager(QObject* parent = nullptr);

    /// Scan plugin directories and load metadata from all .py files
    /// that begin with the `# kernelcad-plugin` header.
    void scanPlugins();

    /// Get all discovered plugins.
    const std::vector<PluginInfo>& plugins() const { return m_plugins; }

    /// Run a plugin by index, with the given parameter values.
    /// Returns a JSON string with the result or an error message.
    QString runPlugin(int index, const QJsonObject& params);

    /// Directories that are scanned for plugins (user-global + project-local).
    static QStringList pluginDirectories();

    /// Ensure the user plugin directory exists (called once at startup).
    static void ensurePluginDirectory();

signals:
    void pluginStarted(const QString& name);
    void pluginFinished(const QString& name, bool success, const QString& message);

private:
    std::vector<PluginInfo> m_plugins;

    /// Parse plugin metadata from the header comment block.
    PluginInfo parsePluginFile(const QString& filePath);

    /// Parse the `parameters = [...]` list from the plugin source.
    QJsonArray parseParameters(const QString& filePath);

    /// Execute a Python plugin via subprocess, returning stdout or an error string.
    QString executePlugin(const QString& filePath, const QJsonObject& params);
};
