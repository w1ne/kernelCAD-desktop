#pragma once
#include <QTreeWidget>
#include <QIcon>
#include <QString>
#include "../features/Feature.h"

namespace document {
    class Document;
    class Component;
}

class FeatureTree : public QTreeWidget
{
    Q_OBJECT
public:
    explicit FeatureTree(QWidget* parent = nullptr);

    /// Return a 16x16 icon for the given feature type.
    static QIcon featureIcon(features::FeatureType type);

    /// Assign the document whose timeline drives tree contents.
    void setDocument(document::Document* doc);

    /// Rebuild the entire tree from the document's timeline.
    void refresh();

signals:
    void featureSelected(const QString& featureId);
    void featureEditRequested(const QString& featureId);
    void featureDeleteRequested(const QString& featureId);
    void featureSuppressToggled(const QString& featureId);
    void createComponentRequested(const QString& parentComponentId);
    void activateComponentRequested(const QString& componentId);
    void bodyVisibilityToggled(const QString& bodyId, bool visible);
    void bodyMaterialRequested(const QString& bodyId, const QString& materialName);
    void featureRenamed(const QString& featureId, const QString& newName);

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);
    void onItemRenamed(QTreeWidgetItem* item, int column);

private:
    document::Document* m_document = nullptr;

    /// True while refresh() is rebuilding items (suppresses itemChanged signals).
    bool m_refreshing = false;

    /// Recursively build tree items for a component and its children.
    void buildComponentTree(QTreeWidgetItem* parentItem,
                            const document::Component& comp);
};
