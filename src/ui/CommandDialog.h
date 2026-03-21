#pragma once
#include <QDialog>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <unordered_map>
#include <string>

namespace document { class InteractiveCommand; class Document; struct CommandInputValues; }

class CommandDialog : public QDialog
{
    Q_OBJECT
public:
    CommandDialog(document::InteractiveCommand* cmd,
                  document::Document* doc,
                  QWidget* parent = nullptr);

    document::CommandInputValues values() const;

signals:
    void inputChanged();

private:
    void buildForm();

    document::InteractiveCommand* m_cmd;
    document::Document* m_doc;
    QFormLayout* m_formLayout = nullptr;
    std::unordered_map<std::string, QWidget*> m_widgets;
    QTimer m_previewTimer;
};
