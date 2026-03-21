#include "TimelinePanel.h"
#include <QHBoxLayout>
#include <QLabel>

TimelinePanel::TimelinePanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->addWidget(new QLabel("Timeline", this));
    setMinimumHeight(80);
    // TODO: scrollable list of TimelineEntry widgets + draggable marker
}

void TimelinePanel::refresh() {}
