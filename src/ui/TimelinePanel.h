#pragma once
#include <QWidget>

class TimelinePanel : public QWidget
{
    Q_OBJECT
public:
    explicit TimelinePanel(QWidget* parent = nullptr);
    void refresh();

signals:
    void markerMoved(int index);
    void entryDoubleClicked(const QString& featureId);
};
