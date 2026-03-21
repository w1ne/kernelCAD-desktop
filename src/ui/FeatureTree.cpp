#include "FeatureTree.h"
#include "../document/Document.h"
#include "../document/Timeline.h"
#include "../document/Origin.h"
#include "../document/Component.h"
#include "../features/Feature.h"
#include "../kernel/BRepModel.h"
#include "../kernel/Appearance.h"

#include <QMenu>
#include <QAction>
#include <QFont>
#include <QBrush>
#include <QColor>
#include <QHeaderView>
#include <QPixmap>
#include <QIcon>

// Custom role to distinguish component items from feature items
static constexpr int ComponentIdRole = Qt::UserRole + 1;
// Custom role to mark body items (for visibility checkbox)
static constexpr int BodyIdRole = Qt::UserRole + 2;

FeatureTree::FeatureTree(QWidget* parent) : QTreeWidget(parent)
{
    setHeaderLabel("Browser");
    setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this, &QTreeWidget::itemClicked,
            this, &FeatureTree::onItemClicked);
    connect(this, &QTreeWidget::itemDoubleClicked,
            this, &FeatureTree::onItemDoubleClicked);
    connect(this, &QTreeWidget::customContextMenuRequested,
            this, &FeatureTree::onContextMenu);
    connect(this, &QTreeWidget::itemChanged,
            this, [this](QTreeWidgetItem* item, int /*column*/) {
        QVariant bodyData = item->data(0, BodyIdRole);
        if (bodyData.isValid() && !bodyData.toString().isEmpty()) {
            bool visible = (item->checkState(0) == Qt::Checked);
            emit bodyVisibilityToggled(bodyData.toString(), visible);
        }
    });
}

void FeatureTree::setDocument(document::Document* doc)
{
    m_document = doc;
    refresh();
}

void FeatureTree::buildComponentTree(QTreeWidgetItem* parentItem,
                                     const document::Component& comp)
{
    auto& tl = m_document->timeline();
    const size_t markerPos = tl.markerPosition();

    // Origin group (only for root component)
    if (&comp == &m_document->components().rootComponent()) {
        auto* originItem = new QTreeWidgetItem(parentItem);
        originItem->setText(0, QStringLiteral("Origin"));

        const auto& orig = m_document->origin();
        for (auto* feat : orig.allFeatures()) {
            auto* child = new QTreeWidgetItem(originItem);
            child->setText(0, QString::fromStdString(feat->name()));
            child->setData(0, Qt::UserRole, QString::fromStdString(feat->id()));
        }
    }

    // Bodies group with visibility checkboxes
    const auto& bodyRefs = comp.bodyRefs();
    if (!bodyRefs.empty()) {
        auto* bodiesItem = new QTreeWidgetItem(parentItem);
        bodiesItem->setText(0, QStringLiteral("Bodies"));
        for (const auto& bodyId : bodyRefs) {
            auto* bodyItem = new QTreeWidgetItem(bodiesItem);
            bodyItem->setText(0, QString::fromStdString(bodyId));
            bodyItem->setData(0, Qt::UserRole, QString::fromStdString(bodyId));
            bodyItem->setData(0, BodyIdRole, QString::fromStdString(bodyId));

            // Add visibility checkbox
            bool visible = true;
            if (m_document) {
                visible = m_document->brepModel().isBodyVisible(bodyId);
            }
            bodyItem->setFlags(bodyItem->flags() | Qt::ItemIsUserCheckable);
            bodyItem->setCheckState(0, visible ? Qt::Checked : Qt::Unchecked);
        }
    }

    // Timeline features (only for root component for now -- features are global)
    if (&comp == &m_document->components().rootComponent()) {
        for (size_t i = 0; i < tl.count(); ++i) {
            const auto& entry = tl.entry(i);

            auto* item = new QTreeWidgetItem(parentItem);
            item->setText(0, QString::fromStdString(entry.name));
            item->setData(0, Qt::UserRole, QString::fromStdString(entry.id));

            // Suppressed features: italic gray
            if (entry.isSuppressed) {
                QFont font = item->font(0);
                font.setItalic(true);
                item->setFont(0, font);
                item->setForeground(0, QBrush(Qt::gray));
            }

            // Rolled-back features (past the marker): dimmed
            if (i >= markerPos) {
                item->setForeground(0, QBrush(QColor(180, 180, 180)));
            }

            // Error health state: red text
            if (entry.feature &&
                entry.feature->healthState() == features::HealthState::Error) {
                item->setForeground(0, QBrush(Qt::red));
            }
        }
    }

    // Child component occurrences
    for (const auto& occ : comp.occurrences()) {
        auto* occItem = new QTreeWidgetItem(parentItem);
        occItem->setText(0, QString::fromStdString(occ.name));
        occItem->setData(0, ComponentIdRole,
                         QString::fromStdString(occ.componentId));

        // Make component items bold
        QFont font = occItem->font(0);
        font.setBold(true);
        occItem->setFont(0, font);

        // If the occurrence is hidden, dim it
        if (!occ.isVisible) {
            occItem->setForeground(0, QBrush(QColor(160, 160, 160)));
        }

        // Recursively build the child component's subtree
        const auto* childComp =
            m_document->components().findComponent(occ.componentId);
        if (childComp) {
            buildComponentTree(occItem, *childComp);
        }
    }
}

void FeatureTree::refresh()
{
    clear(); // remove all items

    if (!m_document)
        return;

    // Top-level item: root component
    const auto& root = m_document->components().rootComponent();
    auto* rootItem = new QTreeWidgetItem(this);
    QString rootLabel = QString::fromStdString(m_document->name())
                        + " (" + QString::fromStdString(root.name()) + ")";
    rootItem->setText(0, rootLabel);
    rootItem->setData(0, ComponentIdRole, QString::fromStdString(root.id()));
    rootItem->setExpanded(true);

    // Make root item bold
    QFont font = rootItem->font(0);
    font.setBold(true);
    rootItem->setFont(0, font);

    // Mark active component
    if (root.isActive()) {
        rootItem->setForeground(0, QBrush(QColor(0, 120, 215)));
    }

    buildComponentTree(rootItem, root);

    expandAll();
}

void FeatureTree::onItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item)
        return;

    QVariant data = item->data(0, Qt::UserRole);
    if (data.isValid() && !data.toString().isEmpty())
        emit featureSelected(data.toString());
}

void FeatureTree::onItemDoubleClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item)
        return;

    // Only emit edit request for feature items (not component/body items)
    QVariant compData = item->data(0, ComponentIdRole);
    QVariant bodyData = item->data(0, BodyIdRole);
    if ((compData.isValid() && !compData.toString().isEmpty()) ||
        (bodyData.isValid() && !bodyData.toString().isEmpty()))
        return;

    QVariant data = item->data(0, Qt::UserRole);
    if (data.isValid() && !data.toString().isEmpty())
        emit featureEditRequested(data.toString());
}

void FeatureTree::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = itemAt(pos);
    if (!item)
        return;

    QMenu menu(this);

    // Check if this is a component item
    QVariant compData = item->data(0, ComponentIdRole);
    if (compData.isValid() && !compData.toString().isEmpty()) {
        const QString componentId = compData.toString();

        QAction* createCompAction = menu.addAction(tr("Create Component"));
        connect(createCompAction, &QAction::triggered, this, [this, componentId]() {
            emit createComponentRequested(componentId);
        });

        QAction* activateAction = menu.addAction(tr("Activate"));
        connect(activateAction, &QAction::triggered, this, [this, componentId]() {
            emit activateComponentRequested(componentId);
        });

        menu.addSeparator();
    }

    // Body-level context menu: "Set Material" submenu
    QVariant bodyData = item->data(0, BodyIdRole);
    if (bodyData.isValid() && !bodyData.toString().isEmpty()) {
        const QString bodyId = bodyData.toString();

        QMenu* matMenu = menu.addMenu(tr("Set Material"));
        const auto& allMats = kernel::MaterialLibrary::all();
        for (const auto& mat : allMats) {
            QString matName = QString::fromStdString(mat.name);
            QAction* matAction = matMenu->addAction(matName);

            // Show a color swatch icon
            QPixmap swatch(16, 16);
            swatch.fill(QColor::fromRgbF(mat.baseR, mat.baseG, mat.baseB));
            matAction->setIcon(QIcon(swatch));

            connect(matAction, &QAction::triggered, this, [this, bodyId, matName]() {
                emit bodyMaterialRequested(bodyId, matName);
            });
        }

        menu.addSeparator();
    }

    // Feature-level context menu
    QVariant data = item->data(0, Qt::UserRole);
    if (data.isValid() && !data.toString().isEmpty()) {
        const QString featureId = data.toString();

        // Determine if the feature is currently suppressed
        bool isSuppressed = false;
        if (m_document) {
            auto& tl = m_document->timeline();
            for (size_t i = 0; i < tl.count(); ++i) {
                if (QString::fromStdString(tl.entry(i).id) == featureId) {
                    isSuppressed = tl.entry(i).isSuppressed;
                    break;
                }
            }
        }

        QAction* suppressAction = menu.addAction(
            isSuppressed ? tr("Unsuppress") : tr("Suppress"));
        connect(suppressAction, &QAction::triggered, this, [this, featureId]() {
            emit featureSuppressToggled(featureId);
        });

        QAction* deleteAction = menu.addAction(tr("Delete"));
        connect(deleteAction, &QAction::triggered, this, [this, featureId]() {
            emit featureDeleteRequested(featureId);
        });

        QAction* editAction = menu.addAction(tr("Edit"));
        connect(editAction, &QAction::triggered, this, [this, featureId]() {
            emit featureEditRequested(featureId);
        });
    }

    if (!menu.isEmpty())
        menu.exec(viewport()->mapToGlobal(pos));
}
