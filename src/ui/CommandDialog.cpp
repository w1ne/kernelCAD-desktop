#include "CommandDialog.h"
#include "../document/CommandInput.h"
#include "../document/Document.h"

#include <QVBoxLayout>
#include <QDialogButtonBox>

CommandDialog::CommandDialog(document::InteractiveCommand* cmd,
                             document::Document* doc,
                             QWidget* parent)
    : QDialog(parent), m_cmd(cmd), m_doc(doc)
{
    setWindowTitle(QString::fromStdString(cmd->description()));
    setMinimumWidth(320);

    auto* rootLayout = new QVBoxLayout(this);
    m_formLayout = new QFormLayout;
    rootLayout->addLayout(m_formLayout);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    rootLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(50);
    connect(&m_previewTimer, &QTimer::timeout, this, [this]() {
        auto vals = values();
        m_cmd->preview(*m_doc, vals);
    });

    buildForm();
}

void CommandDialog::buildForm()
{
    auto defs = m_cmd->inputDefinitions();
    for (const auto& def : defs) {
        QWidget* widget = nullptr;
        switch (def.type) {
        case document::CommandInputType::Value:
        case document::CommandInputType::Distance:
        case document::CommandInputType::Angle:
        case document::CommandInputType::Slider: {
            auto* spin = new QDoubleSpinBox(this);
            spin->setRange(def.minValue, def.maxValue);
            spin->setValue(def.defaultValue);
            spin->setDecimals(def.type == document::CommandInputType::Angle ? 1 : 3);
            if (def.type == document::CommandInputType::Distance) spin->setSuffix(" mm");
            if (def.type == document::CommandInputType::Angle) spin->setSuffix(" °");
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this]() { m_previewTimer.start(); emit inputChanged(); });
            widget = spin;
            break;
        }
        case document::CommandInputType::DropDown: {
            auto* combo = new QComboBox(this);
            for (const auto& item : def.dropdownItems)
                combo->addItem(QString::fromStdString(item));
            combo->setCurrentIndex(def.defaultDropdownIndex);
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, [this]() { m_previewTimer.start(); emit inputChanged(); });
            widget = combo;
            break;
        }
        case document::CommandInputType::CheckBox: {
            auto* cb = new QCheckBox(this);
            cb->setChecked(def.defaultBool);
            connect(cb, &QCheckBox::toggled,
                    this, [this]() { m_previewTimer.start(); emit inputChanged(); });
            widget = cb;
            break;
        }
        case document::CommandInputType::TextBox: {
            auto* le = new QLineEdit(QString::fromStdString(def.defaultText), this);
            connect(le, &QLineEdit::textChanged,
                    this, [this]() { m_previewTimer.start(); emit inputChanged(); });
            widget = le;
            break;
        }
        case document::CommandInputType::Selection: {
            auto* label = new QLabel("Click to select...", this);
            widget = label;
            break;
        }
        case document::CommandInputType::Direction: {
            auto* label = new QLabel("Pick direction...", this);
            widget = label;
            break;
        }
        }

        if (widget) {
            m_formLayout->addRow(QString::fromStdString(def.label), widget);
            m_widgets[def.id] = widget;
        }
    }
}

document::CommandInputValues CommandDialog::values() const
{
    document::CommandInputValues vals;
    auto defs = m_cmd->inputDefinitions();
    for (const auto& def : defs) {
        auto it = m_widgets.find(def.id);
        if (it == m_widgets.end()) continue;

        switch (def.type) {
        case document::CommandInputType::Value:
        case document::CommandInputType::Distance:
        case document::CommandInputType::Angle:
        case document::CommandInputType::Slider:
            if (auto* spin = qobject_cast<QDoubleSpinBox*>(it->second))
                vals.numericValues[def.id] = spin->value();
            break;
        case document::CommandInputType::DropDown:
            if (auto* combo = qobject_cast<QComboBox*>(it->second))
                vals.intValues[def.id] = combo->currentIndex();
            break;
        case document::CommandInputType::CheckBox:
            if (auto* cb = qobject_cast<QCheckBox*>(it->second))
                vals.boolValues[def.id] = cb->isChecked();
            break;
        case document::CommandInputType::TextBox:
            if (auto* le = qobject_cast<QLineEdit*>(it->second))
                vals.stringValues[def.id] = le->text().toStdString();
            break;
        default:
            break;
        }
    }
    return vals;
}
