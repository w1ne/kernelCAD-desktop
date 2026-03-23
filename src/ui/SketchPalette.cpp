#include "SketchPalette.h"
#include "SketchEditor.h"
#include "../sketch/Sketch.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QPalette>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QEvent>
#include <QResizeEvent>
#include <QMouseEvent>

// =============================================================================
// Construction
// =============================================================================

SketchPalette::SketchPalette(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("SketchPalette");
    setAttribute(Qt::WA_StyledBackground, true);

    buildUI();
    applyStyleSheet();

    // Drop shadow for floating appearance
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(16);
    shadow->setOffset(0, 3);
    shadow->setColor(QColor(0, 0, 0, 140));
    setGraphicsEffect(shadow);

    setVisible(false);

    // Track parent resizes to reposition
    if (parent)
        parent->installEventFilter(this);
}

// =============================================================================
// Build UI
// =============================================================================

void SketchPalette::buildUI()
{
    setFixedWidth(200);

    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(12, 10, 12, 12);
    m_rootLayout->setSpacing(6);

    // Helper to force white/light text on any widget
    auto setLightText = [](QWidget* w, const QColor& color = QColor(204, 204, 204)) {
        QPalette pal = w->palette();
        pal.setColor(QPalette::WindowText, color);
        pal.setColor(QPalette::Text, color);
        pal.setColor(QPalette::ButtonText, color);
        w->setPalette(pal);
    };

    // ── Title ─────────────────────────────────────────────────────────────
    auto* titleLabel = new QLabel(tr("SKETCH PALETTE"), this);
    titleLabel->setObjectName("paletteTitle");
    setLightText(titleLabel, QColor(255, 255, 255));
    m_rootLayout->addWidget(titleLabel);

    // Separator
    auto* sep0 = new QFrame(this);
    sep0->setFrameShape(QFrame::HLine);
    sep0->setStyleSheet("color: #555;");
    sep0->setMaximumHeight(1);
    m_rootLayout->addWidget(sep0);

    // Helper: create a collapsible section header + content widget pair
    auto makeCollapsible = [this](const QString& title) -> std::pair<QPushButton*, QWidget*> {
        auto* header = new QPushButton(QString::fromUtf8("\u25BC ") + title, this);
        header->setFlat(true);
        header->setStyleSheet(
            "text-align: left; color: #aaa; font-weight: bold; font-size: 11px;"
            " border: none; padding: 2px 0; background: transparent;");
        header->setCursor(Qt::PointingHandCursor);
        m_rootLayout->addWidget(header);

        auto* content = new QWidget(this);
        auto* contentLayout = new QVBoxLayout(content);
        contentLayout->setContentsMargins(0, 0, 0, 0);
        contentLayout->setSpacing(4);
        m_rootLayout->addWidget(content);

        connect(header, &QPushButton::clicked, [header, content, title]() {
            bool visible = !content->isVisible();
            content->setVisible(visible);
            header->setText((visible ? QString::fromUtf8("\u25BC ") : QString::fromUtf8("\u25B6 ")) + title);
        });

        return {header, content};
    };

    // ── Options section (collapsible) ─────────────────────────────────────
    {
        auto [header, content] = makeCollapsible(tr("Options"));
        auto* cl = qobject_cast<QVBoxLayout*>(content->layout());

        m_snapToGrid = new QCheckBox(tr("Snap to Grid"), content);
        m_snapToGrid->setChecked(true);
        setLightText(m_snapToGrid);
        cl->addWidget(m_snapToGrid);

        m_showInference = new QCheckBox(tr("Show Inference Lines"), content);
        m_showInference->setChecked(true);
        setLightText(m_showInference);
        cl->addWidget(m_showInference);
    }

    // Separator
    auto* sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet("color: #444;");
    sep1->setMaximumHeight(1);
    m_rootLayout->addWidget(sep1);

    // ── Display section (collapsible) ─────────────────────────────────────
    {
        auto [header, content] = makeCollapsible(tr("Display"));
        auto* cl = qobject_cast<QVBoxLayout*>(content->layout());

        m_showPoints = new QCheckBox(tr("Show Points"), content);
        m_showPoints->setChecked(true);
        setLightText(m_showPoints);
        cl->addWidget(m_showPoints);

        m_showDimensions = new QCheckBox(tr("Show Dimensions"), content);
        m_showDimensions->setChecked(true);
        setLightText(m_showDimensions);
        cl->addWidget(m_showDimensions);

        m_showConstraints = new QCheckBox(tr("Show Constraints"), content);
        m_showConstraints->setChecked(true);
        setLightText(m_showConstraints);
        cl->addWidget(m_showConstraints);

        m_showConstruction = new QCheckBox(tr("Show Construction"), content);
        m_showConstruction->setChecked(true);
        setLightText(m_showConstruction);
        cl->addWidget(m_showConstruction);
    }

    // Separator
    auto* sep2 = new QFrame(this);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet("color: #444;");
    sep2->setMaximumHeight(1);
    m_rootLayout->addWidget(sep2);

    // ── Sketch Info section (collapsible) ─────────────────────────────────
    {
        auto [header, content] = makeCollapsible(tr("Sketch Info"));
        (void)header;
        auto* cl = qobject_cast<QVBoxLayout*>(content->layout());

        auto addStatRow = [cl, &setLightText](QWidget* parent, const QString& label) -> QLabel* {
            auto* row = new QWidget(parent);
            auto* hl = new QHBoxLayout(row);
            hl->setContentsMargins(0, 0, 0, 0);
            hl->setSpacing(4);
            auto* nameLabel = new QLabel(label, row);
            nameLabel->setObjectName("statName");
            setLightText(nameLabel, QColor(153, 153, 153));
            auto* valueLabel = new QLabel("0", row);
            valueLabel->setObjectName("statValue");
            setLightText(valueLabel, QColor(204, 204, 204));
            valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            hl->addWidget(nameLabel);
            hl->addStretch();
            hl->addWidget(valueLabel);
            cl->addWidget(row);
            return valueLabel;
        };

        m_pointCount      = addStatRow(content, tr("Points"));
        m_lineCount       = addStatRow(content, tr("Lines"));
        m_constraintCount = addStatRow(content, tr("Constraints"));
        m_dofLabel        = addStatRow(content, tr("DOF"));
    }

    // Separator
    auto* sep3 = new QFrame(this);
    sep3->setFrameShape(QFrame::HLine);
    sep3->setStyleSheet("color: #444;");
    sep3->setMaximumHeight(1);
    m_rootLayout->addWidget(sep3);

    // ── Finish Sketch button ──────────────────────────────────────────────
    m_finishButton = new QPushButton(tr("Finish Sketch"), this);
    m_finishButton->setObjectName("finishButton");
    m_finishButton->setFixedHeight(32);
    m_finishButton->setStyleSheet(
        "QPushButton { background-color: #5294e2; color: white; border: none; "
        "border-radius: 4px; font-weight: bold; font-size: 12px; }"
        "QPushButton:hover { background-color: #6aa5ef; }");
    m_rootLayout->addWidget(m_finishButton);

    // ── Signal connections ────────────────────────────────────────────────
    connect(m_snapToGrid, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_editor)
            m_editor->setGridSnap(checked);
        emit settingChanged(QStringLiteral("Snap to Grid"), checked);
    });
    connect(m_showInference, &QCheckBox::toggled, this, [this](bool checked) {
        emit settingChanged(QStringLiteral("Show Inference Lines"), checked);
    });
    connect(m_showPoints, &QCheckBox::toggled, this, [this](bool checked) {
        emit settingChanged(QStringLiteral("Show Points"), checked);
    });
    connect(m_showDimensions, &QCheckBox::toggled, this, [this](bool checked) {
        emit settingChanged(QStringLiteral("Show Dimensions"), checked);
    });
    connect(m_showConstraints, &QCheckBox::toggled, this, [this](bool checked) {
        emit settingChanged(QStringLiteral("Show Constraints"), checked);
    });
    connect(m_showConstruction, &QCheckBox::toggled, this, [this](bool checked) {
        emit settingChanged(QStringLiteral("Show Construction"), checked);
    });
    connect(m_finishButton, &QPushButton::clicked, this, [this]() {
        emit finishSketchClicked();
    });
}

// =============================================================================
// Style sheet
// =============================================================================

void SketchPalette::applyStyleSheet()
{
    // Use inline styles on the widget itself — avoid CSS selectors that
    // get overridden by GTK platform theme on some Linux distributions.
    setStyleSheet(
        "background-color: rgba(42, 42, 42, 240);"
        "border: 1px solid #3a3a3a;"
        "border-radius: 6px;"
    );

    // Force all child widgets to use light text via palette
    QPalette widgetPal = palette();
    widgetPal.setColor(QPalette::WindowText, QColor(204, 204, 204));
    widgetPal.setColor(QPalette::Text, QColor(204, 204, 204));
    widgetPal.setColor(QPalette::ButtonText, QColor(204, 204, 204));
    widgetPal.setColor(QPalette::Window, QColor(42, 42, 42));
    widgetPal.setColor(QPalette::Base, QColor(51, 51, 51));
    widgetPal.setColor(QPalette::Button, QColor(51, 51, 51));
    setPalette(widgetPal);
}

// =============================================================================
// Show / Dismiss
// =============================================================================

void SketchPalette::showForSketch(sketch::Sketch* sketch, SketchEditor* editor)
{
    m_sketch = sketch;
    m_editor = editor;

    if (m_snapToGrid && editor)
        m_snapToGrid->setChecked(editor->gridSnapEnabled());

    refresh();
    adjustSize();
    repositionOverParent();
    raise();
    setVisible(true);
}

void SketchPalette::dismiss()
{
    m_sketch = nullptr;
    m_editor = nullptr;
    setVisible(false);
}

// =============================================================================
// Refresh stats
// =============================================================================

void SketchPalette::refresh()
{
    if (!m_sketch)
        return;

    const auto& sk = *m_sketch;
    if (m_pointCount)
        m_pointCount->setText(QString::number(static_cast<int>(sk.points().size())));
    if (m_lineCount)
        m_lineCount->setText(QString::number(static_cast<int>(sk.lines().size())));
    if (m_constraintCount)
        m_constraintCount->setText(QString::number(static_cast<int>(sk.constraints().size())));

    if (m_dofLabel) {
        int dof = sk.freeDOF();
        if (dof == 0) {
            m_dofLabel->setText(QStringLiteral("0 (Locked)"));
            m_dofLabel->setStyleSheet("color: #4caf50; background: transparent;");
        } else {
            m_dofLabel->setText(QString::number(dof));
            m_dofLabel->setStyleSheet("color: #ff9800; background: transparent;");
        }
    }
}

// =============================================================================
// Positioning
// =============================================================================

void SketchPalette::repositionOverParent()
{
    if (!parentWidget())
        return;
    int x = parentWidget()->width() - width() - 20;
    int y = parentWidget()->height() / 2 - height() / 2;
    // Clamp so it does not go off-screen
    if (y < 20) y = 20;
    move(x, y);
}

// =============================================================================
// Drag by title area
// =============================================================================

void SketchPalette::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && event->pos().y() < 30) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void SketchPalette::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
    } else {
        QWidget::mouseMoveEvent(event);
    }
}

void SketchPalette::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragging) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
    } else {
        QWidget::mouseReleaseEvent(event);
    }
}

bool SketchPalette::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        if (isVisible() && !m_dragging)
            repositionOverParent();
    }
    return QWidget::eventFilter(watched, event);
}
