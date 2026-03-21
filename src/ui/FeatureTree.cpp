#include "FeatureTree.h"

FeatureTree::FeatureTree(QWidget* parent) : QTreeWidget(parent)
{
    setHeaderLabel("Browser");
    setContextMenuPolicy(Qt::CustomContextMenu);
}

void FeatureTree::refresh() { /* TODO: populate from document */ }
