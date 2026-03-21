#include "AutoSave.h"
#include "Document.h"
#include "Serializer.h"
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>

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

QString AutoSave::autoSavePath() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    QString docName = QString::fromStdString(m_doc->name()).replace(' ', '_');
    return dir + "/autosave_" + docName + ".kcd";
}

QString AutoSave::recoveryPath(const QString& docName)
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString name = docName.isEmpty() ? "Untitled" : docName;
    name.replace(' ', '_');
    QString path = dir + "/autosave_" + name + ".kcd";
    return QFile::exists(path) ? path : QString();
}

void AutoSave::performAutoSave()
{
    if (!m_doc || !m_doc->isModified()) return;

    QString path = autoSavePath();
    Serializer::save(*m_doc, path.toStdString());
}

} // namespace document
