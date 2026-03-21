#pragma once
#include <QWidget>

class PropertiesPanel : public QWidget
{
    Q_OBJECT
public:
    explicit PropertiesPanel(QWidget* parent = nullptr);
    void showFeature(const QString& featureId);
    void clear();
};
