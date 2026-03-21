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
    m_distance2Spin = nullptr;
    m_distance2Label = nullptr;
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
    m_ppDistanceSpin = nullptr;
    m_ppFaceCountLabel = nullptr;
    m_cpTypeCombo = nullptr;
    m_cpRefCombo = nullptr;
    m_cpOffsetSpin = nullptr;
    m_cpAngleSpin = nullptr;
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

    // Distance 2 (for Two Sides mode)
    m_distance2Label = new QLabel(tr("Distance 2"), this);
    m_distance2Spin = new QDoubleSpinBox(this);
    m_distance2Spin->setRange(0.01, 10000.0);
    m_distance2Spin->setDecimals(2);
    m_distance2Spin->setSuffix(tr(" mm"));
    double dist2 = 10.0;
    if (!defaults.distance2Expr.empty()) {
        QString expr2 = QString::fromStdString(defaults.distance2Expr);
        expr2.remove(" mm");
        bool ok2 = false;
        double v2 = expr2.toDouble(&ok2);
        if (ok2) dist2 = v2;
    }
    m_distance2Spin->setValue(dist2);
    connect(m_distance2Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double val) {
        emit parameterChanged("distance2", val);
    });
    m_formLayout->addRow(m_distance2Label, m_distance2Spin);
    // Only visible when direction is "Two Sides" (index 1)
    bool showDist2 = (defaults.direction == features::ExtentDirection::Negative &&
                      !defaults.distance2Expr.empty()) ||
                     static_cast<int>(defaults.direction) == 1;
    m_distance2Spin->setVisible(showDist2);
    m_distance2Label->setVisible(showDist2);

    // Wire direction combo to show/hide Distance 2
    connect(m_directionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        bool twoSides = (idx == 1);
        if (m_distance2Spin) m_distance2Spin->setVisible(twoSides);
        if (m_distance2Label) m_distance2Label->setVisible(twoSides);
    });

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
        // Two Sides: capture distance2
        if (m_directionCombo && m_directionCombo->currentIndex() == 1 && m_distance2Spin)
            p.distance2Expr = QString::number(m_distance2Spin->value(), 'f', 2).toStdString() + " mm";
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
    case Mode::PressPull: {
        features::OffsetFacesParams p = m_pressPullDefaults;
        if (m_ppDistanceSpin)
            p.distance = m_ppDistanceSpin->value();
        emit pressPullAccepted(p);
        emit accepted();
        break;
    }
    case Mode::ConstructionPlane: {
        features::ConstructionPlaneParams p = m_cpDefaults;
        if (m_cpRefCombo) {
            QString ref = m_cpRefCombo->currentText();
            p.parentPlaneId = ref.toStdString();
            // Compute origin and normal based on reference plane + offset
            double offset = m_cpOffsetSpin ? m_cpOffsetSpin->value() : 0.0;
            p.offsetDistance = offset;
            p.definitionType = features::PlaneDefinitionType::OffsetFromPlane;
            if (ref == "XY") {
                p.originX = 0; p.originY = 0; p.originZ = offset;
                p.normalX = 0; p.normalY = 0; p.normalZ = 1;
                p.xDirX = 1; p.xDirY = 0; p.xDirZ = 0;
            } else if (ref == "XZ") {
                p.originX = 0; p.originY = offset; p.originZ = 0;
                p.normalX = 0; p.normalY = 1; p.normalZ = 0;
                p.xDirX = 1; p.xDirY = 0; p.xDirZ = 0;
            } else if (ref == "YZ") {
                p.originX = offset; p.originY = 0; p.originZ = 0;
                p.normalX = 1; p.normalY = 0; p.normalZ = 0;
                p.xDirX = 0; p.xDirY = 1; p.xDirZ = 0;
            }
        }
        emit constructionPlaneAccepted(p);
        emit accepted();
        break;
    }
    case Mode::None:
        break;
    }
}

// =============================================================================
// Press/Pull (Offset Faces)
// =============================================================================

void FeatureDialog::showPressPull(const features::OffsetFacesParams& defaults)
{
    clearForm();
    m_mode = Mode::PressPull;
    m_pressPullDefaults = defaults;
    m_titleLabel->setText(tr("PRESS/PULL"));
    buildPressPullForm(defaults);
    showAtPosition();
}

void FeatureDialog::buildPressPullForm(const features::OffsetFacesParams& defaults)
{
    // Distance
    m_ppDistanceSpin = new QDoubleSpinBox(this);
    m_ppDistanceSpin->setRange(-1000.0, 1000.0);
    m_ppDistanceSpin->setDecimals(2);
    m_ppDistanceSpin->setSuffix(tr(" mm"));
    m_ppDistanceSpin->setValue(defaults.distance);
    m_formLayout->addRow(tr("Distance:"), m_ppDistanceSpin);

    connect(m_ppDistanceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        emit parameterChanged("distance", v);
    });

    // Face count label
    m_ppFaceCountLabel = new QLabel(
        tr("%1 face(s) selected").arg(defaults.faceIndices.size()), this);
    m_formLayout->addRow(tr("Faces:"), m_ppFaceCountLabel);

    m_ppDistanceSpin->setFocus();
    m_ppDistanceSpin->selectAll();
}

// =============================================================================
// Construction Plane
// =============================================================================

void FeatureDialog::showConstructionPlane(const features::ConstructionPlaneParams& defaults)
{
    clearForm();
    m_mode = Mode::ConstructionPlane;
    m_cpDefaults = defaults;
    m_titleLabel->setText(tr("CONSTRUCTION PLANE"));
    buildConstructionPlaneForm(defaults);
    showAtPosition();
}

void FeatureDialog::buildConstructionPlaneForm(const features::ConstructionPlaneParams& defaults)
{
    // Reference plane
    m_cpRefCombo = new QComboBox(this);
    m_cpRefCombo->addItems({"XY", "XZ", "YZ"});
    if (!defaults.parentPlaneId.empty()) {
        int idx = m_cpRefCombo->findText(QString::fromStdString(defaults.parentPlaneId));
        if (idx >= 0) m_cpRefCombo->setCurrentIndex(idx);
    }
    m_formLayout->addRow(tr("Reference:"), m_cpRefCombo);

    // Offset distance
    m_cpOffsetSpin = new QDoubleSpinBox(this);
    m_cpOffsetSpin->setRange(-10000.0, 10000.0);
    m_cpOffsetSpin->setDecimals(2);
    m_cpOffsetSpin->setSuffix(tr(" mm"));
    m_cpOffsetSpin->setValue(defaults.offsetDistance);
    m_formLayout->addRow(tr("Offset:"), m_cpOffsetSpin);

    connect(m_cpOffsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        emit parameterChanged("offset", v);
    });

    m_cpOffsetSpin->setFocus();
    m_cpOffsetSpin->selectAll();
}
