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
#include <QKeyEvent>
#include <QFont>
#include <QBrush>
#include <QColor>
#include <QHeaderView>
#include <QPixmap>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

// Custom role to distinguish component items from feature items
static constexpr int ComponentIdRole = Qt::UserRole + 1;
// Custom role to mark body items (for visibility checkbox)
static constexpr int BodyIdRole = Qt::UserRole + 2;
// Custom role to mark feature items (for in-place rename)
static constexpr int FeatureIdRole = Qt::UserRole + 3;

// =====================================================================
// overlayHealthDot -- composite a small colored dot onto an icon
// =====================================================================

static QIcon overlayHealthDot(const QIcon& base, features::HealthState state)
{
    if (state == features::HealthState::Healthy)
        return base;  // no dot needed for healthy features

    QColor dotColor;
    switch (state) {
    case features::HealthState::Error:      dotColor = QColor(220, 50, 50);  break;
    case features::HealthState::Warning:    dotColor = QColor(220, 160, 30); break;
    case features::HealthState::Suppressed: dotColor = QColor(120, 120, 120); break;
    default:                                return base;
    }

    QPixmap pm = base.pixmap(16, 16);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(dotColor);
    p.drawEllipse(QPointF(12.5, 12.5), 3.0, 3.0);  // bottom-right corner
    return QIcon(pm);
}

// =====================================================================
// featureIcon -- draw simple 16x16 icons per feature type
// =====================================================================

QIcon FeatureTree::featureIcon(features::FeatureType type)
{
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    switch (type) {
    case features::FeatureType::Extrude: {
        // Blue upward arrow
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(80, 160, 255));
        // Arrow shaft
        p.drawRect(6, 5, 4, 9);
        // Arrow head
        QPainterPath tri;
        tri.moveTo(8, 1);
        tri.lineTo(3, 7);
        tri.lineTo(13, 7);
        tri.closeSubpath();
        p.drawPath(tri);
        break;
    }
    case features::FeatureType::Revolve: {
        // Green circular arrow
        p.setPen(QPen(QColor(80, 200, 80), 2));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRect(2, 2, 12, 12), 30 * 16, 270 * 16);
        // Arrow tip
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(80, 200, 80));
        QPainterPath tip;
        tip.moveTo(11, 2);
        tip.lineTo(14, 5);
        tip.lineTo(9, 5);
        tip.closeSubpath();
        p.drawPath(tip);
        break;
    }
    case features::FeatureType::Fillet: {
        // Orange quarter-circle
        p.setPen(QPen(QColor(255, 160, 60), 2.5));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRect(-2, -2, 16, 16), 0, 90 * 16);
        // Corner lines
        p.setPen(QPen(QColor(255, 160, 60), 1.2));
        p.drawLine(2, 14, 2, 6);
        p.drawLine(2, 14, 10, 14);
        break;
    }
    case features::FeatureType::Chamfer: {
        // Orange diagonal line with corner
        p.setPen(QPen(QColor(255, 140, 40), 1.5));
        p.drawLine(2, 14, 2, 7);
        p.drawLine(2, 7, 9, 14);
        p.drawLine(9, 14, 14, 14);
        break;
    }
    case features::FeatureType::Sketch: {
        // Purple pencil shape
        p.setPen(QPen(QColor(180, 100, 220), 1.8));
        p.drawLine(3, 13, 13, 3);
        p.drawLine(13, 3, 11, 1);
        p.drawLine(11, 1, 1, 11);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(180, 100, 220));
        QPainterPath nib;
        nib.moveTo(1, 11);
        nib.lineTo(3, 13);
        nib.lineTo(1, 15);
        nib.closeSubpath();
        p.drawPath(nib);
        break;
    }
    case features::FeatureType::Hole: {
        // Red circle with inner hole
        p.setPen(QPen(QColor(220, 60, 60), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRect(2, 2, 12, 12));
        p.setBrush(QColor(220, 60, 60, 80));
        p.drawEllipse(QRect(5, 5, 6, 6));
        break;
    }
    case features::FeatureType::Shell: {
        // Hollow box outline (teal)
        p.setPen(QPen(QColor(80, 200, 200), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRect(2, 2, 12, 12);
        p.drawRect(5, 5, 6, 6);
        break;
    }
    case features::FeatureType::Mirror: {
        // Dashed vertical line with symmetric shapes
        p.setPen(QPen(QColor(100, 180, 255), 1, Qt::DashLine));
        p.drawLine(8, 1, 8, 15);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(100, 180, 255));
        p.drawRect(2, 5, 4, 6);
        p.setBrush(QColor(100, 180, 255, 120));
        p.drawRect(10, 5, 4, 6);
        break;
    }
    case features::FeatureType::Sweep: {
        // Curved path with circle (teal)
        p.setPen(QPen(QColor(80, 200, 180), 2));
        p.setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(2, 12);
        path.cubicTo(4, 4, 12, 4, 14, 12);
        p.drawPath(path);
        p.setBrush(QColor(80, 200, 180));
        p.drawEllipse(QPoint(2, 12), 2, 2);
        break;
    }
    case features::FeatureType::Loft: {
        // Two horizontal lines connected (teal)
        p.setPen(QPen(QColor(80, 200, 180), 1.5));
        p.drawLine(4, 3, 12, 3);
        p.drawLine(2, 13, 14, 13);
        p.setPen(QPen(QColor(80, 200, 180, 120), 1, Qt::DashLine));
        p.drawLine(4, 3, 2, 13);
        p.drawLine(12, 3, 14, 13);
        break;
    }
    case features::FeatureType::Thread: {
        // Helix lines (brown)
        p.setPen(QPen(QColor(180, 140, 80), 1.5));
        for (int y = 3; y <= 13; y += 3) {
            p.drawLine(4, y, 12, y + 1);
        }
        break;
    }
    case features::FeatureType::Combine: {
        // Overlapping rectangles (yellow)
        p.setPen(QPen(QColor(220, 200, 60), 1.5));
        p.setBrush(QColor(220, 200, 60, 60));
        p.drawRect(1, 1, 9, 9);
        p.drawRect(6, 6, 9, 9);
        break;
    }
    case features::FeatureType::Move: {
        // Cross arrows (gray-blue)
        p.setPen(QPen(QColor(140, 170, 210), 2));
        p.drawLine(8, 2, 8, 14);
        p.drawLine(2, 8, 14, 8);
        break;
    }
    case features::FeatureType::Scale: {
        // Nested squares (purple)
        p.setPen(QPen(QColor(160, 120, 200), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRect(1, 1, 14, 14);
        p.drawRect(4, 4, 8, 8);
        break;
    }
    case features::FeatureType::Draft: {
        // Angled trapezoid (orange)
        p.setPen(QPen(QColor(255, 160, 60), 1.5));
        p.setBrush(QColor(255, 160, 60, 40));
        QPolygonF trap;
        trap << QPointF(4, 2) << QPointF(12, 2) << QPointF(14, 14) << QPointF(2, 14);
        p.drawPolygon(trap);
        break;
    }
    case features::FeatureType::Joint: {
        // Link icon (green)
        p.setPen(QPen(QColor(80, 200, 80), 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRect(1, 1, 8, 8));
        p.drawEllipse(QRect(7, 7, 8, 8));
        break;
    }
    case features::FeatureType::ConstructionPlane: {
        // Colored filled circle -- plane icon
        // Determine color by name later; default blue for XY
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(80, 140, 255));
        p.drawEllipse(QRect(2, 2, 12, 12));
        break;
    }
    case features::FeatureType::ConstructionAxis: {
        // Red line icon for axis
        p.setPen(QPen(QColor(220, 80, 80), 2));
        p.drawLine(3, 8, 13, 8);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(220, 80, 80));
        p.drawEllipse(QRect(1, 6, 4, 4));
        p.drawEllipse(QRect(11, 6, 4, 4));
        break;
    }
    case features::FeatureType::ConstructionPoint: {
        // Orange dot
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 160, 60));
        p.drawEllipse(QRect(4, 4, 8, 8));
        break;
    }
    default: {
        // Generic feature: gray diamond
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(160, 165, 170));
        QPolygonF diamond;
        diamond << QPointF(8, 1) << QPointF(15, 8) << QPointF(8, 15) << QPointF(1, 8);
        p.drawPolygon(diamond);
        break;
    }
    }

    p.end();
    return QIcon(pm);
}

// =====================================================================

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

    // Body visibility checkbox changes
    connect(this, &QTreeWidget::itemChanged,
            this, [this](QTreeWidgetItem* item, int /*column*/) {
        if (m_refreshing)
            return;
        QVariant bodyData = item->data(0, BodyIdRole);
        if (bodyData.isValid() && !bodyData.toString().isEmpty()) {
            bool visible = (item->checkState(0) == Qt::Checked);
            emit bodyVisibilityToggled(bodyData.toString(), visible);
            return;
        }
    });

    // In-place rename for feature items
    connect(this, &QTreeWidget::itemChanged,
            this, &FeatureTree::onItemRenamed);
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
            child->setIcon(0, featureIcon(feat->type()));
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
            // Use displayName (custom name if set, else auto-generated numbered name)
            item->setText(0, QString::fromStdString(entry.displayName()));
            item->setData(0, Qt::UserRole, QString::fromStdString(entry.id));
            item->setData(0, FeatureIdRole, QString::fromStdString(entry.id));

            // Set feature type icon with health indicator dot
            if (entry.feature) {
                QIcon icon = featureIcon(entry.feature->type());
                features::HealthState health = entry.feature->healthState();
                if (entry.isSuppressed)
                    health = features::HealthState::Suppressed;
                item->setIcon(0, overlayHealthDot(icon, health));
            }

            // Make feature items editable (in-place rename)
            item->setFlags(item->flags() | Qt::ItemIsEditable);

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
    m_refreshing = true;
    clear(); // remove all items

    if (!m_document) {
        m_refreshing = false;
        return;
    }

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

    // Re-apply the current tab filter after rebuilding
    if (m_currentTab != BrowserTab::Model)
        applyFilter(m_currentTab);

    m_refreshing = false;
}

// =====================================================================
// applyFilter -- show/hide tree items based on the selected browser tab
// =====================================================================

void FeatureTree::applyFilter(BrowserTab tab)
{
    m_currentTab = tab;

    if (!m_document || topLevelItemCount() == 0)
        return;

    QTreeWidgetItem* rootItem = topLevelItem(0);

    // "Model" tab: show everything
    if (tab == BrowserTab::Model) {
        for (int i = 0; i < rootItem->childCount(); ++i)
            rootItem->child(i)->setHidden(false);
        return;
    }

    // For other tabs, iterate children of root and show/hide based on type
    for (int i = 0; i < rootItem->childCount(); ++i) {
        QTreeWidgetItem* child = rootItem->child(i);
        QString text = child->text(0);

        bool show = false;
        switch (tab) {
        case BrowserTab::Bodies:
            show = (text == QStringLiteral("Bodies"));
            break;
        case BrowserTab::Sketches:
            // Show items that are sketch features or the "Origin" group
            if (text == QStringLiteral("Origin")) {
                show = false;
            } else if (text == QStringLiteral("Bodies")) {
                show = false;
            } else {
                // Check if this is a sketch feature
                QVariant featData = child->data(0, Qt::UserRole);
                if (featData.isValid() && !featData.toString().isEmpty() && m_document) {
                    auto& tl = m_document->timeline();
                    for (size_t j = 0; j < tl.count(); ++j) {
                        if (QString::fromStdString(tl.entry(j).id) == featData.toString()) {
                            show = (tl.entry(j).feature &&
                                    tl.entry(j).feature->type() == features::FeatureType::Sketch);
                            break;
                        }
                    }
                }
            }
            break;
        case BrowserTab::Components: {
            // Show only component occurrence items (those with ComponentIdRole)
            QVariant compData = child->data(0, ComponentIdRole);
            show = compData.isValid() && !compData.toString().isEmpty()
                   && child != rootItem;  // exclude root itself
            break;
        }
        default:
            show = true;
            break;
        }

        child->setHidden(!show);
    }
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

    // Double-click on body item -> isolate view
    QVariant bodyData = item->data(0, BodyIdRole);
    if (bodyData.isValid() && !bodyData.toString().isEmpty()) {
        emit bodyIsolateRequested(bodyData.toString());
        return;
    }

    // Only emit edit request for feature items (not component items)
    QVariant compData = item->data(0, ComponentIdRole);
    if (compData.isValid() && !compData.toString().isEmpty())
        return;

    QVariant data = item->data(0, Qt::UserRole);
    if (data.isValid() && !data.toString().isEmpty())
        emit featureEditRequested(data.toString());
}

void FeatureTree::onItemRenamed(QTreeWidgetItem* item, int column)
{
    if (m_refreshing || !item || column != 0)
        return;

    // Only handle rename for feature items (those with FeatureIdRole)
    QVariant featData = item->data(0, FeatureIdRole);
    if (!featData.isValid() || featData.toString().isEmpty())
        return;

    QString featureId = featData.toString();
    QString newText = item->text(0).trimmed();
    if (newText.isEmpty())
        return;

    emit featureRenamed(featureId, newText);
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

        // Rename action
        QAction* renameAction = menu.addAction(tr("Rename"));
        connect(renameAction, &QAction::triggered, this, [this, item]() {
            editItem(item, 0);
        });

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

void FeatureTree::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F2) {
        QTreeWidgetItem* item = currentItem();
        if (item && (item->flags() & Qt::ItemIsEditable)) {
            editItem(item, 0);
        }
        return;
    }
    QTreeWidget::keyPressEvent(event);
}
