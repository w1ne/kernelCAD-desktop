#include "AutoSave.h"
#include "Document.h"
#include "Serializer.h"
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDateTime>
#include <algorithm>

namespace document {

AutoSave::AutoSave(Document* doc, QObject* parent)
    : QObject(parent), m_doc(doc)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &AutoSave::performAutoSave);
}

void AutoSave::setInterval(int seconds)
{
    m_intervalSec = seconds;
}

void AutoSave::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!m_enabled)
        m_timer.stop();
}

void AutoSave::documentChanged()
{
    if (!m_enabled) return;
    // Reset the countdown — autosave fires after m_intervalSec of no changes
    m_timer.start(m_intervalSec * 1000);
}

void AutoSave::clearAutoSave()
{
    m_timer.stop();
    QString path = autoSavePath();
    if (QFile::exists(path))
        QFile::remove(path);
}

QString AutoSave::autoSaveDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                  + "/autosave";
    QDir().mkpath(dir);
    return dir;
}

QString AutoSave::autoSavePath() const
{
    QString dir = autoSaveDir();
    QString docName = QString::fromStdString(m_doc->name()).replace(' ', '_');
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return dir + "/autosave_" + docName + "_" + timestamp + ".kcd";
}

QString AutoSave::recoveryPath(const QString& docName)
{
    QString dir = autoSaveDir();
    QDir autoDir(dir);
    if (!autoDir.exists())
        return QString();

    // Find all auto-save files, sorted by modification time (newest first)
    QStringList nameFilters;
    if (docName.isEmpty()) {
        nameFilters << "autosave_*.kcd";
    } else {
        QString safeName = docName;
        safeName.replace(' ', '_');
        nameFilters << ("autosave_" + safeName + "_*.kcd");
    }

    QFileInfoList files = autoDir.entryInfoList(nameFilters, QDir::Files, QDir::Time);
    if (files.isEmpty())
        return QString();

    return files.first().absoluteFilePath();
}

void AutoSave::cleanupOldAutoSaves(int maxKeep)
{
    QString dir = autoSaveDir();
    QDir autoDir(dir);
    if (!autoDir.exists())
        return;

    QFileInfoList files = autoDir.entryInfoList(
        {"autosave_*.kcd"}, QDir::Files, QDir::Time);

    // Keep only the newest maxKeep files, delete the rest
    for (int i = maxKeep; i < files.size(); ++i) {
        QFile::remove(files[i].absoluteFilePath());
    }
}

void AutoSave::performAutoSave()
{
    if (!m_doc || !m_doc->isModified()) return;

    try {
        QString path = autoSavePath();
        Serializer::save(*m_doc, path.toStdString());

        // Clean up old auto-saves, keeping only the 3 most recent
        cleanupOldAutoSaves(3);

        emit autoSaved();
    } catch (...) {
        // Auto-save is best-effort; do not crash if it fails
    }
}

} // namespace document
