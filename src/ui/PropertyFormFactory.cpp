#include "PropertyFormFactory.h"

#include "../features/ExtrudeFeature.h"
#include "../features/RevolveFeature.h"
#include "../features/FilletFeature.h"
#include "../features/ChamferFeature.h"
#include "../features/SketchFeature.h"
#include "../features/RectangularPatternFeature.h"
#include "../features/ShellFeature.h"
#include "../features/SweepFeature.h"
#include "../features/LoftFeature.h"
#include "../features/HoleFeature.h"
#include "../features/MirrorFeature.h"
#include "../features/CircularPatternFeature.h"
#include "../features/MoveFeature.h"
#include "../features/DraftFeature.h"
#include "../features/ThickenFeature.h"
#include "../features/ThreadFeature.h"
#include "../features/ScaleFeature.h"
#include "../features/CombineFeature.h"
#include "../features/SplitBodyFeature.h"
#include "../features/OffsetFacesFeature.h"
#include "../sketch/Sketch.h"

#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>

#include <cstdlib>

// ---------------------------------------------------------------------------
// parseExprValue — extract leading numeric value from "10 mm", "360 deg" etc.
// ---------------------------------------------------------------------------
double PropertyFormFactory::parseExprValue(const std::string& expr)
{
    if (expr.empty())
        return 0.0;
    char* end = nullptr;
    double v = std::strtod(expr.c_str(), &end);
    return (end != expr.c_str()) ? v : 0.0;
}

// ---------------------------------------------------------------------------
// buildExtrudeForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildExtrudeForm(QFormLayout* layout,
                                           const features::ExtrudeFeature* feat,
                                           ChangeCallback onChange)
{
    const auto& p = feat->params();

    // --- Distance (expression field) ---
    auto* distField = new QLineEdit();
    distField->setObjectName("Distance");
    distField->setText(QString::fromStdString(p.distanceExpr));
    distField->setPlaceholderText(QObject::tr("e.g. 50 mm or width/2"));
    distField->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #3c3f41; color: #e0e0e0; border: 1px solid #555; padding: 2px 4px; }"));
    layout->addRow(QObject::tr("Distance"), distField);

    QObject::connect(distField, &QLineEdit::editingFinished, [onChange, distField]() {
        onChange(QStringLiteral("distanceExpr"), distField->text());
    });

    // --- Extent Type ---
    auto* extentCombo = new QComboBox();
    extentCombo->setObjectName("ExtentType");
    extentCombo->addItems({QObject::tr("Distance"), QObject::tr("ThroughAll"), QObject::tr("ToEntity"), QObject::tr("Symmetric")});
    extentCombo->setCurrentIndex(static_cast<int>(p.extentType));
    layout->addRow(QObject::tr("Extent Type"), extentCombo);

    QObject::connect(extentCombo, &QComboBox::currentIndexChanged, [onChange, extentCombo](int idx) {
        onChange(QStringLiteral("ExtentType"), extentCombo->itemText(idx));
    });

    // --- Operation ---
    auto* opCombo = new QComboBox();
    opCombo->setObjectName("Operation");
    opCombo->addItems({QObject::tr("NewBody"), QObject::tr("Join"), QObject::tr("Cut"), QObject::tr("Intersect")});
    opCombo->setCurrentIndex(static_cast<int>(p.operation));
    layout->addRow(QObject::tr("Operation"), opCombo);

    QObject::connect(opCombo, &QComboBox::currentIndexChanged, [onChange, opCombo](int idx) {
        onChange(QStringLiteral("Operation"), opCombo->itemText(idx));
    });

    // --- Taper Angle ---
    auto* taperSpin = new QDoubleSpinBox();
    taperSpin->setObjectName("TaperAngle");
    taperSpin->setRange(-89.9, 89.9);
    taperSpin->setDecimals(2);
    taperSpin->setSuffix(QStringLiteral(" \u00B0")); // degree sign
    taperSpin->setValue(p.taperAngleDeg);
    layout->addRow(QObject::tr("Taper Angle"), taperSpin);

    QObject::connect(taperSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("TaperAngle"), v);
    });

    // --- Symmetric ---
    auto* symCheck = new QCheckBox();
    symCheck->setObjectName("Symmetric");
    symCheck->setChecked(p.isSymmetric);
    layout->addRow(QObject::tr("Symmetric"), symCheck);

    QObject::connect(symCheck, &QCheckBox::stateChanged, [onChange](int state) {
        onChange(QStringLiteral("Symmetric"), state == Qt::Checked);
    });
}

// ---------------------------------------------------------------------------
// buildRevolveForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildRevolveForm(QFormLayout* layout,
                                           const features::RevolveFeature* feat,
                                           ChangeCallback onChange)
{
    const auto& p = feat->params();

    // --- Angle (expression field) ---
    auto* angleField = new QLineEdit();
    angleField->setObjectName("Angle");
    angleField->setText(QString::fromStdString(p.angleExpr));
    angleField->setPlaceholderText(QObject::tr("e.g. 360 deg or sweep_angle"));
    angleField->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #3c3f41; color: #e0e0e0; border: 1px solid #555; padding: 2px 4px; }"));
    layout->addRow(QObject::tr("Angle"), angleField);

    QObject::connect(angleField, &QLineEdit::editingFinished, [onChange, angleField]() {
        onChange(QStringLiteral("angleExpr"), angleField->text());
    });

    // --- Axis Type ---
    auto* axisCombo = new QComboBox();
    axisCombo->setObjectName("AxisType");
    axisCombo->addItems({QObject::tr("XAxis"), QObject::tr("YAxis"), QObject::tr("ZAxis"), QObject::tr("Custom")});
    axisCombo->setCurrentIndex(static_cast<int>(p.axisType));
    layout->addRow(QObject::tr("Axis Type"), axisCombo);

    QObject::connect(axisCombo, &QComboBox::currentIndexChanged, [onChange, axisCombo](int idx) {
        onChange(QStringLiteral("AxisType"), axisCombo->itemText(idx));
    });

    // --- Full Revolution ---
    auto* fullCheck = new QCheckBox();
    fullCheck->setObjectName("FullRevolution");
    fullCheck->setChecked(p.isFullRevolution);
    layout->addRow(QObject::tr("Full Revolution"), fullCheck);

    QObject::connect(fullCheck, &QCheckBox::stateChanged, [onChange](int state) {
        onChange(QStringLiteral("FullRevolution"), state == Qt::Checked);
    });
}

// ---------------------------------------------------------------------------
// buildFilletForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildFilletForm(QFormLayout* layout,
                                          const features::FilletFeature* feat,
                                          ChangeCallback onChange)
{
    const auto& p = feat->params();

    // --- Radius (expression field) ---
    auto* radiusField = new QLineEdit();
    radiusField->setObjectName("Radius");
    radiusField->setText(QString::fromStdString(p.radiusExpr));
    radiusField->setPlaceholderText(QObject::tr("e.g. 2 mm or fillet_r"));
    radiusField->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #3c3f41; color: #e0e0e0; border: 1px solid #555; padding: 2px 4px; }"));
    layout->addRow(QObject::tr("Radius"), radiusField);

    QObject::connect(radiusField, &QLineEdit::editingFinished, [onChange, radiusField]() {
        onChange(QStringLiteral("radiusExpr"), radiusField->text());
    });

    // --- Edge count (read-only) ---
    auto* edgeLbl = new QLabel(QString::number(static_cast<int>(p.edgeIds.size())));
    layout->addRow(QObject::tr("Edges"), edgeLbl);
}

// ---------------------------------------------------------------------------
// buildChamferForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildChamferForm(QFormLayout* layout,
                                           const features::ChamferFeature* feat,
                                           ChangeCallback onChange)
{
    const auto& p = feat->params();

    // --- Distance (expression field) ---
    auto* distField = new QLineEdit();
    distField->setObjectName("Distance");
    distField->setText(QString::fromStdString(p.distanceExpr));
    distField->setPlaceholderText(QObject::tr("e.g. 1 mm or chamfer_d"));
    distField->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #3c3f41; color: #e0e0e0; border: 1px solid #555; padding: 2px 4px; }"));
    layout->addRow(QObject::tr("Distance"), distField);

    QObject::connect(distField, &QLineEdit::editingFinished, [onChange, distField]() {
        onChange(QStringLiteral("distanceExpr"), distField->text());
    });

    // --- Chamfer Type ---
    auto* typeCombo = new QComboBox();
    typeCombo->setObjectName("ChamferType");
    typeCombo->addItems({QObject::tr("EqualDistance"), QObject::tr("TwoDistances"), QObject::tr("DistanceAndAngle")});
    typeCombo->setCurrentIndex(static_cast<int>(p.chamferType));
    layout->addRow(QObject::tr("Chamfer Type"), typeCombo);

    QObject::connect(typeCombo, &QComboBox::currentIndexChanged, [onChange, typeCombo](int idx) {
        onChange(QStringLiteral("ChamferType"), typeCombo->itemText(idx));
    });

    // --- Edge count (read-only) ---
    auto* edgeLbl = new QLabel(QString::number(static_cast<int>(p.edgeIds.size())));
    layout->addRow(QObject::tr("Edges"), edgeLbl);
}

// ---------------------------------------------------------------------------
// buildSketchForm — show sketch summary info (read-only)
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildSketchForm(QFormLayout* layout,
                                          const features::SketchFeature* feat)
{
    const auto& sk = feat->sketch();
    layout->addRow(QObject::tr("Plane"),
        new QLabel(QString::fromStdString(feat->params().planeId)));
    layout->addRow(QObject::tr("Points"),
        new QLabel(QString::number(static_cast<int>(sk.points().size()))));
    layout->addRow(QObject::tr("Lines"),
        new QLabel(QString::number(static_cast<int>(sk.lines().size()))));
    layout->addRow(QObject::tr("Circles"),
        new QLabel(QString::number(static_cast<int>(sk.circles().size()))));
    layout->addRow(QObject::tr("Arcs"),
        new QLabel(QString::number(static_cast<int>(sk.arcs().size()))));
    layout->addRow(QObject::tr("Constraints"),
        new QLabel(QString::number(static_cast<int>(sk.constraints().size()))));
}

// ---------------------------------------------------------------------------
// buildRectangularPatternForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildRectangularPatternForm(QFormLayout* layout,
                                                      const features::RectangularPatternFeature* feat)
{
    const auto& p = feat->params();

    layout->addRow(QObject::tr("Dir1 Count"),
        new QLabel(QString::number(p.count1)));
    layout->addRow(QObject::tr("Dir1 Spacing"),
        new QLabel(QString::fromStdString(p.spacing1Expr)));
    layout->addRow(QObject::tr("Dir2 Count"),
        new QLabel(QString::number(p.count2)));
    layout->addRow(QObject::tr("Dir2 Spacing"),
        new QLabel(QString::fromStdString(p.spacing2Expr)));
}

// ---------------------------------------------------------------------------
// buildShellForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildShellForm(QFormLayout* layout,
                                         const features::ShellFeature* feat,
                                         ChangeCallback onChange)
{
    const auto& p = feat->params();

    // --- Thickness ---
    auto* thickSpin = new QDoubleSpinBox();
    thickSpin->setObjectName("Thickness");
    thickSpin->setRange(0.001, 100000.0);
    thickSpin->setDecimals(4);
    thickSpin->setSuffix(" mm");
    thickSpin->setValue(p.thicknessExpr);
    layout->addRow(QObject::tr("Thickness"), thickSpin);

    QObject::connect(thickSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("Thickness"), v);
    });

    // --- Removed faces (read-only) ---
    auto* faceLbl = new QLabel(QString::number(static_cast<int>(p.removedFaceIds.size())));
    layout->addRow(QObject::tr("Removed Faces"), faceLbl);
}

// ---------------------------------------------------------------------------
// buildSweepForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildSweepForm(QFormLayout* layout,
                                         const features::SweepFeature* feat,
                                         ChangeCallback onChange)
{
    const auto& p = feat->params();

    // --- Orientation ---
    auto* orientCheck = new QCheckBox(QObject::tr("Perpendicular"));
    orientCheck->setObjectName("Orientation");
    orientCheck->setChecked(p.isPerpendicularOrientation);
    layout->addRow(QObject::tr("Orientation"), orientCheck);

    QObject::connect(orientCheck, &QCheckBox::stateChanged, [onChange](int state) {
        onChange(QStringLiteral("Orientation"), state == Qt::Checked);
    });

    // --- Operation ---
    auto* opCombo = new QComboBox();
    opCombo->setObjectName("Operation");
    opCombo->addItems({QObject::tr("NewBody"), QObject::tr("Join"), QObject::tr("Cut"), QObject::tr("Intersect")});
    opCombo->setCurrentIndex(static_cast<int>(p.operation));
    layout->addRow(QObject::tr("Operation"), opCombo);

    QObject::connect(opCombo, &QComboBox::currentIndexChanged, [onChange, opCombo](int idx) {
        onChange(QStringLiteral("Operation"), opCombo->itemText(idx));
    });
}

// ---------------------------------------------------------------------------
// buildLoftForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildLoftForm(QFormLayout* layout,
                                        const features::LoftFeature* feat,
                                        ChangeCallback onChange)
{
    const auto& p = feat->params();

    // --- Closed ---
    auto* closedCheck = new QCheckBox();
    closedCheck->setObjectName("IsClosed");
    closedCheck->setChecked(p.isClosed);
    layout->addRow(QObject::tr("Closed"), closedCheck);

    QObject::connect(closedCheck, &QCheckBox::stateChanged, [onChange](int state) {
        onChange(QStringLiteral("IsClosed"), state == Qt::Checked);
    });

    // --- Operation ---
    auto* opCombo = new QComboBox();
    opCombo->setObjectName("Operation");
    opCombo->addItems({QObject::tr("NewBody"), QObject::tr("Join"), QObject::tr("Cut"), QObject::tr("Intersect")});
    opCombo->setCurrentIndex(static_cast<int>(p.operation));
    layout->addRow(QObject::tr("Operation"), opCombo);

    QObject::connect(opCombo, &QComboBox::currentIndexChanged, [onChange, opCombo](int idx) {
        onChange(QStringLiteral("Operation"), opCombo->itemText(idx));
    });

    // --- Section count (read-only) ---
    auto* secLbl = new QLabel(QString::number(static_cast<int>(p.sectionIds.size())));
    layout->addRow(QObject::tr("Sections"), secLbl);
}

// ---------------------------------------------------------------------------
// buildHoleForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildHoleForm(QFormLayout* layout,
                                        const features::HoleFeature* feat,
                                        ChangeCallback onChange)
{
    const auto& p = feat->params();

    // --- Hole Type ---
    auto* typeCombo = new QComboBox();
    typeCombo->setObjectName("HoleType");
    typeCombo->addItems({QObject::tr("Simple"), QObject::tr("Counterbore"), QObject::tr("Countersink")});
    typeCombo->setCurrentIndex(static_cast<int>(p.holeType));
    layout->addRow(QObject::tr("Hole Type"), typeCombo);

    // --- Diameter (expression field) ---
    auto* diamField = new QLineEdit();
    diamField->setObjectName("Diameter");
    diamField->setText(QString::fromStdString(p.diameterExpr));
    diamField->setPlaceholderText(QObject::tr("e.g. 10 mm or hole_d"));
    diamField->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #3c3f41; color: #e0e0e0; border: 1px solid #555; padding: 2px 4px; }"));
    layout->addRow(QObject::tr("Diameter"), diamField);

    QObject::connect(diamField, &QLineEdit::editingFinished, [onChange, diamField]() {
        onChange(QStringLiteral("diameterExpr"), diamField->text());
    });

    // --- Depth (expression field) ---
    auto* depthField = new QLineEdit();
    depthField->setObjectName("Depth");
    depthField->setText(QString::fromStdString(p.depthExpr));
    depthField->setPlaceholderText(QObject::tr("e.g. 20 mm or hole_depth (0=through)"));
    depthField->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #3c3f41; color: #e0e0e0; border: 1px solid #555; padding: 2px 4px; }"));
    layout->addRow(QObject::tr("Depth (0=through)"), depthField);

    QObject::connect(depthField, &QLineEdit::editingFinished, [onChange, depthField]() {
        onChange(QStringLiteral("depthExpr"), depthField->text());
    });

    // --- Tip Angle ---
    auto* tipSpin = new QDoubleSpinBox();
    tipSpin->setObjectName("TipAngle");
    tipSpin->setRange(0.0, 180.0);
    tipSpin->setDecimals(2);
    tipSpin->setSuffix(QStringLiteral(" \u00B0"));
    tipSpin->setValue(p.tipAngleDeg);
    layout->addRow(QObject::tr("Tip Angle"), tipSpin);

    QObject::connect(tipSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("TipAngle"), v);
    });

    // --- Counterbore fields ---
    auto* cboreDiamSpin = new QDoubleSpinBox();
    cboreDiamSpin->setObjectName("CboreDiameter");
    cboreDiamSpin->setRange(0.001, 100000.0);
    cboreDiamSpin->setDecimals(4);
    cboreDiamSpin->setSuffix(" mm");
    cboreDiamSpin->setValue(parseExprValue(p.cboreDiameterExpr));
    layout->addRow(QObject::tr("Cbore Diameter"), cboreDiamSpin);

    QObject::connect(cboreDiamSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("CboreDiameter"), v);
    });

    auto* cboreDepthSpin = new QDoubleSpinBox();
    cboreDepthSpin->setObjectName("CboreDepth");
    cboreDepthSpin->setRange(0.0, 100000.0);
    cboreDepthSpin->setDecimals(4);
    cboreDepthSpin->setSuffix(" mm");
    cboreDepthSpin->setValue(parseExprValue(p.cboreDepthExpr));
    layout->addRow(QObject::tr("Cbore Depth"), cboreDepthSpin);

    QObject::connect(cboreDepthSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("CboreDepth"), v);
    });

    // --- Countersink fields ---
    auto* csinkDiamSpin = new QDoubleSpinBox();
    csinkDiamSpin->setObjectName("CsinkDiameter");
    csinkDiamSpin->setRange(0.001, 100000.0);
    csinkDiamSpin->setDecimals(4);
    csinkDiamSpin->setSuffix(" mm");
    csinkDiamSpin->setValue(parseExprValue(p.csinkDiameterExpr));
    layout->addRow(QObject::tr("Csink Diameter"), csinkDiamSpin);

    QObject::connect(csinkDiamSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("CsinkDiameter"), v);
    });

    auto* csinkAngleSpin = new QDoubleSpinBox();
    csinkAngleSpin->setObjectName("CsinkAngle");
    csinkAngleSpin->setRange(0.0, 180.0);
    csinkAngleSpin->setDecimals(2);
    csinkAngleSpin->setSuffix(QStringLiteral(" \u00B0"));
    csinkAngleSpin->setValue(p.csinkAngleDeg);
    layout->addRow(QObject::tr("Csink Angle"), csinkAngleSpin);

    QObject::connect(csinkAngleSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("CsinkAngle"), v);
    });

    // Show/hide counterbore and countersink fields based on hole type
    auto updateVisibility = [=]() {
        int idx = typeCombo->currentIndex();
        bool isCbore = (idx == static_cast<int>(features::HoleType::Counterbore));
        bool isCsink = (idx == static_cast<int>(features::HoleType::Countersink));

        cboreDiamSpin->setVisible(isCbore);
        layout->labelForField(cboreDiamSpin)->setVisible(isCbore);
        cboreDepthSpin->setVisible(isCbore);
        layout->labelForField(cboreDepthSpin)->setVisible(isCbore);

        csinkDiamSpin->setVisible(isCsink);
        layout->labelForField(csinkDiamSpin)->setVisible(isCsink);
        csinkAngleSpin->setVisible(isCsink);
        layout->labelForField(csinkAngleSpin)->setVisible(isCsink);
    };

    updateVisibility();

    QObject::connect(typeCombo, &QComboBox::currentIndexChanged, [onChange, typeCombo, updateVisibility](int idx) {
        onChange(QStringLiteral("HoleType"), typeCombo->itemText(idx));
        updateVisibility();
    });
}

// ---------------------------------------------------------------------------
// buildMirrorForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildMirrorForm(QFormLayout* layout,
                                          const features::MirrorFeature* feat,
                                          ChangeCallback onChange)
{
    const auto& p = feat->params();

    // --- Plane Origin X ---
    auto* oxSpin = new QDoubleSpinBox();
    oxSpin->setObjectName("PlaneOx");
    oxSpin->setRange(-1e9, 1e9);
    oxSpin->setDecimals(4);
    oxSpin->setSuffix(" mm");
    oxSpin->setValue(p.planeOx);
    layout->addRow(QObject::tr("Plane Origin X"), oxSpin);

    QObject::connect(oxSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("PlaneOx"), v);
    });

    // --- Plane Origin Y ---
    auto* oySpin = new QDoubleSpinBox();
    oySpin->setObjectName("PlaneOy");
    oySpin->setRange(-1e9, 1e9);
    oySpin->setDecimals(4);
    oySpin->setSuffix(" mm");
    oySpin->setValue(p.planeOy);
    layout->addRow(QObject::tr("Plane Origin Y"), oySpin);

    QObject::connect(oySpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("PlaneOy"), v);
    });

    // --- Plane Origin Z ---
    auto* ozSpin = new QDoubleSpinBox();
    ozSpin->setObjectName("PlaneOz");
    ozSpin->setRange(-1e9, 1e9);
    ozSpin->setDecimals(4);
    ozSpin->setSuffix(" mm");
    ozSpin->setValue(p.planeOz);
    layout->addRow(QObject::tr("Plane Origin Z"), ozSpin);

    QObject::connect(ozSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("PlaneOz"), v);
    });

    // --- Plane Normal X ---
    auto* nxSpin = new QDoubleSpinBox();
    nxSpin->setObjectName("PlaneNx");
    nxSpin->setRange(-1.0, 1.0);
    nxSpin->setDecimals(6);
    nxSpin->setSingleStep(0.1);
    nxSpin->setValue(p.planeNx);
    layout->addRow(QObject::tr("Plane Normal X"), nxSpin);

    QObject::connect(nxSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("PlaneNx"), v);
    });

    // --- Plane Normal Y ---
    auto* nySpin = new QDoubleSpinBox();
    nySpin->setObjectName("PlaneNy");
    nySpin->setRange(-1.0, 1.0);
    nySpin->setDecimals(6);
    nySpin->setSingleStep(0.1);
    nySpin->setValue(p.planeNy);
    layout->addRow(QObject::tr("Plane Normal Y"), nySpin);

    QObject::connect(nySpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("PlaneNy"), v);
    });

    // --- Plane Normal Z ---
    auto* nzSpin = new QDoubleSpinBox();
    nzSpin->setObjectName("PlaneNz");
    nzSpin->setRange(-1.0, 1.0);
    nzSpin->setDecimals(6);
    nzSpin->setSingleStep(0.1);
    nzSpin->setValue(p.planeNz);
    layout->addRow(QObject::tr("Plane Normal Z"), nzSpin);

    QObject::connect(nzSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("PlaneNz"), v);
    });

    // --- Combine ---
    auto* combineCheck = new QCheckBox();
    combineCheck->setObjectName("IsCombine");
    combineCheck->setChecked(p.isCombine);
    layout->addRow(QObject::tr("Combine"), combineCheck);

    QObject::connect(combineCheck, &QCheckBox::stateChanged, [onChange](int state) {
        onChange(QStringLiteral("IsCombine"), state == Qt::Checked);
    });
}

// ---------------------------------------------------------------------------
// buildCircularPatternForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildCircularPatternForm(QFormLayout* layout,
                                                    const features::CircularPatternFeature* feat,
                                                    ChangeCallback onChange)
{
    const auto& p = feat->params();

    // --- Axis Origin X ---
    auto* axOxSpin = new QDoubleSpinBox();
    axOxSpin->setObjectName("AxisOx");
    axOxSpin->setRange(-1e9, 1e9);
    axOxSpin->setDecimals(4);
    axOxSpin->setSuffix(" mm");
    axOxSpin->setValue(p.axisOx);
    layout->addRow(QObject::tr("Axis Origin X"), axOxSpin);

    QObject::connect(axOxSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("AxisOx"), v);
    });

    // --- Axis Origin Y ---
    auto* axOySpin = new QDoubleSpinBox();
    axOySpin->setObjectName("AxisOy");
    axOySpin->setRange(-1e9, 1e9);
    axOySpin->setDecimals(4);
    axOySpin->setSuffix(" mm");
    axOySpin->setValue(p.axisOy);
    layout->addRow(QObject::tr("Axis Origin Y"), axOySpin);

    QObject::connect(axOySpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("AxisOy"), v);
    });

    // --- Axis Origin Z ---
    auto* axOzSpin = new QDoubleSpinBox();
    axOzSpin->setObjectName("AxisOz");
    axOzSpin->setRange(-1e9, 1e9);
    axOzSpin->setDecimals(4);
    axOzSpin->setSuffix(" mm");
    axOzSpin->setValue(p.axisOz);
    layout->addRow(QObject::tr("Axis Origin Z"), axOzSpin);

    QObject::connect(axOzSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("AxisOz"), v);
    });

    // --- Axis Direction X ---
    auto* axDxSpin = new QDoubleSpinBox();
    axDxSpin->setObjectName("AxisDx");
    axDxSpin->setRange(-1.0, 1.0);
    axDxSpin->setDecimals(6);
    axDxSpin->setSingleStep(0.1);
    axDxSpin->setValue(p.axisDx);
    layout->addRow(QObject::tr("Axis Dir X"), axDxSpin);

    QObject::connect(axDxSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("AxisDx"), v);
    });

    // --- Axis Direction Y ---
    auto* axDySpin = new QDoubleSpinBox();
    axDySpin->setObjectName("AxisDy");
    axDySpin->setRange(-1.0, 1.0);
    axDySpin->setDecimals(6);
    axDySpin->setSingleStep(0.1);
    axDySpin->setValue(p.axisDy);
    layout->addRow(QObject::tr("Axis Dir Y"), axDySpin);

    QObject::connect(axDySpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("AxisDy"), v);
    });

    // --- Axis Direction Z ---
    auto* axDzSpin = new QDoubleSpinBox();
    axDzSpin->setObjectName("AxisDz");
    axDzSpin->setRange(-1.0, 1.0);
    axDzSpin->setDecimals(6);
    axDzSpin->setSingleStep(0.1);
    axDzSpin->setValue(p.axisDz);
    layout->addRow(QObject::tr("Axis Dir Z"), axDzSpin);

    QObject::connect(axDzSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("AxisDz"), v);
    });

    // --- Count ---
    auto* countSpin = new QSpinBox();
    countSpin->setObjectName("Count");
    countSpin->setRange(1, 10000);
    countSpin->setValue(p.count);
    layout->addRow(QObject::tr("Count"), countSpin);

    QObject::connect(countSpin, &QSpinBox::valueChanged, [onChange](int v) {
        onChange(QStringLiteral("Count"), v);
    });

    // --- Total Angle ---
    auto* angleSpin = new QDoubleSpinBox();
    angleSpin->setObjectName("TotalAngle");
    angleSpin->setRange(0.0, 360.0);
    angleSpin->setDecimals(2);
    angleSpin->setSuffix(QStringLiteral(" \u00B0"));
    angleSpin->setValue(p.totalAngleDeg);
    layout->addRow(QObject::tr("Total Angle"), angleSpin);

    QObject::connect(angleSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("TotalAngle"), v);
    });
}

// ---------------------------------------------------------------------------
// buildMoveForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildMoveForm(QFormLayout* layout,
                                        const features::MoveFeature* feat,
                                        ChangeCallback onChange)
{
    const auto& p = feat->params();

    // --- Mode ---
    auto* modeCombo = new QComboBox();
    modeCombo->setObjectName("Mode");
    modeCombo->addItems({QObject::tr("FreeTransform"), QObject::tr("Translate"), QObject::tr("Rotate")});
    // MoveMode enum: FreeTransform=0, TranslateXYZ=1, Rotate=2
    modeCombo->setCurrentIndex(static_cast<int>(p.mode));
    layout->addRow(QObject::tr("Mode"), modeCombo);

    // --- Translate fields ---
    auto* dxSpin = new QDoubleSpinBox();
    dxSpin->setObjectName("DX");
    dxSpin->setRange(-1e9, 1e9);
    dxSpin->setDecimals(4);
    dxSpin->setSuffix(" mm");
    dxSpin->setValue(p.dx);
    layout->addRow(QObject::tr("DX"), dxSpin);

    QObject::connect(dxSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("DX"), v);
    });

    auto* dySpin = new QDoubleSpinBox();
    dySpin->setObjectName("DY");
    dySpin->setRange(-1e9, 1e9);
    dySpin->setDecimals(4);
    dySpin->setSuffix(" mm");
    dySpin->setValue(p.dy);
    layout->addRow(QObject::tr("DY"), dySpin);

    QObject::connect(dySpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("DY"), v);
    });

    auto* dzSpin = new QDoubleSpinBox();
    dzSpin->setObjectName("DZ");
    dzSpin->setRange(-1e9, 1e9);
    dzSpin->setDecimals(4);
    dzSpin->setSuffix(" mm");
    dzSpin->setValue(p.dz);
    layout->addRow(QObject::tr("DZ"), dzSpin);

    QObject::connect(dzSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("DZ"), v);
    });

    // --- Rotate fields ---
    auto* rAxOxSpin = new QDoubleSpinBox();
    rAxOxSpin->setObjectName("RotAxisOx");
    rAxOxSpin->setRange(-1e9, 1e9);
    rAxOxSpin->setDecimals(4);
    rAxOxSpin->setSuffix(" mm");
    rAxOxSpin->setValue(p.axisOx);
    layout->addRow(QObject::tr("Rot Axis Origin X"), rAxOxSpin);

    QObject::connect(rAxOxSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("RotAxisOx"), v);
    });

    auto* rAxOySpin = new QDoubleSpinBox();
    rAxOySpin->setObjectName("RotAxisOy");
    rAxOySpin->setRange(-1e9, 1e9);
    rAxOySpin->setDecimals(4);
    rAxOySpin->setSuffix(" mm");
    rAxOySpin->setValue(p.axisOy);
    layout->addRow(QObject::tr("Rot Axis Origin Y"), rAxOySpin);

    QObject::connect(rAxOySpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("RotAxisOy"), v);
    });

    auto* rAxOzSpin = new QDoubleSpinBox();
    rAxOzSpin->setObjectName("RotAxisOz");
    rAxOzSpin->setRange(-1e9, 1e9);
    rAxOzSpin->setDecimals(4);
    rAxOzSpin->setSuffix(" mm");
    rAxOzSpin->setValue(p.axisOz);
    layout->addRow(QObject::tr("Rot Axis Origin Z"), rAxOzSpin);

    QObject::connect(rAxOzSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("RotAxisOz"), v);
    });

    auto* rAxDxSpin = new QDoubleSpinBox();
    rAxDxSpin->setObjectName("RotAxisDx");
    rAxDxSpin->setRange(-1.0, 1.0);
    rAxDxSpin->setDecimals(6);
    rAxDxSpin->setSingleStep(0.1);
    rAxDxSpin->setValue(p.axisDx);
    layout->addRow(QObject::tr("Rot Axis Dir X"), rAxDxSpin);

    QObject::connect(rAxDxSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("RotAxisDx"), v);
    });

    auto* rAxDySpin = new QDoubleSpinBox();
    rAxDySpin->setObjectName("RotAxisDy");
    rAxDySpin->setRange(-1.0, 1.0);
    rAxDySpin->setDecimals(6);
    rAxDySpin->setSingleStep(0.1);
    rAxDySpin->setValue(p.axisDy);
    layout->addRow(QObject::tr("Rot Axis Dir Y"), rAxDySpin);

    QObject::connect(rAxDySpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("RotAxisDy"), v);
    });

    auto* rAxDzSpin = new QDoubleSpinBox();
    rAxDzSpin->setObjectName("RotAxisDz");
    rAxDzSpin->setRange(-1.0, 1.0);
    rAxDzSpin->setDecimals(6);
    rAxDzSpin->setSingleStep(0.1);
    rAxDzSpin->setValue(p.axisDz);
    layout->addRow(QObject::tr("Rot Axis Dir Z"), rAxDzSpin);

    QObject::connect(rAxDzSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("RotAxisDz"), v);
    });

    auto* rotAngleSpin = new QDoubleSpinBox();
    rotAngleSpin->setObjectName("RotAngle");
    rotAngleSpin->setRange(-360.0, 360.0);
    rotAngleSpin->setDecimals(2);
    rotAngleSpin->setSuffix(QStringLiteral(" \u00B0"));
    rotAngleSpin->setValue(p.angleDeg);
    layout->addRow(QObject::tr("Rotation Angle"), rotAngleSpin);

    QObject::connect(rotAngleSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("RotAngle"), v);
    });

    // Show/hide translate vs rotate fields based on mode
    auto updateMoveVisibility = [=]() {
        int idx = modeCombo->currentIndex();
        bool isTranslate = (idx == static_cast<int>(features::MoveMode::TranslateXYZ));
        bool isRotate    = (idx == static_cast<int>(features::MoveMode::Rotate));

        dxSpin->setVisible(isTranslate);
        layout->labelForField(dxSpin)->setVisible(isTranslate);
        dySpin->setVisible(isTranslate);
        layout->labelForField(dySpin)->setVisible(isTranslate);
        dzSpin->setVisible(isTranslate);
        layout->labelForField(dzSpin)->setVisible(isTranslate);

        rAxOxSpin->setVisible(isRotate);
        layout->labelForField(rAxOxSpin)->setVisible(isRotate);
        rAxOySpin->setVisible(isRotate);
        layout->labelForField(rAxOySpin)->setVisible(isRotate);
        rAxOzSpin->setVisible(isRotate);
        layout->labelForField(rAxOzSpin)->setVisible(isRotate);
        rAxDxSpin->setVisible(isRotate);
        layout->labelForField(rAxDxSpin)->setVisible(isRotate);
        rAxDySpin->setVisible(isRotate);
        layout->labelForField(rAxDySpin)->setVisible(isRotate);
        rAxDzSpin->setVisible(isRotate);
        layout->labelForField(rAxDzSpin)->setVisible(isRotate);
        rotAngleSpin->setVisible(isRotate);
        layout->labelForField(rotAngleSpin)->setVisible(isRotate);
    };

    updateMoveVisibility();

    QObject::connect(modeCombo, &QComboBox::currentIndexChanged, [onChange, modeCombo, updateMoveVisibility](int idx) {
        onChange(QStringLiteral("Mode"), modeCombo->itemText(idx));
        updateMoveVisibility();
    });
}

// ---------------------------------------------------------------------------
// buildDraftForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildDraftForm(QFormLayout* layout,
                                         const features::DraftFeature* feat,
                                         ChangeCallback onChange)
{
    const auto& p = feat->params();

    // --- Angle ---
    auto* angleSpin = new QDoubleSpinBox();
    angleSpin->setObjectName("Angle");
    angleSpin->setRange(0.0, 89.9);
    angleSpin->setDecimals(2);
    angleSpin->setSuffix(QStringLiteral(" \u00B0"));
    angleSpin->setValue(parseExprValue(p.angleExpr));
    layout->addRow(QObject::tr("Angle"), angleSpin);

    QObject::connect(angleSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("Angle"), v);
    });

    // --- Pull Direction X ---
    auto* pdxSpin = new QDoubleSpinBox();
    pdxSpin->setObjectName("PullDirX");
    pdxSpin->setRange(-1.0, 1.0);
    pdxSpin->setDecimals(6);
    pdxSpin->setSingleStep(0.1);
    pdxSpin->setValue(p.pullDirX);
    layout->addRow(QObject::tr("Pull Dir X"), pdxSpin);

    QObject::connect(pdxSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("PullDirX"), v);
    });

    // --- Pull Direction Y ---
    auto* pdySpin = new QDoubleSpinBox();
    pdySpin->setObjectName("PullDirY");
    pdySpin->setRange(-1.0, 1.0);
    pdySpin->setDecimals(6);
    pdySpin->setSingleStep(0.1);
    pdySpin->setValue(p.pullDirY);
    layout->addRow(QObject::tr("Pull Dir Y"), pdySpin);

    QObject::connect(pdySpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("PullDirY"), v);
    });

    // --- Pull Direction Z ---
    auto* pdzSpin = new QDoubleSpinBox();
    pdzSpin->setObjectName("PullDirZ");
    pdzSpin->setRange(-1.0, 1.0);
    pdzSpin->setDecimals(6);
    pdzSpin->setSingleStep(0.1);
    pdzSpin->setValue(p.pullDirZ);
    layout->addRow(QObject::tr("Pull Dir Z"), pdzSpin);

    QObject::connect(pdzSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("PullDirZ"), v);
    });

    // --- Face count (read-only) ---
    auto* faceLbl = new QLabel(QString::number(static_cast<int>(p.faceIndices.size())));
    layout->addRow(QObject::tr("Faces"), faceLbl);
}

// ---------------------------------------------------------------------------
// buildThickenForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildThickenForm(QFormLayout* layout,
                                           const features::ThickenFeature* feat,
                                           ChangeCallback onChange)
{
    const auto& p = feat->params();

    // --- Thickness ---
    auto* thickSpin = new QDoubleSpinBox();
    thickSpin->setObjectName("Thickness");
    thickSpin->setRange(-100000.0, 100000.0);
    thickSpin->setDecimals(4);
    thickSpin->setSuffix(" mm");
    thickSpin->setValue(parseExprValue(p.thicknessExpr));
    layout->addRow(QObject::tr("Thickness"), thickSpin);

    QObject::connect(thickSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("Thickness"), v);
    });

    // --- Symmetric ---
    auto* symCheck = new QCheckBox();
    symCheck->setObjectName("IsSymmetric");
    symCheck->setChecked(p.isSymmetric);
    layout->addRow(QObject::tr("Symmetric"), symCheck);

    QObject::connect(symCheck, &QCheckBox::stateChanged, [onChange](int state) {
        onChange(QStringLiteral("IsSymmetric"), state == Qt::Checked);
    });
}

// ---------------------------------------------------------------------------
// buildThreadForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildThreadForm(QFormLayout* layout,
                                          const features::ThreadFeature* feat,
                                          ChangeCallback onChange)
{
    const auto& p = feat->params();

    // --- Thread Type ---
    auto* typeCombo = new QComboBox();
    typeCombo->setObjectName("ThreadType");
    typeCombo->addItems({QObject::tr("Metric Coarse"), QObject::tr("Metric Fine"), QObject::tr("UNC"), QObject::tr("UNF")});
    typeCombo->setCurrentIndex(static_cast<int>(p.threadType));
    layout->addRow(QObject::tr("Thread Type"), typeCombo);

    QObject::connect(typeCombo, &QComboBox::currentIndexChanged, [onChange, typeCombo](int idx) {
        onChange(QStringLiteral("ThreadType"), typeCombo->itemText(idx));
    });

    // --- Pitch ---
    auto* pitchSpin = new QDoubleSpinBox();
    pitchSpin->setObjectName("Pitch");
    pitchSpin->setRange(0.001, 1000.0);
    pitchSpin->setDecimals(4);
    pitchSpin->setSuffix(" mm");
    pitchSpin->setValue(p.pitch);
    layout->addRow(QObject::tr("Pitch"), pitchSpin);

    QObject::connect(pitchSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("Pitch"), v);
    });

    // --- Depth ---
    auto* depthSpin = new QDoubleSpinBox();
    depthSpin->setObjectName("Depth");
    depthSpin->setRange(0.001, 1000.0);
    depthSpin->setDecimals(4);
    depthSpin->setSuffix(" mm");
    depthSpin->setValue(p.depth);
    layout->addRow(QObject::tr("Depth"), depthSpin);

    QObject::connect(depthSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("Depth"), v);
    });

    // --- Internal ---
    auto* internalCheck = new QCheckBox();
    internalCheck->setObjectName("IsInternal");
    internalCheck->setChecked(p.isInternal);
    layout->addRow(QObject::tr("Internal"), internalCheck);

    QObject::connect(internalCheck, &QCheckBox::stateChanged, [onChange](int state) {
        onChange(QStringLiteral("IsInternal"), state == Qt::Checked);
    });

    // --- Right-Handed ---
    auto* rhCheck = new QCheckBox();
    rhCheck->setObjectName("IsRightHanded");
    rhCheck->setChecked(p.isRightHanded);
    layout->addRow(QObject::tr("Right-Handed"), rhCheck);

    QObject::connect(rhCheck, &QCheckBox::stateChanged, [onChange](int state) {
        onChange(QStringLiteral("IsRightHanded"), state == Qt::Checked);
    });

    // --- Modeled ---
    auto* modeledCheck = new QCheckBox();
    modeledCheck->setObjectName("IsModeled");
    modeledCheck->setChecked(p.isModeled);
    layout->addRow(QObject::tr("Modeled"), modeledCheck);

    QObject::connect(modeledCheck, &QCheckBox::stateChanged, [onChange](int state) {
        onChange(QStringLiteral("IsModeled"), state == Qt::Checked);
    });
}

// ---------------------------------------------------------------------------
// buildScaleForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildScaleForm(QFormLayout* layout,
                                         const features::ScaleFeature* feat,
                                         ChangeCallback onChange)
{
    const auto& p = feat->params();

    // --- Scale Type ---
    auto* typeCombo = new QComboBox();
    typeCombo->setObjectName("ScaleType");
    typeCombo->addItems({QObject::tr("Uniform"), QObject::tr("Non-Uniform")});
    typeCombo->setCurrentIndex(static_cast<int>(p.scaleType));
    layout->addRow(QObject::tr("Scale Type"), typeCombo);

    // --- Uniform factor ---
    auto* factorSpin = new QDoubleSpinBox();
    factorSpin->setObjectName("Factor");
    factorSpin->setRange(0.001, 100000.0);
    factorSpin->setDecimals(6);
    factorSpin->setValue(p.factor);
    layout->addRow(QObject::tr("Factor"), factorSpin);

    QObject::connect(factorSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("Factor"), v);
    });

    // --- Non-uniform factors ---
    auto* fxSpin = new QDoubleSpinBox();
    fxSpin->setObjectName("FactorX");
    fxSpin->setRange(0.001, 100000.0);
    fxSpin->setDecimals(6);
    fxSpin->setValue(p.factorX);
    layout->addRow(QObject::tr("Factor X"), fxSpin);

    QObject::connect(fxSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("FactorX"), v);
    });

    auto* fySpin = new QDoubleSpinBox();
    fySpin->setObjectName("FactorY");
    fySpin->setRange(0.001, 100000.0);
    fySpin->setDecimals(6);
    fySpin->setValue(p.factorY);
    layout->addRow(QObject::tr("Factor Y"), fySpin);

    QObject::connect(fySpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("FactorY"), v);
    });

    auto* fzSpin = new QDoubleSpinBox();
    fzSpin->setObjectName("FactorZ");
    fzSpin->setRange(0.001, 100000.0);
    fzSpin->setDecimals(6);
    fzSpin->setValue(p.factorZ);
    layout->addRow(QObject::tr("Factor Z"), fzSpin);

    QObject::connect(fzSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("FactorZ"), v);
    });

    // Show/hide uniform vs non-uniform fields based on scale type
    auto updateScaleVisibility = [=]() {
        int idx = typeCombo->currentIndex();
        bool isUniform = (idx == static_cast<int>(features::ScaleType::Uniform));

        factorSpin->setVisible(isUniform);
        layout->labelForField(factorSpin)->setVisible(isUniform);

        fxSpin->setVisible(!isUniform);
        layout->labelForField(fxSpin)->setVisible(!isUniform);
        fySpin->setVisible(!isUniform);
        layout->labelForField(fySpin)->setVisible(!isUniform);
        fzSpin->setVisible(!isUniform);
        layout->labelForField(fzSpin)->setVisible(!isUniform);
    };

    updateScaleVisibility();

    QObject::connect(typeCombo, &QComboBox::currentIndexChanged, [onChange, typeCombo, updateScaleVisibility](int idx) {
        onChange(QStringLiteral("ScaleType"), typeCombo->itemText(idx));
        updateScaleVisibility();
    });
}

// ---------------------------------------------------------------------------
// buildCombineForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildCombineForm(QFormLayout* layout,
                                           const features::CombineFeature* feat,
                                           ChangeCallback onChange)
{
    const auto& p = feat->params();

    // --- Operation ---
    auto* opCombo = new QComboBox();
    opCombo->setObjectName("Operation");
    opCombo->addItems({QObject::tr("Join"), QObject::tr("Cut"), QObject::tr("Intersect")});
    opCombo->setCurrentIndex(static_cast<int>(p.operation));
    layout->addRow(QObject::tr("Operation"), opCombo);

    QObject::connect(opCombo, &QComboBox::currentIndexChanged, [onChange, opCombo](int idx) {
        onChange(QStringLiteral("Operation"), opCombo->itemText(idx));
    });

    // --- Keep Tool Body ---
    auto* keepCheck = new QCheckBox();
    keepCheck->setObjectName("KeepToolBody");
    keepCheck->setChecked(p.keepToolBody);
    layout->addRow(QObject::tr("Keep Tool Body"), keepCheck);

    QObject::connect(keepCheck, &QCheckBox::stateChanged, [onChange](int state) {
        onChange(QStringLiteral("KeepToolBody"), state == Qt::Checked);
    });
}

// ---------------------------------------------------------------------------
// buildSplitBodyForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildSplitBodyForm(QFormLayout* layout,
                                              const features::SplitBodyFeature* feat,
                                              ChangeCallback onChange)
{
    const auto& p = feat->params();

    // --- Plane Origin X ---
    auto* oxSpin = new QDoubleSpinBox();
    oxSpin->setObjectName("PlaneOx");
    oxSpin->setRange(-1e9, 1e9);
    oxSpin->setDecimals(4);
    oxSpin->setSuffix(" mm");
    oxSpin->setValue(p.planeOx);
    layout->addRow(QObject::tr("Plane Origin X"), oxSpin);

    QObject::connect(oxSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("PlaneOx"), v);
    });

    // --- Plane Origin Y ---
    auto* oySpin = new QDoubleSpinBox();
    oySpin->setObjectName("PlaneOy");
    oySpin->setRange(-1e9, 1e9);
    oySpin->setDecimals(4);
    oySpin->setSuffix(" mm");
    oySpin->setValue(p.planeOy);
    layout->addRow(QObject::tr("Plane Origin Y"), oySpin);

    QObject::connect(oySpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("PlaneOy"), v);
    });

    // --- Plane Origin Z ---
    auto* ozSpin = new QDoubleSpinBox();
    ozSpin->setObjectName("PlaneOz");
    ozSpin->setRange(-1e9, 1e9);
    ozSpin->setDecimals(4);
    ozSpin->setSuffix(" mm");
    ozSpin->setValue(p.planeOz);
    layout->addRow(QObject::tr("Plane Origin Z"), ozSpin);

    QObject::connect(ozSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("PlaneOz"), v);
    });

    // --- Plane Normal X ---
    auto* nxSpin = new QDoubleSpinBox();
    nxSpin->setObjectName("PlaneNx");
    nxSpin->setRange(-1.0, 1.0);
    nxSpin->setDecimals(6);
    nxSpin->setSingleStep(0.1);
    nxSpin->setValue(p.planeNx);
    layout->addRow(QObject::tr("Plane Normal X"), nxSpin);

    QObject::connect(nxSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("PlaneNx"), v);
    });

    // --- Plane Normal Y ---
    auto* nySpin = new QDoubleSpinBox();
    nySpin->setObjectName("PlaneNy");
    nySpin->setRange(-1.0, 1.0);
    nySpin->setDecimals(6);
    nySpin->setSingleStep(0.1);
    nySpin->setValue(p.planeNy);
    layout->addRow(QObject::tr("Plane Normal Y"), nySpin);

    QObject::connect(nySpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("PlaneNy"), v);
    });

    // --- Plane Normal Z ---
    auto* nzSpin = new QDoubleSpinBox();
    nzSpin->setObjectName("PlaneNz");
    nzSpin->setRange(-1.0, 1.0);
    nzSpin->setDecimals(6);
    nzSpin->setSingleStep(0.1);
    nzSpin->setValue(p.planeNz);
    layout->addRow(QObject::tr("Plane Normal Z"), nzSpin);

    QObject::connect(nzSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("PlaneNz"), v);
    });
}

// ---------------------------------------------------------------------------
// buildOffsetFacesForm
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildOffsetFacesForm(QFormLayout* layout,
                                                const features::OffsetFacesFeature* feat,
                                                ChangeCallback onChange)
{
    const auto& p = feat->params();

    // --- Distance ---
    auto* distSpin = new QDoubleSpinBox();
    distSpin->setObjectName("Distance");
    distSpin->setRange(-100000.0, 100000.0);
    distSpin->setDecimals(4);
    distSpin->setSuffix(" mm");
    distSpin->setValue(p.distance);
    layout->addRow(QObject::tr("Distance"), distSpin);

    QObject::connect(distSpin, &QDoubleSpinBox::valueChanged, [onChange](double v) {
        onChange(QStringLiteral("Distance"), v);
    });

    // --- Face count (read-only) ---
    auto* faceLbl = new QLabel(QString::number(static_cast<int>(p.faceIndices.size())));
    layout->addRow(QObject::tr("Faces"), faceLbl);
}

// ---------------------------------------------------------------------------
// buildGenericForm — fallback for unknown feature types
// ---------------------------------------------------------------------------
void PropertyFormFactory::buildGenericForm(QFormLayout* layout,
                                           const QString& featureId,
                                           const features::Feature* feat)
{
    layout->addRow(QObject::tr("ID"), new QLabel(featureId));
    layout->addRow(QObject::tr("Type"),
        new QLabel(QString::number(static_cast<int>(feat->type()))));
}
