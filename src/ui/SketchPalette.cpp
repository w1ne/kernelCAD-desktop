#include "SketchPalette.h"
#include "SketchEditor.h"
#include "../sketch/Sketch.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QEvent>
#include <QResizeEvent>

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

    // ── Title ─────────────────────────────────────────────────────────────
    auto* titleLabel = new QLabel(tr("SKETCH PALETTE"), this);
    titleLabel->setObjectName("paletteTitle");
    m_rootLayout->addWidget(titleLabel);

    // Separator
    auto* sep0 = new QFrame(this);
    sep0->setFrameShape(QFrame::HLine);
    sep0->setStyleSheet("color: #555;");
    sep0->setMaximumHeight(1);
    m_rootLayout->addWidget(sep0);

    // ── Options section ───────────────────────────────────────────────────
    auto* optHeader = new QLabel(tr("Options"), this);
    optHeader->setObjectName("sectionHeader");
    m_rootLayout->addWidget(optHeader);

    m_snapToGrid = new QCheckBox(tr("Snap to Grid"), this);
    m_snapToGrid->setChecked(true);
    m_rootLayout->addWidget(m_snapToGrid);

    m_showInference = new QCheckBox(tr("Show Inference Lines"), this);
    m_showInference->setChecked(true);
    m_rootLayout->addWidget(m_showInference);

    // Separator
    auto* sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet("color: #444;");
    sep1->setMaximumHeight(1);
    m_rootLayout->addWidget(sep1);

    // ── Display section ───────────────────────────────────────────────────
    auto* dispHeader = new QLabel(tr("Display"), this);
    dispHeader->setObjectName("sectionHeader");
    m_rootLayout->addWidget(dispHeader);

    m_showPoints = new QCheckBox(tr("Show Points"), this);
    m_showPoints->setChecked(true);
    m_rootLayout->addWidget(m_showPoints);

    m_showDimensions = new QCheckBox(tr("Show Dimensions"), this);
    m_showDimensions->setChecked(true);
    m_rootLayout->addWidget(m_showDimensions);

    m_showConstraints = new QCheckBox(tr("Show Constraints"), this);
    m_showConstraints->setChecked(true);
    m_rootLayout->addWidget(m_showConstraints);

    m_showConstruction = new QCheckBox(tr("Show Construction"), this);
    m_showConstruction->setChecked(true);
    m_rootLayout->addWidget(m_showConstruction);

    // Separator
    auto* sep2 = new QFrame(this);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet("color: #444;");
    sep2->setMaximumHeight(1);
    m_rootLayout->addWidget(sep2);

    // ── Sketch Info section ───────────────────────────────────────────────
    auto* infoHeader = new QLabel(tr("Sketch Info"), this);
    infoHeader->setObjectName("sectionHeader");
    m_rootLayout->addWidget(infoHeader);

    auto addStatRow = [this](const QString& label) -> QLabel* {
        auto* row = new QWidget(this);
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(4);
        auto* nameLabel = new QLabel(label, row);
        nameLabel->setObjectName("statName");
        auto* valueLabel = new QLabel("0", row);
        valueLabel->setObjectName("statValue");
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        hl->addWidget(nameLabel);
        hl->addStretch();
        hl->addWidget(valueLabel);
        m_rootLayout->addWidget(row);
        return valueLabel;
    };

    m_pointCount      = addStatRow(tr("Points"));
    m_lineCount       = addStatRow(tr("Lines"));
    m_constraintCount = addStatRow(tr("Constraints"));
    m_dofLabel        = addStatRow(tr("DOF"));

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
    setStyleSheet(
        "QWidget#SketchPalette {"
        "  background-color: rgba(42, 42, 42, 240);"
        "  border: 1px solid #3a3a3a;"
        "  border-radius: 6px;"
        "}"
        "QLabel {"
        "  color: #cccccc;"
        "  font-size: 11px;"
        "  background: transparent;"
        "}"
        "QLabel#paletteTitle {"
        "  color: #ffffff;"
        "  font-size: 12px;"
        "  font-weight: bold;"
        "}"
        "QLabel#sectionHeader {"
        "  color: #aaaaaa;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "}"
        "QLabel#statName {"
        "  color: #999999;"
        "}"
        "QLabel#statValue {"
        "  color: #cccccc;"
        "}"
        "QCheckBox {"
        "  color: #cccccc;"
        "  spacing: 6px;"
        "  font-size: 11px;"
        "}"
        "QCheckBox::indicator {"
        "  width: 14px; height: 14px;"
        "  border: 1px solid #555;"
        "  border-radius: 2px;"
        "  background-color: #333;"
        "}"
        "QCheckBox::indicator:checked {"
        "  background-color: #5294e2;"
        "  border-color: #5294e2;"
        "}"
        "QPushButton#finishButton {"
        "  background-color: #5294e2;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 4px;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "  padding: 8px;"
        "}"
        "QPushButton#finishButton:hover {"
        "  background-color: #6aa5ef;"
        "}"
        "QFrame {"
        "  max-height: 1px;"
        "}"
    );
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

bool SketchPalette::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        if (isVisible())
            repositionOverParent();
    }
    return QWidget::eventFilter(watched, event);
}
