#pragma once
#include <QWidget>

class QTableWidget;
class QPushButton;

namespace document { class Document; }

class ParameterTablePanel : public QWidget
{
    Q_OBJECT
public:
    explicit ParameterTablePanel(QWidget* parent = nullptr);

    void setDocument(document::Document* doc);
    void refresh();

signals:
    void parameterChanged();  // emitted when user edits a param -> triggers recompute

private:
    document::Document* m_doc = nullptr;
    QTableWidget* m_table = nullptr;
    QPushButton* m_addBtn = nullptr;

    /// True while refresh() is populating the table (suppresses onCellChanged).
    bool m_populating = false;

    void onAddParameter();
    void onCellChanged(int row, int col);
    void onDeleteParameter();
};
