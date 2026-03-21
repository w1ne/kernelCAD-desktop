#include "FeatureDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QEvent>
#include <QResizeEvent>

// =============================================================================
// Construction
// =============================================================================

FeatureDialog::FeatureDialog(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("FeatureDialog");
    // Non-modal floating widget parented to the viewport
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::StrongFocus);

    setupLayout();
    applyStyleSheet();

    // Drop shadow
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 160));
    setGraphicsEffect(shadow);

    setVisible(false);

    // Install event filter on parent to track resizes
    if (parent)
        parent->installEventFilter(this);
}

// =============================================================================
// Layout skeleton
// =============================================================================

void FeatureDialog::setupLayout()
{
    setFixedWidth(280);

    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(14, 10, 14, 12);
    m_rootLayout->setSpacing(10);

    // Title
    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName("dialogTitle");
    m_rootLayout->addWidget(m_titleLabel);

    // Separator line
    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("color: #555;");
    m_rootLayout->addWidget(separator);

    // Form area
    m_formWidget = new QWidget(this);
    m_formLayout = new QFormLayout(m_formWidget);
    m_formLayout->setContentsMargins(0, 0, 0, 0);
    m_formLayout->setSpacing(8);
    m_formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_rootLayout->addWidget(m_formWidget);

    // Button row
    auto* btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(8);
    btnLayout->addStretch();

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    m_cancelButton->setObjectName("cancelButton");
    m_cancelButton->setFixedHeight(30);
    m_cancelButton->setMinimumWidth(70);
    btnLayout->addWidget(m_cancelButton);

    m_okButton = new QPushButton(tr("OK"), this);
    m_okButton->setObjectName("okButton");
    m_okButton->setFixedHeight(30);
    m_okButton->setMinimumWidth(70);
    m_okButton->setDefault(true);
    btnLayout->addWidget(m_okButton);

    m_rootLayout->addLayout(btnLayout);

    // Wire buttons
    connect(m_okButton, &QPushButton::clicked, this, [this]() {
        emitAccepted();
        dismiss();
    });
    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        emit cancelled();
        dismiss();
    });
}

// =============================================================================
// Style sheet
// =============================================================================

void FeatureDialog::applyStyleSheet()
{
    setStyleSheet(
        "QWidget#FeatureDialog {"
        "  background-color: rgba(45, 45, 45, 242);"
        "  border: 1px solid #555;"
        "  border-radius: 6px;"
        "}"
        "QWidget#FeatureDialog QLabel {"
        "  color: #ccc;"
        "  font-size: 11px;"
        "}"
        "QWidget#FeatureDialog QLabel#dialogTitle {"
        "  color: #fff;"
        "  font-size: 12px;"
        "  font-weight: bold;"
        "}"
        "QWidget#FeatureDialog QDoubleSpinBox,"
        "QWidget#FeatureDialog QComboBox {"
        "  background: #3c3f41;"
        "  color: #e0e0e0;"
        "  border: 1px solid #555;"
        "  border-radius: 3px;"
        "  padding: 4px;"
        "  min-height: 24px;"
        "}"
        "QWidget#FeatureDialog QDoubleSpinBox:focus,"
        "QWidget#FeatureDialog QComboBox:focus {"
        "  border: 1px solid #5294e2;"
        "}"
        "QWidget#FeatureDialog QCheckBox {"
        "  color: #ccc;"
        "  spacing: 6px;"
        "}"
        "QWidget#FeatureDialog QCheckBox::indicator {"
        "  width: 16px; height: 16px;"
        "}"
        "QWidget#FeatureDialog QPushButton#okButton {"
        "  background: #5294e2;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 6px 20px;"
        "  font-weight: bold;"
        "}"
        "QWidget#FeatureDialog QPushButton#okButton:hover {"
        "  background: #6aa8f0;"
        "}"
        "QWidget#FeatureDialog QPushButton#cancelButton {"
        "  background: #555;"
        "  color: #ccc;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 6px 20px;"
        "}"
        "QWidget#FeatureDialog QPushButton#cancelButton:hover {"
        "  background: #666;"
        "}"
        "QWidget#FeatureDialog QFrame {"
        "  max-height: 1px;"
        "}"
    );
}

// =============================================================================
// Clear / Dismiss
// =============================================================================

void FeatureDialog::clearForm()
{
    // Remove all rows from the form layout
    while (m_formLayout->rowCount() > 0)
        m_formLayout->removeRow(0);

    // Reset all field pointers
    m_profileLabel = nullptr;
    m_directionCombo = nullptr;
    m_extentTypeCombo = nullptr;
    m_distanceSpin = nullptr;
    m_taperAngleSpin = nullptr;
    m_operationCombo = nullptr;
    m_radiusSpin = nullptr;
    m_tangentChainCheck = nullptr;
    m_edgeCountLabel = nullptr;
    m_chamferDistSpin = nullptr;
    m_chamferTypeCombo = nullptr;
    m_chamferEdgeLabel = nullptr;
    m_thicknessSpin = nullptr;
    m_shellDirCombo = nullptr;
    m_faceCountLabel = nullptr;
    m_revolveProfileLabel = nullptr;
    m_axisCombo = nullptr;
    m_angleSpin = nullptr;
    m_fullRevCheck = nullptr;
    m_revolveOpCombo = nullptr;
}

void FeatureDialog::dismiss()
{
    m_mode = Mode::None;
    setVisible(false);
}

bool FeatureDialog::isActive() const
{
    return isVisible() && m_mode != Mode::None;
}

// =============================================================================
// Positioning
// =============================================================================

void FeatureDialog::repositionOverParent()
{
    if (!parentWidget())
        return;
    int parentW = parentWidget()->width();
    int x = parentW - width() - 20;
    int y = 20;
    move(x, y);
}

void FeatureDialog::showAtPosition()
{
    adjustSize();
    repositionOverParent();
    raise();
    setVisible(true);
}

bool FeatureDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        if (isVisible())
            repositionOverParent();
    }
    return QWidget::eventFilter(watched, event);
}

// =============================================================================
// Extrude
// =============================================================================

void FeatureDialog::showExtrude(const features::ExtrudeParams& defaults)
{
    clearForm();
    m_mode = Mode::Extrude;
    m_extrudeDefaults = defaults;
    m_titleLabel->setText(tr("EXTRUDE"));
    buildExtrudeForm(defaults);
    showAtPosition();
}

void FeatureDialog::buildExtrudeForm(const features::ExtrudeParams& defaults)
{
    // Profile (read-only)
    m_profileLabel = new QLabel(this);
    m_profileLabel->setText(defaults.profileId.empty()
        ? tr("Select profile...")
        : QString::fromStdString(defaults.profileId).left(20) + "...");
    m_profileLabel->setToolTip(QString::fromStdString(defaults.profileId));
    m_formLayout->addRow(tr("Profile"), m_profileLabel);

    // Direction
    m_directionCombo = new QComboBox(this);
    m_directionCombo->addItems({tr("One Side"), tr("Two Sides"), tr("Symmetric")});
    m_directionCombo->setCurrentIndex(static_cast<int>(defaults.direction));
    connect(m_directionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        emit parameterChanged("direction", idx);
    });
    m_formLayout->addRow(tr("Direction"), m_directionCombo);

    // Extent Type
    m_extentTypeCombo = new QComboBox(this);
    m_extentTypeCombo->addItems({tr("Distance"), tr("Through All"), tr("To Object"), tr("Symmetric")});
    m_extentTypeCombo->setCurrentIndex(static_cast<int>(defaults.extentType));
    connect(m_extentTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        emit parameterChanged("extentType", idx);
    });
    m_formLayout->addRow(tr("Extent Type"), m_extentTypeCombo);

    // Distance
    m_distanceSpin = new QDoubleSpinBox(this);
    m_distanceSpin->setRange(0.01, 10000.0);
    m_distanceSpin->setDecimals(2);
    m_distanceSpin->setSuffix(tr(" mm"));
    // Parse distance from expression
    double dist = 10.0;
    {
        QString expr = QString::fromStdString(defaults.distanceExpr);
        expr.remove(" mm").remove(" deg");
        bool ok = false;
        double v = expr.toDouble(&ok);
        if (ok) dist = v;
    }
    m_distanceSpin->setValue(dist);
    connect(m_distanceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double val) {
        emit parameterChanged("distance", val);
    });
    m_formLayout->addRow(tr("Distance"), m_distanceSpin);

    // Taper Angle
    m_taperAngleSpin = new QDoubleSpinBox(this);
    m_taperAngleSpin->setRange(-89.9, 89.9);
    m_taperAngleSpin->setDecimals(1);
    m_taperAngleSpin->setSuffix(QString::fromUtf8(" \xC2\xB0")); // degree sign
    m_taperAngleSpin->setValue(defaults.taperAngleDeg);
    connect(m_taperAngleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double val) {
        emit parameterChanged("taperAngle", val);
    });
    m_formLayout->addRow(tr("Taper Angle"), m_taperAngleSpin);

    // Operation
    m_operationCombo = new QComboBox(this);
    m_operationCombo->addItems({tr("New Body"), tr("Join"), tr("Cut"), tr("Intersect"), tr("New Component")});
    m_operationCombo->setCurrentIndex(static_cast<int>(defaults.operation));
    connect(m_operationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        emit parameterChanged("operation", idx);
    });
    m_formLayout->addRow(tr("Operation"), m_operationCombo);
}

// =============================================================================
// Fillet
// =============================================================================

void FeatureDialog::showFillet(const features::FilletParams& defaults)
{
    clearForm();
    m_mode = Mode::Fillet;
    m_filletDefaults = defaults;
    m_titleLabel->setText(tr("FILLET"));
    buildFilletForm(defaults);
    showAtPosition();
}

void FeatureDialog::buildFilletForm(const features::FilletParams& defaults)
{
    // Radius
    m_radiusSpin = new QDoubleSpinBox(this);
    m_radiusSpin->setRange(0.01, 1000.0);
    m_radiusSpin->setDecimals(2);
    m_radiusSpin->setSuffix(tr(" mm"));
    {
        double r = 3.0;
        QString expr = QString::fromStdString(defaults.radiusExpr);
        expr.remove(" mm");
        bool ok = false;
        double v = expr.toDouble(&ok);
        if (ok) r = v;
        m_radiusSpin->setValue(r);
    }
    connect(m_radiusSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double val) {
        emit parameterChanged("radius", val);
    });
    m_formLayout->addRow(tr("Radius"), m_radiusSpin);

    // Tangent Chain
    m_tangentChainCheck = new QCheckBox(this);
    m_tangentChainCheck->setChecked(defaults.isTangentChain);
    connect(m_tangentChainCheck, &QCheckBox::toggled,
            this, [this](bool checked) {
        emit parameterChanged("tangentChain", checked);
    });
    m_formLayout->addRow(tr("Tangent Chain"), m_tangentChainCheck);

    // Edge count (read-only)
    m_edgeCountLabel = new QLabel(this);
    int edgeCount = static_cast<int>(defaults.edgeIds.size());
    m_edgeCountLabel->setText(edgeCount > 0
        ? tr("%1 edge(s) selected").arg(edgeCount)
        : tr("All edges"));
    m_formLayout->addRow(tr("Edges"), m_edgeCountLabel);
}

// =============================================================================
// Chamfer
// =============================================================================

void FeatureDialog::showChamfer(const features::ChamferParams& defaults)
{
    clearForm();
    m_mode = Mode::Chamfer;
    m_chamferDefaults = defaults;
    m_titleLabel->setText(tr("CHAMFER"));
    buildChamferForm(defaults);
    showAtPosition();
}

void FeatureDialog::buildChamferForm(const features::ChamferParams& defaults)
{
    // Distance
    m_chamferDistSpin = new QDoubleSpinBox(this);
    m_chamferDistSpin->setRange(0.01, 1000.0);
    m_chamferDistSpin->setDecimals(2);
    m_chamferDistSpin->setSuffix(tr(" mm"));
    {
        double d = 2.0;
        QString expr = QString::fromStdString(defaults.distanceExpr);
        expr.remove(" mm");
        bool ok = false;
        double v = expr.toDouble(&ok);
        if (ok) d = v;
        m_chamferDistSpin->setValue(d);
    }
    connect(m_chamferDistSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double val) {
        emit parameterChanged("distance", val);
    });
    m_formLayout->addRow(tr("Distance"), m_chamferDistSpin);

    // Type
    m_chamferTypeCombo = new QComboBox(this);
    m_chamferTypeCombo->addItems({tr("Equal Distance"), tr("Two Distances"), tr("Distance and Angle")});
    m_chamferTypeCombo->setCurrentIndex(static_cast<int>(defaults.chamferType));
    connect(m_chamferTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        emit parameterChanged("chamferType", idx);
    });
    m_formLayout->addRow(tr("Type"), m_chamferTypeCombo);

    // Edge count (read-only)
    m_chamferEdgeLabel = new QLabel(this);
    int edgeCount = static_cast<int>(defaults.edgeIds.size());
    m_chamferEdgeLabel->setText(edgeCount > 0
        ? tr("%1 edge(s) selected").arg(edgeCount)
        : tr("All edges"));
    m_formLayout->addRow(tr("Edges"), m_chamferEdgeLabel);
}

// =============================================================================
// Shell
// =============================================================================

void FeatureDialog::showShell(const features::ShellParams& defaults)
{
    clearForm();
    m_mode = Mode::Shell;
    m_shellDefaults = defaults;
    m_titleLabel->setText(tr("SHELL"));
    buildShellForm(defaults);
    showAtPosition();
}

void FeatureDialog::buildShellForm(const features::ShellParams& defaults)
{
    // Thickness
    m_thicknessSpin = new QDoubleSpinBox(this);
    m_thicknessSpin->setRange(0.01, 1000.0);
    m_thicknessSpin->setDecimals(2);
    m_thicknessSpin->setSuffix(tr(" mm"));
    m_thicknessSpin->setValue(defaults.thicknessExpr);
    connect(m_thicknessSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double val) {
        emit parameterChanged("thickness", val);
    });
    m_formLayout->addRow(tr("Thickness"), m_thicknessSpin);

    // Direction
    m_shellDirCombo = new QComboBox(this);
    m_shellDirCombo->addItems({tr("Inside"), tr("Outside"), tr("Both")});
    m_shellDirCombo->setCurrentIndex(0);
    connect(m_shellDirCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        emit parameterChanged("shellDirection", idx);
    });
    m_formLayout->addRow(tr("Direction"), m_shellDirCombo);

    // Face count (read-only)
    m_faceCountLabel = new QLabel(this);
    int faceCount = static_cast<int>(defaults.removedFaceIds.size());
    m_faceCountLabel->setText(faceCount > 0
        ? tr("%1 face(s) to remove").arg(faceCount)
        : tr("Auto-detect"));
    m_formLayout->addRow(tr("Faces"), m_faceCountLabel);
}

// =============================================================================
// Revolve
// =============================================================================

void FeatureDialog::showRevolve(const features::RevolveParams& defaults)
{
    clearForm();
    m_mode = Mode::Revolve;
    m_revolveDefaults = defaults;
    m_titleLabel->setText(tr("REVOLVE"));
    buildRevolveForm(defaults);
    showAtPosition();
}

void FeatureDialog::buildRevolveForm(const features::RevolveParams& defaults)
{
    // Profile (read-only)
    m_revolveProfileLabel = new QLabel(this);
    m_revolveProfileLabel->setText(defaults.profileId.empty()
        ? tr("Select profile...")
        : QString::fromStdString(defaults.profileId).left(20) + "...");
    m_revolveProfileLabel->setToolTip(QString::fromStdString(defaults.profileId));
    m_formLayout->addRow(tr("Profile"), m_revolveProfileLabel);

    // Axis
    m_axisCombo = new QComboBox(this);
    m_axisCombo->addItems({tr("X Axis"), tr("Y Axis"), tr("Z Axis"), tr("Custom")});
    m_axisCombo->setCurrentIndex(static_cast<int>(defaults.axisType));
    connect(m_axisCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        emit parameterChanged("axis", idx);
    });
    m_formLayout->addRow(tr("Axis"), m_axisCombo);

    // Angle
    m_angleSpin = new QDoubleSpinBox(this);
    m_angleSpin->setRange(0.1, 360.0);
    m_angleSpin->setDecimals(1);
    m_angleSpin->setSuffix(QString::fromUtf8(" \xC2\xB0"));
    {
        double angle = 360.0;
        QString expr = QString::fromStdString(defaults.angleExpr);
        expr.remove(" deg");
        bool ok = false;
        double v = expr.toDouble(&ok);
        if (ok) angle = v;
        m_angleSpin->setValue(angle);
    }
    connect(m_angleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double val) {
        emit parameterChanged("angle", val);
    });
    m_formLayout->addRow(tr("Angle"), m_angleSpin);

    // Full Revolution
    m_fullRevCheck = new QCheckBox(this);
    m_fullRevCheck->setChecked(defaults.isFullRevolution);
    connect(m_fullRevCheck, &QCheckBox::toggled,
            this, [this](bool checked) {
        // When full revolution is checked, set angle to 360 and disable spin
        if (m_angleSpin) {
            m_angleSpin->setEnabled(!checked);
            if (checked)
                m_angleSpin->setValue(360.0);
        }
        emit parameterChanged("fullRevolution", checked);
    });
    m_formLayout->addRow(tr("Full Revolution"), m_fullRevCheck);
    // Apply initial state
    if (defaults.isFullRevolution && m_angleSpin)
        m_angleSpin->setEnabled(false);

    // Operation
    m_revolveOpCombo = new QComboBox(this);
    m_revolveOpCombo->addItems({tr("New Body"), tr("Join"), tr("Cut"), tr("Intersect"), tr("New Component")});
    m_revolveOpCombo->setCurrentIndex(static_cast<int>(defaults.operation));
    connect(m_revolveOpCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        emit parameterChanged("operation", idx);
    });
    m_formLayout->addRow(tr("Operation"), m_revolveOpCombo);
}

// =============================================================================
// Emit accepted -- build params from current form state
// =============================================================================

void FeatureDialog::emitAccepted()
{
    switch (m_mode) {
    case Mode::Extrude: {
        features::ExtrudeParams p = m_extrudeDefaults;
        if (m_distanceSpin)
            p.distanceExpr = QString::number(m_distanceSpin->value(), 'f', 2).toStdString() + " mm";
        if (m_directionCombo)
            p.direction = static_cast<features::ExtentDirection>(m_directionCombo->currentIndex());
        if (m_extentTypeCombo)
            p.extentType = static_cast<features::ExtentType>(m_extentTypeCombo->currentIndex());
        if (m_taperAngleSpin)
            p.taperAngleDeg = m_taperAngleSpin->value();
        if (m_operationCombo)
            p.operation = static_cast<features::FeatureOperation>(m_operationCombo->currentIndex());
        if (m_directionCombo && m_directionCombo->currentIndex() == 2)
            p.isSymmetric = true;
        emit extrudeAccepted(p);
        emit accepted();
        break;
    }
    case Mode::Fillet: {
        features::FilletParams p = m_filletDefaults;
        if (m_radiusSpin)
            p.radiusExpr = QString::number(m_radiusSpin->value(), 'f', 2).toStdString() + " mm";
        if (m_tangentChainCheck)
            p.isTangentChain = m_tangentChainCheck->isChecked();
        emit filletAccepted(p);
        emit accepted();
        break;
    }
    case Mode::Chamfer: {
        features::ChamferParams p = m_chamferDefaults;
        if (m_chamferDistSpin)
            p.distanceExpr = QString::number(m_chamferDistSpin->value(), 'f', 2).toStdString() + " mm";
        if (m_chamferTypeCombo)
            p.chamferType = static_cast<features::ChamferType>(m_chamferTypeCombo->currentIndex());
        emit chamferAccepted(p);
        emit accepted();
        break;
    }
    case Mode::Shell: {
        features::ShellParams p = m_shellDefaults;
        if (m_thicknessSpin)
            p.thicknessExpr = m_thicknessSpin->value();
        emit shellAccepted(p);
        emit accepted();
        break;
    }
    case Mode::Revolve: {
        features::RevolveParams p = m_revolveDefaults;
        if (m_axisCombo)
            p.axisType = static_cast<features::AxisType>(m_axisCombo->currentIndex());
        if (m_angleSpin)
            p.angleExpr = QString::number(m_angleSpin->value(), 'f', 1).toStdString() + " deg";
        if (m_fullRevCheck)
            p.isFullRevolution = m_fullRevCheck->isChecked();
        if (m_revolveOpCombo)
            p.operation = static_cast<features::FeatureOperation>(m_revolveOpCombo->currentIndex());
        emit revolveAccepted(p);
        emit accepted();
        break;
    }
    case Mode::None:
        break;
    }
}
