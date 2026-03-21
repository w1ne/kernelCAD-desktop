#pragma once
#include <QTreeWidget>

class FeatureTree : public QTreeWidget
{
    Q_OBJECT
public:
    explicit FeatureTree(QWidget* parent = nullptr);
    void refresh(); // rebuild from document
};
