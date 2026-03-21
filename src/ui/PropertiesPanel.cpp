#include "PropertiesPanel.h"
#include <QVBoxLayout>
#include <QLabel>

PropertiesPanel::PropertiesPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Properties", this));
    layout->addStretch();
    // TODO: dynamic form driven by active feature/selection
}

void PropertiesPanel::showFeature(const QString&) {}
void PropertiesPanel::clear() {}
