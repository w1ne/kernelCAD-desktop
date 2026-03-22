#pragma once
#include <QObject>
#include <QTimer>
#include <QString>

namespace document {

class Document;

class AutoSave : public QObject
{
    Q_OBJECT
public:
    explicit AutoSave(Document* doc, QObject* parent = nullptr);

    void setInterval(int seconds);
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    /// Call after any document mutation to reset the countdown.
    void documentChanged();

    /// Delete the autosave file (call on normal save or new document).
    void clearAutoSave();

    /// Check if any recovery file exists. Returns the most recent path, or empty string.
    static QString recoveryPath(const QString& docName = QString());

signals:
    /// Emitted after a successful auto-save so the UI can show a brief status message.
    void autoSaved();

private slots:
    void performAutoSave();

private:
    Document* m_doc;
    QTimer m_timer;
    bool m_enabled = true;
    int m_intervalSec = 300; // 5 minutes

    QString autoSavePath() const;

    /// Remove old auto-save files, keeping only the most recent maxKeep entries.
    static void cleanupOldAutoSaves(int maxKeep = 3);

    /// Return the auto-save directory path.
    static QString autoSaveDir();
};

} // namespace document
