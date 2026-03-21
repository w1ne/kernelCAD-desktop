#include "ParameterTablePanel.h"
#include "../document/Document.h"
#include "../document/ParameterStore.h"

#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QLabel>

// Column indices
static constexpr int ColName       = 0;
static constexpr int ColExpression = 1;
static constexpr int ColValue      = 2;
static constexpr int ColUnit       = 3;
static constexpr int ColComment    = 4;
static constexpr int ColCount      = 5;

// ---------------------------------------------------------------------------
// Dark-theme stylesheet shared by the panel
// ---------------------------------------------------------------------------
static const char* kTableStyle = R"(
QTableWidget {
    background-color: #2b2b2b;
    color: #e0e0e0;
    gridline-color: #444;
    border: none;
    selection-background-color: #37474f;
    selection-color: #ffffff;
    font-size: 12px;
}
QTableWidget::item {
    padding: 2px 6px;
}
QHeaderView::section {
    background-color: #3c3f41;
    color: #bbb;
    border: 1px solid #555;
    padding: 3px 6px;
    font-weight: bold;
    font-size: 11px;
}
QPushButton {
    background-color: #3c3f41;
    color: #e0e0e0;
    border: 1px solid #555;
    padding: 4px 14px;
    border-radius: 3px;
    font-size: 12px;
}
QPushButton:hover {
    background-color: #4a4d50;
}
QPushButton:pressed {
    background-color: #555;
}
)";

ParameterTablePanel::ParameterTablePanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Table
    m_table = new QTableWidget(0, ColCount, this);
    m_table->setHorizontalHeaderLabels({tr("Name"), tr("Expression"), tr("Value"),
                                         tr("Unit"), tr("Comment")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(ColName,       QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(ColExpression, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ColValue,      QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(ColUnit,       QHeaderView::Interactive);
    m_table->horizontalHeader()->setDefaultSectionSize(100);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setStyleSheet(QLatin1String(kTableStyle));

    layout->addWidget(m_table, 1);

    // Add button
    m_addBtn = new QPushButton(tr("+ Add Parameter"), this);
    m_addBtn->setStyleSheet(QLatin1String(kTableStyle));
    layout->addWidget(m_addBtn);

    // Connections
    connect(m_table, &QTableWidget::cellChanged, this, &ParameterTablePanel::onCellChanged);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (m_table->rowAt(pos.y()) >= 0)
            onDeleteParameter();
    });
    connect(m_addBtn, &QPushButton::clicked, this, &ParameterTablePanel::onAddParameter);
}

void ParameterTablePanel::setDocument(document::Document* doc)
{
    m_doc = doc;
    refresh();
}

void ParameterTablePanel::refresh()
{
    m_populating = true;

    m_table->setRowCount(0);

    if (!m_doc) {
        m_populating = false;
        return;
    }

    const auto& params = m_doc->parameters().all();

    // Collect and sort by name for deterministic display order
    std::vector<std::string> names;
    names.reserve(params.size());
    for (const auto& [n, _] : params)
        names.push_back(n);
    std::sort(names.begin(), names.end());

    m_table->setRowCount(static_cast<int>(names.size()));

    for (int row = 0; row < static_cast<int>(names.size()); ++row) {
        const auto& p = params.at(names[row]);

        // Name
        auto* nameItem = new QTableWidgetItem(QString::fromStdString(p.name));
        m_table->setItem(row, ColName, nameItem);

        // Expression
        auto* exprItem = new QTableWidgetItem(QString::fromStdString(p.expression));
        m_table->setItem(row, ColExpression, exprItem);

        // Value (read-only)
        auto* valItem = new QTableWidgetItem(QString::number(p.cachedValue, 'g', 10));
        valItem->setFlags(valItem->flags() & ~Qt::ItemIsEditable);
        valItem->setForeground(QBrush(QColor(0x90, 0xCA, 0xF9)));  // light blue
        m_table->setItem(row, ColValue, valItem);

        // Unit
        auto* unitItem = new QTableWidgetItem(QString::fromStdString(p.unit));
        m_table->setItem(row, ColUnit, unitItem);

        // Comment
        auto* commentItem = new QTableWidgetItem(QString::fromStdString(p.comment));
        m_table->setItem(row, ColComment, commentItem);
    }

    m_populating = false;
}

// ---------------------------------------------------------------------------
void ParameterTablePanel::onAddParameter()
{
    if (!m_doc)
        return;

    // Generate a unique default name
    auto& store = m_doc->parameters();
    std::string base = "param";
    int idx = 1;
    while (store.has(base + std::to_string(idx)))
        ++idx;
    std::string name = base + std::to_string(idx);

    try {
        store.set(name, "0", "mm");
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Error"), QString::fromStdString(e.what()));
        return;
    }

    // Also set the comment to empty (the Parameter struct default is fine)
    refresh();
    emit parameterChanged();

    // Start editing the name cell of the new row
    int lastRow = m_table->rowCount() - 1;
    // Find the row with this name
    for (int r = 0; r < m_table->rowCount(); ++r) {
        if (m_table->item(r, ColName)->text() == QString::fromStdString(name)) {
            lastRow = r;
            break;
        }
    }
    m_table->setCurrentCell(lastRow, ColName);
    m_table->editItem(m_table->item(lastRow, ColName));
}

// ---------------------------------------------------------------------------
void ParameterTablePanel::onCellChanged(int row, int col)
{
    if (m_populating || !m_doc)
        return;

    auto& store = m_doc->parameters();

    // Determine which parameter this row corresponds to.
    // We need to know the *original* name so we can look it up.
    // Collect sorted names again to match row indices.
    std::vector<std::string> sortedNames;
    for (const auto& [n, _] : store.all())
        sortedNames.push_back(n);
    std::sort(sortedNames.begin(), sortedNames.end());

    if (row < 0 || row >= static_cast<int>(sortedNames.size()))
        return;

    const std::string oldName = sortedNames[row];

    // Read current cell values
    QString newNameQ = m_table->item(row, ColName)->text().trimmed();
    QString exprQ    = m_table->item(row, ColExpression)->text().trimmed();
    QString unitQ    = m_table->item(row, ColUnit)->text().trimmed();
    QString commentQ = m_table->item(row, ColComment) ? m_table->item(row, ColComment)->text() : QString();

    std::string newName = newNameQ.toStdString();
    std::string expr    = exprQ.toStdString();
    std::string unit    = unitQ.toStdString();

    if (newName.empty()) {
        // Revert to old name
        m_populating = true;
        m_table->item(row, ColName)->setText(QString::fromStdString(oldName));
        m_populating = false;
        return;
    }

    try {
        if (col == ColName && newName != oldName) {
            // Rename: remove old, create new with same expression/unit/comment
            const auto& oldParam = store.all().at(oldName);
            std::string savedExpr    = oldParam.expression;
            std::string savedUnit    = oldParam.unit;
            std::string savedComment = oldParam.comment;
            store.remove(oldName);
            store.set(newName, savedExpr, savedUnit);
            // Restore comment -- access mutable parameter
            // (ParameterStore::set creates/updates but comment needs manual update)
            // We re-read after set since set() may have recreated the entry.
        } else if (col == ColExpression || col == ColUnit) {
            store.set(newName, expr, unit);
        }

        // Update value column
        m_populating = true;
        double val = store.get(newName);
        if (auto* valItem = m_table->item(row, ColValue)) {
            valItem->setText(QString::number(val, 'g', 10));
            valItem->setForeground(QBrush(QColor(0x90, 0xCA, 0xF9)));
        }
        m_populating = false;

        emit parameterChanged();

    } catch (const std::exception& e) {
        // Show error: red text in value column
        m_populating = true;
        if (auto* valItem = m_table->item(row, ColValue)) {
            valItem->setText(QString::fromStdString(e.what()));
            valItem->setForeground(QBrush(QColor(0xFF, 0x55, 0x55)));
        }
        m_populating = false;
    }
}

// ---------------------------------------------------------------------------
void ParameterTablePanel::onDeleteParameter()
{
    if (!m_doc)
        return;

    int row = m_table->currentRow();
    if (row < 0)
        return;

    auto* nameItem = m_table->item(row, ColName);
    if (!nameItem)
        return;

    std::string name = nameItem->text().toStdString();

    auto& store = m_doc->parameters();
    if (!store.has(name))
        return;

    // Confirm
    auto result = QMessageBox::question(
        this, tr("Delete Parameter"),
        tr("Delete parameter \"%1\"?").arg(QString::fromStdString(name)),
        QMessageBox::Yes | QMessageBox::No);

    if (result != QMessageBox::Yes)
        return;

    try {
        store.remove(name);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, tr("Error"), QString::fromStdString(e.what()));
        return;
    }

    refresh();
    emit parameterChanged();
}
