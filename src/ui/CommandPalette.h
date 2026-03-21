#pragma once
#include <QWidget>
#include <QString>
#include <functional>
#include <vector>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QKeyEvent;

/// Full-screen overlay command palette for fuzzy-searching all available
/// commands.  Activated with Ctrl+K or Ctrl+Shift+P.
class CommandPalette : public QWidget
{
    Q_OBJECT
public:
    explicit CommandPalette(QWidget* parent = nullptr);

    struct CommandEntry {
        QString name;        // e.g. "Extrude"
        QString shortcut;    // e.g. "E"
        QString category;    // e.g. "Model"
        std::function<void()> action;
    };

    /// Register the full list of available commands.
    void setCommands(const std::vector<CommandEntry>& commands);

    /// Show the palette overlay and focus the search box.
    void activate();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QLineEdit*   m_searchBox  = nullptr;
    QListWidget* m_resultList = nullptr;
    std::vector<CommandEntry> m_allCommands;
    std::vector<int> m_filteredIndices;   // indices into m_allCommands

    void updateResults(const QString& query);
    void executeSelected();

    /// Simple fuzzy match: all characters of query appear in order in target.
    /// Returns true if matched, and fills `score` with a quality metric (lower = better).
    bool fuzzyMatch(const QString& query, const QString& target, int& score) const;
};
