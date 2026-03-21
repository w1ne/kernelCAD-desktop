#include "PropertiesPanel.h"

#include "../document/Document.h"
#include "../kernel/Appearance.h"
#include "../document/Timeline.h"
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

#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QScrollArea>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QFrame>
#include <QKeyEvent>
#include <QApplication>
#include <QColorDialog>
#include <QPushButton>

#include <sstream>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Stylesheet constants (dark-theme-friendly)
// ---------------------------------------------------------------------------
static const char* kPanelStyle = R"(
    PropertiesPanel {
        background-color: #2b2b2b;
    }
    QLabel#headerLabel {
        color: #e0e0e0;
        font-weight: bold;
        font-size: 13px;
        padding: 6px 4px 2px 4px;
    }
    QLabel {
        color: #cccccc;
    }
    QDoubleSpinBox, QSpinBox, QComboBox, QLineEdit {
        background-color: #3c3f41;
        color: #e0e0e0;
        border: 1px solid #555555;
        border-radius: 3px;
        padding: 2px 4px;
        min-height: 22px;
    }
    QDoubleSpinBox:focus, QSpinBox:focus, QComboBox:focus, QLineEdit:focus {
        border: 1px solid #5294e2;
    }
    QCheckBox {
        color: #cccccc;
        spacing: 6px;
    }
    QCheckBox::indicator {
        width: 16px; height: 16px;
        border: 1px solid #555555;
        border-radius: 2px;
        background-color: #3c3f41;
    }
    QCheckBox::indicator:checked {
        background-color: #5294e2;
        border-color: #5294e2;
    }
    QFrame#separator {
        color: #555555;
    }
)";

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
PropertiesPanel::PropertiesPanel(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet(kPanelStyle);

    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);

    // Header label (feature name + type)
    m_headerLabel = new QLabel(tr("No selection"), this);
    m_headerLabel->setObjectName("headerLabel");
    m_rootLayout->addWidget(m_headerLabel);

    // Horizontal separator
    auto* sep = new QFrame(this);
    sep->setObjectName("separator");
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    m_rootLayout->addWidget(sep);

    // Scrollable form area
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet("QScrollArea { background-color: #2b2b2b; }");

    m_formContainer = new QWidget();
    m_formContainer->setStyleSheet("background-color: #2b2b2b;");
    m_formLayout = new QFormLayout(m_formContainer);
    m_formLayout->setContentsMargins(8, 6, 8, 6);
    m_formLayout->setSpacing(6);
    m_formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_scrollArea->setWidget(m_formContainer);
    m_rootLayout->addWidget(m_scrollArea, 1);

    // Debounce timer: single-shot, 50 ms delay for live preview updates
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(50);
    connect(&m_debounceTimer, &QTimer::timeout, this, [this]() {
        if (!m_pendingFeatureId.isEmpty())
            emit propertyChanged(m_pendingFeatureId, m_pendingPropertyName, m_pendingValue);
    });

    // Enable focus tracking so we can detect when editing finishes
    setFocusPolicy(Qt::StrongFocus);
    installEventFilter(this);
}

// ---------------------------------------------------------------------------
// clear()
// ---------------------------------------------------------------------------
void PropertiesPanel::clear()
{
    m_currentFeatureId.clear();
    m_headerLabel->setText(
        QStringLiteral("<span style='color:#888; font-style:italic;'>"
                       "Select a feature or body to view properties</span>"));
    clearFormWidgets();
}

// ---------------------------------------------------------------------------
// clearFormWidgets — remove every row from the form layout
// ---------------------------------------------------------------------------
void PropertiesPanel::clearFormWidgets()
{
    if (!m_formLayout)
        return;

    while (m_formLayout->rowCount() > 0)
        m_formLayout->removeRow(0);
}

// ---------------------------------------------------------------------------
// setHeaderText
// ---------------------------------------------------------------------------
void PropertiesPanel::setHeaderText(const QString& featureName, const QString& featureType)
{
    if (m_editMode) {
        m_headerLabel->setText(
            QStringLiteral("<span style='color:#00A0FF;'>Editing:</span> "
                           "<b>%1</b> &nbsp;<span style='color:#888;'>(%2)</span>")
                .arg(featureName.toHtmlEscaped(), featureType.toHtmlEscaped()));
    } else {
        m_headerLabel->setText(
            QStringLiteral("<b>%1</b> &nbsp;<span style='color:#888;'>(%2)</span>")
                .arg(featureName.toHtmlEscaped(), featureType.toHtmlEscaped()));
    }
}

void PropertiesPanel::setEditMode(bool editing)
{
    if (m_editMode != editing) {
        m_editMode = editing;
        // Re-show the current feature to update the header text
        if (!m_currentFeatureId.isEmpty())
            showFeature(m_currentFeatureId);
    }
}

// ---------------------------------------------------------------------------
// setDocument
// ---------------------------------------------------------------------------
void PropertiesPanel::setDocument(document::Document* doc)
{
    m_document = doc;
}

// ---------------------------------------------------------------------------
// showBodyProperties — display body statistics and appearance controls
// ---------------------------------------------------------------------------
void PropertiesPanel::showBodyProperties(const std::string& bodyId)
{
    clear();

    if (!m_document || bodyId.empty())
        return;

    auto& brep = m_document->brepModel();
    if (!brep.hasBody(bodyId))
        return;

    // Header
    m_headerLabel->setText(
        QStringLiteral("<b>%1</b> &nbsp;<span style='color:#888;'>(Body)</span>")
            .arg(QString::fromStdString(bodyId).toHtmlEscaped()));

    // --- Section 1: Body Statistics ---
    {
        auto* sectionLabel = new QLabel(
            QStringLiteral("<span style='color:#5294e2; font-weight:bold;'>"
                           "Body Statistics</span>"));
        sectionLabel->setTextFormat(Qt::RichText);
        m_formLayout->addRow(sectionLabel);
    }

    // Get material density for this body
    const auto& bodyMat = m_document->appearances().bodyMaterial(bodyId);
    double density = bodyMat.density;

    try {
        auto phys = brep.getProperties(bodyId, density);

        auto addReadOnly = [this](const QString& label, const QString& value) {
            auto* valLabel = new QLabel(value);
            valLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            m_formLayout->addRow(label, valLabel);
        };

        addReadOnly(tr("Volume"),
                     QString::number(phys.volume, 'f', 2) + QStringLiteral(" mm\u00B3"));
        addReadOnly(tr("Surface Area"),
                     QString::number(phys.surfaceArea, 'f', 2) + QStringLiteral(" mm\u00B2"));
        addReadOnly(tr("Mass"),
                     QString::number(phys.mass, 'f', 2) + QStringLiteral(" g"));
        addReadOnly(tr("Center of Gravity"),
                     QStringLiteral("(%1, %2, %3)")
                         .arg(phys.cogX, 0, 'f', 3)
                         .arg(phys.cogY, 0, 'f', 3)
                         .arg(phys.cogZ, 0, 'f', 3));
        addReadOnly(tr("Bounding Box"),
                     QStringLiteral("(%1, %2, %3) \u2014 (%4, %5, %6)")
                         .arg(phys.bboxMinX, 0, 'f', 2)
                         .arg(phys.bboxMinY, 0, 'f', 2)
                         .arg(phys.bboxMinZ, 0, 'f', 2)
                         .arg(phys.bboxMaxX, 0, 'f', 2)
                         .arg(phys.bboxMaxY, 0, 'f', 2)
                         .arg(phys.bboxMaxZ, 0, 'f', 2));
    } catch (...) {
        auto* errLabel = new QLabel(tr("Could not compute properties"));
        m_formLayout->addRow(errLabel);
    }

    // --- Section 2: Appearance ---
    {
        auto* sep = new QFrame();
        sep->setObjectName("separator");
        sep->setFrameShape(QFrame::HLine);
        sep->setFrameShadow(QFrame::Sunken);
        m_formLayout->addRow(sep);

        auto* sectionLabel = new QLabel(
            QStringLiteral("<span style='color:#5294e2; font-weight:bold;'>"
                           "Appearance</span>"));
        sectionLabel->setTextFormat(Qt::RichText);
        m_formLayout->addRow(sectionLabel);
    }

    // Color picker button
    QColor currentColor = QColor::fromRgbF(bodyMat.baseR, bodyMat.baseG, bodyMat.baseB);
    auto* colorBtn = new QPushButton();
    colorBtn->setFixedSize(60, 24);
    colorBtn->setStyleSheet(
        QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;")
            .arg(currentColor.name()));
    colorBtn->setToolTip(tr("Click to change body color"));

    QString bodyIdQ = QString::fromStdString(bodyId);
    connect(colorBtn, &QPushButton::clicked, this, [this, bodyIdQ, colorBtn]() {
        QColor chosen = QColorDialog::getColor(
            colorBtn->palette().button().color(), this, tr("Body Color"));
        if (chosen.isValid()) {
            colorBtn->setStyleSheet(
                QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;")
                    .arg(chosen.name()));
            emit bodyColorChanged(bodyIdQ, chosen);
        }
    });
    m_formLayout->addRow(tr("Color"), colorBtn);

    // Material dropdown (reuse existing method)
    addMaterialDropdown(bodyIdQ, QString::fromStdString(bodyMat.name));
}

// ---------------------------------------------------------------------------
// parseExprValue — extract leading numeric value from "10 mm", "360 deg" etc.
// ---------------------------------------------------------------------------
double PropertiesPanel::parseExprValue(const std::string& expr)
{
    if (expr.empty())
        return 0.0;
    char* end = nullptr;
    double v = std::strtod(expr.c_str(), &end);
    return (end != expr.c_str()) ? v : 0.0;
}

// ---------------------------------------------------------------------------
// showFeature(featureId) — look up feature in document, build typed form
// ---------------------------------------------------------------------------
void PropertiesPanel::showFeature(const QString& featureId)
{
    clear();
    m_currentFeatureId = featureId;

    if (!m_document) {
        // Fallback: no document set, show generic placeholder
        setHeaderText(featureId, QStringLiteral("Unknown"));
        return;
    }

    // Look up the feature in the timeline by matching entry ID
    auto& tl = m_document->timeline();
    features::Feature* feat = nullptr;
    for (size_t i = 0; i < tl.count(); ++i) {
        auto& entry = tl.entry(i);
        if (entry.id == featureId.toStdString() ||
            (entry.feature && entry.feature->id() == featureId.toStdString()))
        {
            feat = entry.feature.get();
            break;
        }
    }

    if (!feat) {
        setHeaderText(featureId, QStringLiteral("Not Found"));
        return;
    }

    using FT = features::FeatureType;
    switch (feat->type()) {
    case FT::Extrude:
        buildExtrudeForm(featureId, static_cast<const features::ExtrudeFeature*>(feat));
        break;
    case FT::Revolve:
        buildRevolveForm(featureId, static_cast<const features::RevolveFeature*>(feat));
        break;
    case FT::Fillet:
        buildFilletForm(featureId, static_cast<const features::FilletFeature*>(feat));
        break;
    case FT::Chamfer:
        buildChamferForm(featureId, static_cast<const features::ChamferFeature*>(feat));
        break;
    case FT::Sketch:
        buildSketchForm(featureId, static_cast<const features::SketchFeature*>(feat));
        break;
    case FT::RectangularPattern:
        buildRectangularPatternForm(featureId, static_cast<const features::RectangularPatternFeature*>(feat));
        break;
    case FT::Shell:
        buildShellForm(featureId, static_cast<const features::ShellFeature*>(feat));
        break;
    case FT::Sweep:
        buildSweepForm(featureId, static_cast<const features::SweepFeature*>(feat));
        break;
    case FT::Loft:
        buildLoftForm(featureId, static_cast<const features::LoftFeature*>(feat));
        break;
    case FT::Hole:
        buildHoleForm(featureId, static_cast<const features::HoleFeature*>(feat));
        break;
    case FT::Mirror:
        buildMirrorForm(featureId, static_cast<const features::MirrorFeature*>(feat));
        break;
    case FT::CircularPattern:
        buildCircularPatternForm(featureId, static_cast<const features::CircularPatternFeature*>(feat));
        break;
    case FT::Move:
        buildMoveForm(featureId, static_cast<const features::MoveFeature*>(feat));
        break;
    case FT::Draft:
        buildDraftForm(featureId, static_cast<const features::DraftFeature*>(feat));
        break;
    case FT::Thicken:
        buildThickenForm(featureId, static_cast<const features::ThickenFeature*>(feat));
        break;
    case FT::Thread:
        buildThreadForm(featureId, static_cast<const features::ThreadFeature*>(feat));
        break;
    case FT::Scale:
        buildScaleForm(featureId, static_cast<const features::ScaleFeature*>(feat));
        break;
    case FT::Combine:
        buildCombineForm(featureId, static_cast<const features::CombineFeature*>(feat));
        break;
    case FT::SplitBody:
        buildSplitBodyForm(featureId, static_cast<const features::SplitBodyFeature*>(feat));
        break;
    case FT::OffsetFaces:
        buildOffsetFacesForm(featureId, static_cast<const features::OffsetFacesFeature*>(feat));
        break;
    default:
        buildGenericForm(featureId, feat);
        break;
    }
}

// ---------------------------------------------------------------------------
// buildExtrudeForm — populate from actual ExtrudeParams
// ---------------------------------------------------------------------------
void PropertiesPanel::buildExtrudeForm(const QString& featureId,
                                       const features::ExtrudeFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("ExtrudeFeature"));

    // --- Distance ---
    auto* distSpin = new QDoubleSpinBox();
    distSpin->setObjectName("Distance");
    distSpin->setRange(0.0, 100000.0);
    distSpin->setDecimals(4);
    distSpin->setSuffix(" mm");
    distSpin->setValue(parseExprValue(p.distanceExpr));
    m_formLayout->addRow(tr("Distance"), distSpin);

    connect(distSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("Distance"), v);
        });

    // --- Extent Type ---
    auto* extentCombo = new QComboBox();
    extentCombo->setObjectName("ExtentType");
    extentCombo->addItems({tr("Distance"), tr("ThroughAll"), tr("ToEntity"), tr("Symmetric")});
    extentCombo->setCurrentIndex(static_cast<int>(p.extentType));
    m_formLayout->addRow(tr("Extent Type"), extentCombo);

    connect(extentCombo, &QComboBox::currentIndexChanged, this,
        [this, featureId, extentCombo](int idx) {
            schedulePropertyChanged(featureId, QStringLiteral("ExtentType"),
                                 extentCombo->itemText(idx));
        });

    // --- Operation ---
    auto* opCombo = new QComboBox();
    opCombo->setObjectName("Operation");
    opCombo->addItems({tr("NewBody"), tr("Join"), tr("Cut"), tr("Intersect")});
    opCombo->setCurrentIndex(static_cast<int>(p.operation));
    m_formLayout->addRow(tr("Operation"), opCombo);

    connect(opCombo, &QComboBox::currentIndexChanged, this,
        [this, featureId, opCombo](int idx) {
            schedulePropertyChanged(featureId, QStringLiteral("Operation"),
                                 opCombo->itemText(idx));
        });

    // --- Taper Angle ---
    auto* taperSpin = new QDoubleSpinBox();
    taperSpin->setObjectName("TaperAngle");
    taperSpin->setRange(-89.9, 89.9);
    taperSpin->setDecimals(2);
    taperSpin->setSuffix(QStringLiteral(" \u00B0")); // degree sign
    taperSpin->setValue(p.taperAngleDeg);
    m_formLayout->addRow(tr("Taper Angle"), taperSpin);

    connect(taperSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("TaperAngle"), v);
        });

    // --- Symmetric ---
    auto* symCheck = new QCheckBox();
    symCheck->setObjectName("Symmetric");
    symCheck->setChecked(p.isSymmetric);
    m_formLayout->addRow(tr("Symmetric"), symCheck);

    connect(symCheck, &QCheckBox::stateChanged, this,
        [this, featureId](int state) {
            schedulePropertyChanged(featureId, QStringLiteral("Symmetric"),
                                 state == Qt::Checked);
        });
}

// ---------------------------------------------------------------------------
// buildRevolveForm — populate from actual RevolveParams
// ---------------------------------------------------------------------------
void PropertiesPanel::buildRevolveForm(const QString& featureId,
                                       const features::RevolveFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("RevolveFeature"));

    // --- Angle ---
    auto* angleSpin = new QDoubleSpinBox();
    angleSpin->setObjectName("Angle");
    angleSpin->setRange(0.0, 360.0);
    angleSpin->setDecimals(2);
    angleSpin->setSuffix(QStringLiteral(" \u00B0"));
    angleSpin->setValue(parseExprValue(p.angleExpr));
    m_formLayout->addRow(tr("Angle"), angleSpin);

    connect(angleSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("Angle"), v);
        });

    // --- Axis Type ---
    auto* axisCombo = new QComboBox();
    axisCombo->setObjectName("AxisType");
    axisCombo->addItems({tr("XAxis"), tr("YAxis"), tr("ZAxis"), tr("Custom")});
    axisCombo->setCurrentIndex(static_cast<int>(p.axisType));
    m_formLayout->addRow(tr("Axis Type"), axisCombo);

    connect(axisCombo, &QComboBox::currentIndexChanged, this,
        [this, featureId, axisCombo](int idx) {
            schedulePropertyChanged(featureId, QStringLiteral("AxisType"),
                                 axisCombo->itemText(idx));
        });

    // --- Full Revolution ---
    auto* fullCheck = new QCheckBox();
    fullCheck->setObjectName("FullRevolution");
    fullCheck->setChecked(p.isFullRevolution);
    m_formLayout->addRow(tr("Full Revolution"), fullCheck);

    connect(fullCheck, &QCheckBox::stateChanged, this,
        [this, featureId](int state) {
            schedulePropertyChanged(featureId, QStringLiteral("FullRevolution"),
                                 state == Qt::Checked);
        });
}

// ---------------------------------------------------------------------------
// buildFilletForm — populate from actual FilletParams
// ---------------------------------------------------------------------------
void PropertiesPanel::buildFilletForm(const QString& featureId,
                                      const features::FilletFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("FilletFeature"));

    // --- Radius ---
    auto* radiusSpin = new QDoubleSpinBox();
    radiusSpin->setObjectName("Radius");
    radiusSpin->setRange(0.0, 100000.0);
    radiusSpin->setDecimals(4);
    radiusSpin->setSuffix(" mm");
    radiusSpin->setValue(parseExprValue(p.radiusExpr));
    m_formLayout->addRow(tr("Radius"), radiusSpin);

    connect(radiusSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("Radius"), v);
        });

    // --- Edge count (read-only) ---
    auto* edgeLbl = new QLabel(QString::number(static_cast<int>(p.edgeIds.size())));
    m_formLayout->addRow(tr("Edges"), edgeLbl);
}

// ---------------------------------------------------------------------------
// buildChamferForm — populate from actual ChamferParams
// ---------------------------------------------------------------------------
void PropertiesPanel::buildChamferForm(const QString& featureId,
                                       const features::ChamferFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("ChamferFeature"));

    // --- Distance ---
    auto* distSpin = new QDoubleSpinBox();
    distSpin->setObjectName("Distance");
    distSpin->setRange(0.0, 100000.0);
    distSpin->setDecimals(4);
    distSpin->setSuffix(" mm");
    distSpin->setValue(parseExprValue(p.distanceExpr));
    m_formLayout->addRow(tr("Distance"), distSpin);

    connect(distSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("Distance"), v);
        });

    // --- Chamfer Type ---
    auto* typeCombo = new QComboBox();
    typeCombo->setObjectName("ChamferType");
    typeCombo->addItems({tr("EqualDistance"), tr("TwoDistances"), tr("DistanceAndAngle")});
    typeCombo->setCurrentIndex(static_cast<int>(p.chamferType));
    m_formLayout->addRow(tr("Chamfer Type"), typeCombo);

    connect(typeCombo, &QComboBox::currentIndexChanged, this,
        [this, featureId, typeCombo](int idx) {
            schedulePropertyChanged(featureId, QStringLiteral("ChamferType"),
                                 typeCombo->itemText(idx));
        });

    // --- Edge count (read-only) ---
    auto* edgeLbl = new QLabel(QString::number(static_cast<int>(p.edgeIds.size())));
    m_formLayout->addRow(tr("Edges"), edgeLbl);
}

// ---------------------------------------------------------------------------
// buildSketchForm — show sketch summary info (read-only)
// ---------------------------------------------------------------------------
void PropertiesPanel::buildSketchForm(const QString& featureId,
                                      const features::SketchFeature* feat)
{
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("SketchFeature"));

    const auto& sk = feat->sketch();
    m_formLayout->addRow(tr("Plane"),
        new QLabel(QString::fromStdString(feat->params().planeId)));
    m_formLayout->addRow(tr("Points"),
        new QLabel(QString::number(static_cast<int>(sk.points().size()))));
    m_formLayout->addRow(tr("Lines"),
        new QLabel(QString::number(static_cast<int>(sk.lines().size()))));
    m_formLayout->addRow(tr("Circles"),
        new QLabel(QString::number(static_cast<int>(sk.circles().size()))));
    m_formLayout->addRow(tr("Arcs"),
        new QLabel(QString::number(static_cast<int>(sk.arcs().size()))));
    m_formLayout->addRow(tr("Constraints"),
        new QLabel(QString::number(static_cast<int>(sk.constraints().size()))));
}

// ---------------------------------------------------------------------------
// buildRectangularPatternForm
// ---------------------------------------------------------------------------
void PropertiesPanel::buildRectangularPatternForm(const QString& featureId,
                                                   const features::RectangularPatternFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("RectangularPatternFeature"));

    m_formLayout->addRow(tr("Dir1 Count"),
        new QLabel(QString::number(p.count1)));
    m_formLayout->addRow(tr("Dir1 Spacing"),
        new QLabel(QString::fromStdString(p.spacing1Expr)));
    m_formLayout->addRow(tr("Dir2 Count"),
        new QLabel(QString::number(p.count2)));
    m_formLayout->addRow(tr("Dir2 Spacing"),
        new QLabel(QString::fromStdString(p.spacing2Expr)));
}

// ---------------------------------------------------------------------------
// buildShellForm — thickness + removed face count
// ---------------------------------------------------------------------------
void PropertiesPanel::buildShellForm(const QString& featureId,
                                     const features::ShellFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("ShellFeature"));

    // --- Thickness ---
    auto* thickSpin = new QDoubleSpinBox();
    thickSpin->setObjectName("Thickness");
    thickSpin->setRange(0.001, 100000.0);
    thickSpin->setDecimals(4);
    thickSpin->setSuffix(" mm");
    thickSpin->setValue(p.thicknessExpr);
    m_formLayout->addRow(tr("Thickness"), thickSpin);

    connect(thickSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("Thickness"), v);
        });

    // --- Removed faces (read-only) ---
    auto* faceLbl = new QLabel(QString::number(static_cast<int>(p.removedFaceIds.size())));
    m_formLayout->addRow(tr("Removed Faces"), faceLbl);
}

// ---------------------------------------------------------------------------
// buildSweepForm — orientation checkbox
// ---------------------------------------------------------------------------
void PropertiesPanel::buildSweepForm(const QString& featureId,
                                     const features::SweepFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("SweepFeature"));

    // --- Orientation ---
    auto* orientCheck = new QCheckBox(tr("Perpendicular"));
    orientCheck->setObjectName("Orientation");
    orientCheck->setChecked(p.isPerpendicularOrientation);
    m_formLayout->addRow(tr("Orientation"), orientCheck);

    connect(orientCheck, &QCheckBox::stateChanged, this,
        [this, featureId](int state) {
            schedulePropertyChanged(featureId, QStringLiteral("Orientation"),
                                 state == Qt::Checked);
        });

    // --- Operation ---
    auto* opCombo = new QComboBox();
    opCombo->setObjectName("Operation");
    opCombo->addItems({tr("NewBody"), tr("Join"), tr("Cut"), tr("Intersect")});
    opCombo->setCurrentIndex(static_cast<int>(p.operation));
    m_formLayout->addRow(tr("Operation"), opCombo);

    connect(opCombo, &QComboBox::currentIndexChanged, this,
        [this, featureId, opCombo](int idx) {
            schedulePropertyChanged(featureId, QStringLiteral("Operation"),
                                 opCombo->itemText(idx));
        });
}

// ---------------------------------------------------------------------------
// buildLoftForm — isClosed checkbox + section count
// ---------------------------------------------------------------------------
void PropertiesPanel::buildLoftForm(const QString& featureId,
                                    const features::LoftFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("LoftFeature"));

    // --- Closed ---
    auto* closedCheck = new QCheckBox();
    closedCheck->setObjectName("IsClosed");
    closedCheck->setChecked(p.isClosed);
    m_formLayout->addRow(tr("Closed"), closedCheck);

    connect(closedCheck, &QCheckBox::stateChanged, this,
        [this, featureId](int state) {
            schedulePropertyChanged(featureId, QStringLiteral("IsClosed"),
                                 state == Qt::Checked);
        });

    // --- Operation ---
    auto* opCombo = new QComboBox();
    opCombo->setObjectName("Operation");
    opCombo->addItems({tr("NewBody"), tr("Join"), tr("Cut"), tr("Intersect")});
    opCombo->setCurrentIndex(static_cast<int>(p.operation));
    m_formLayout->addRow(tr("Operation"), opCombo);

    connect(opCombo, &QComboBox::currentIndexChanged, this,
        [this, featureId, opCombo](int idx) {
            schedulePropertyChanged(featureId, QStringLiteral("Operation"),
                                 opCombo->itemText(idx));
        });

    // --- Section count (read-only) ---
    auto* secLbl = new QLabel(QString::number(static_cast<int>(p.sectionIds.size())));
    m_formLayout->addRow(tr("Sections"), secLbl);
}

// ---------------------------------------------------------------------------
// buildHoleForm — type combo, dimensions, counterbore/countersink fields
// ---------------------------------------------------------------------------
void PropertiesPanel::buildHoleForm(const QString& featureId,
                                    const features::HoleFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("HoleFeature"));

    // --- Hole Type ---
    auto* typeCombo = new QComboBox();
    typeCombo->setObjectName("HoleType");
    typeCombo->addItems({tr("Simple"), tr("Counterbore"), tr("Countersink")});
    typeCombo->setCurrentIndex(static_cast<int>(p.holeType));
    m_formLayout->addRow(tr("Hole Type"), typeCombo);

    // --- Diameter ---
    auto* diamSpin = new QDoubleSpinBox();
    diamSpin->setObjectName("Diameter");
    diamSpin->setRange(0.001, 100000.0);
    diamSpin->setDecimals(4);
    diamSpin->setSuffix(" mm");
    diamSpin->setValue(parseExprValue(p.diameterExpr));
    m_formLayout->addRow(tr("Diameter"), diamSpin);

    connect(diamSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("Diameter"), v);
        });

    // --- Depth ---
    auto* depthSpin = new QDoubleSpinBox();
    depthSpin->setObjectName("Depth");
    depthSpin->setRange(0.0, 100000.0);
    depthSpin->setDecimals(4);
    depthSpin->setSuffix(" mm");
    depthSpin->setValue(parseExprValue(p.depthExpr));
    m_formLayout->addRow(tr("Depth (0=through)"), depthSpin);

    connect(depthSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("Depth"), v);
        });

    // --- Tip Angle ---
    auto* tipSpin = new QDoubleSpinBox();
    tipSpin->setObjectName("TipAngle");
    tipSpin->setRange(0.0, 180.0);
    tipSpin->setDecimals(2);
    tipSpin->setSuffix(QStringLiteral(" \u00B0"));
    tipSpin->setValue(p.tipAngleDeg);
    m_formLayout->addRow(tr("Tip Angle"), tipSpin);

    connect(tipSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("TipAngle"), v);
        });

    // --- Counterbore fields ---
    auto* cboreDiamSpin = new QDoubleSpinBox();
    cboreDiamSpin->setObjectName("CboreDiameter");
    cboreDiamSpin->setRange(0.001, 100000.0);
    cboreDiamSpin->setDecimals(4);
    cboreDiamSpin->setSuffix(" mm");
    cboreDiamSpin->setValue(parseExprValue(p.cboreDiameterExpr));
    m_formLayout->addRow(tr("Cbore Diameter"), cboreDiamSpin);

    connect(cboreDiamSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("CboreDiameter"), v);
        });

    auto* cboreDepthSpin = new QDoubleSpinBox();
    cboreDepthSpin->setObjectName("CboreDepth");
    cboreDepthSpin->setRange(0.0, 100000.0);
    cboreDepthSpin->setDecimals(4);
    cboreDepthSpin->setSuffix(" mm");
    cboreDepthSpin->setValue(parseExprValue(p.cboreDepthExpr));
    m_formLayout->addRow(tr("Cbore Depth"), cboreDepthSpin);

    connect(cboreDepthSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("CboreDepth"), v);
        });

    // --- Countersink fields ---
    auto* csinkDiamSpin = new QDoubleSpinBox();
    csinkDiamSpin->setObjectName("CsinkDiameter");
    csinkDiamSpin->setRange(0.001, 100000.0);
    csinkDiamSpin->setDecimals(4);
    csinkDiamSpin->setSuffix(" mm");
    csinkDiamSpin->setValue(parseExprValue(p.csinkDiameterExpr));
    m_formLayout->addRow(tr("Csink Diameter"), csinkDiamSpin);

    connect(csinkDiamSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("CsinkDiameter"), v);
        });

    auto* csinkAngleSpin = new QDoubleSpinBox();
    csinkAngleSpin->setObjectName("CsinkAngle");
    csinkAngleSpin->setRange(0.0, 180.0);
    csinkAngleSpin->setDecimals(2);
    csinkAngleSpin->setSuffix(QStringLiteral(" \u00B0"));
    csinkAngleSpin->setValue(p.csinkAngleDeg);
    m_formLayout->addRow(tr("Csink Angle"), csinkAngleSpin);

    connect(csinkAngleSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("CsinkAngle"), v);
        });

    // Show/hide counterbore and countersink fields based on hole type
    auto updateVisibility = [=]() {
        int idx = typeCombo->currentIndex();
        bool isCbore = (idx == static_cast<int>(features::HoleType::Counterbore));
        bool isCsink = (idx == static_cast<int>(features::HoleType::Countersink));

        cboreDiamSpin->setVisible(isCbore);
        m_formLayout->labelForField(cboreDiamSpin)->setVisible(isCbore);
        cboreDepthSpin->setVisible(isCbore);
        m_formLayout->labelForField(cboreDepthSpin)->setVisible(isCbore);

        csinkDiamSpin->setVisible(isCsink);
        m_formLayout->labelForField(csinkDiamSpin)->setVisible(isCsink);
        csinkAngleSpin->setVisible(isCsink);
        m_formLayout->labelForField(csinkAngleSpin)->setVisible(isCsink);
    };

    updateVisibility();

    connect(typeCombo, &QComboBox::currentIndexChanged, this,
        [this, featureId, typeCombo, updateVisibility](int idx) {
            schedulePropertyChanged(featureId, QStringLiteral("HoleType"),
                                 typeCombo->itemText(idx));
            updateVisibility();
        });
}

// ---------------------------------------------------------------------------
// buildMirrorForm — plane origin/normal XYZ, isCombine
// ---------------------------------------------------------------------------
void PropertiesPanel::buildMirrorForm(const QString& featureId,
                                      const features::MirrorFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("MirrorFeature"));

    // --- Plane Origin X ---
    auto* oxSpin = new QDoubleSpinBox();
    oxSpin->setObjectName("PlaneOx");
    oxSpin->setRange(-1e9, 1e9);
    oxSpin->setDecimals(4);
    oxSpin->setSuffix(" mm");
    oxSpin->setValue(p.planeOx);
    m_formLayout->addRow(tr("Plane Origin X"), oxSpin);

    connect(oxSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("PlaneOx"), v);
        });

    // --- Plane Origin Y ---
    auto* oySpin = new QDoubleSpinBox();
    oySpin->setObjectName("PlaneOy");
    oySpin->setRange(-1e9, 1e9);
    oySpin->setDecimals(4);
    oySpin->setSuffix(" mm");
    oySpin->setValue(p.planeOy);
    m_formLayout->addRow(tr("Plane Origin Y"), oySpin);

    connect(oySpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("PlaneOy"), v);
        });

    // --- Plane Origin Z ---
    auto* ozSpin = new QDoubleSpinBox();
    ozSpin->setObjectName("PlaneOz");
    ozSpin->setRange(-1e9, 1e9);
    ozSpin->setDecimals(4);
    ozSpin->setSuffix(" mm");
    ozSpin->setValue(p.planeOz);
    m_formLayout->addRow(tr("Plane Origin Z"), ozSpin);

    connect(ozSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("PlaneOz"), v);
        });

    // --- Plane Normal X ---
    auto* nxSpin = new QDoubleSpinBox();
    nxSpin->setObjectName("PlaneNx");
    nxSpin->setRange(-1.0, 1.0);
    nxSpin->setDecimals(6);
    nxSpin->setSingleStep(0.1);
    nxSpin->setValue(p.planeNx);
    m_formLayout->addRow(tr("Plane Normal X"), nxSpin);

    connect(nxSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("PlaneNx"), v);
        });

    // --- Plane Normal Y ---
    auto* nySpin = new QDoubleSpinBox();
    nySpin->setObjectName("PlaneNy");
    nySpin->setRange(-1.0, 1.0);
    nySpin->setDecimals(6);
    nySpin->setSingleStep(0.1);
    nySpin->setValue(p.planeNy);
    m_formLayout->addRow(tr("Plane Normal Y"), nySpin);

    connect(nySpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("PlaneNy"), v);
        });

    // --- Plane Normal Z ---
    auto* nzSpin = new QDoubleSpinBox();
    nzSpin->setObjectName("PlaneNz");
    nzSpin->setRange(-1.0, 1.0);
    nzSpin->setDecimals(6);
    nzSpin->setSingleStep(0.1);
    nzSpin->setValue(p.planeNz);
    m_formLayout->addRow(tr("Plane Normal Z"), nzSpin);

    connect(nzSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("PlaneNz"), v);
        });

    // --- Combine ---
    auto* combineCheck = new QCheckBox();
    combineCheck->setObjectName("IsCombine");
    combineCheck->setChecked(p.isCombine);
    m_formLayout->addRow(tr("Combine"), combineCheck);

    connect(combineCheck, &QCheckBox::stateChanged, this,
        [this, featureId](int state) {
            schedulePropertyChanged(featureId, QStringLiteral("IsCombine"),
                                 state == Qt::Checked);
        });
}

// ---------------------------------------------------------------------------
// buildCircularPatternForm — axis origin/direction, count, totalAngle
// ---------------------------------------------------------------------------
void PropertiesPanel::buildCircularPatternForm(const QString& featureId,
                                                const features::CircularPatternFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("CircularPatternFeature"));

    // --- Axis Origin X ---
    auto* axOxSpin = new QDoubleSpinBox();
    axOxSpin->setObjectName("AxisOx");
    axOxSpin->setRange(-1e9, 1e9);
    axOxSpin->setDecimals(4);
    axOxSpin->setSuffix(" mm");
    axOxSpin->setValue(p.axisOx);
    m_formLayout->addRow(tr("Axis Origin X"), axOxSpin);

    connect(axOxSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("AxisOx"), v);
        });

    // --- Axis Origin Y ---
    auto* axOySpin = new QDoubleSpinBox();
    axOySpin->setObjectName("AxisOy");
    axOySpin->setRange(-1e9, 1e9);
    axOySpin->setDecimals(4);
    axOySpin->setSuffix(" mm");
    axOySpin->setValue(p.axisOy);
    m_formLayout->addRow(tr("Axis Origin Y"), axOySpin);

    connect(axOySpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("AxisOy"), v);
        });

    // --- Axis Origin Z ---
    auto* axOzSpin = new QDoubleSpinBox();
    axOzSpin->setObjectName("AxisOz");
    axOzSpin->setRange(-1e9, 1e9);
    axOzSpin->setDecimals(4);
    axOzSpin->setSuffix(" mm");
    axOzSpin->setValue(p.axisOz);
    m_formLayout->addRow(tr("Axis Origin Z"), axOzSpin);

    connect(axOzSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("AxisOz"), v);
        });

    // --- Axis Direction X ---
    auto* axDxSpin = new QDoubleSpinBox();
    axDxSpin->setObjectName("AxisDx");
    axDxSpin->setRange(-1.0, 1.0);
    axDxSpin->setDecimals(6);
    axDxSpin->setSingleStep(0.1);
    axDxSpin->setValue(p.axisDx);
    m_formLayout->addRow(tr("Axis Dir X"), axDxSpin);

    connect(axDxSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("AxisDx"), v);
        });

    // --- Axis Direction Y ---
    auto* axDySpin = new QDoubleSpinBox();
    axDySpin->setObjectName("AxisDy");
    axDySpin->setRange(-1.0, 1.0);
    axDySpin->setDecimals(6);
    axDySpin->setSingleStep(0.1);
    axDySpin->setValue(p.axisDy);
    m_formLayout->addRow(tr("Axis Dir Y"), axDySpin);

    connect(axDySpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("AxisDy"), v);
        });

    // --- Axis Direction Z ---
    auto* axDzSpin = new QDoubleSpinBox();
    axDzSpin->setObjectName("AxisDz");
    axDzSpin->setRange(-1.0, 1.0);
    axDzSpin->setDecimals(6);
    axDzSpin->setSingleStep(0.1);
    axDzSpin->setValue(p.axisDz);
    m_formLayout->addRow(tr("Axis Dir Z"), axDzSpin);

    connect(axDzSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("AxisDz"), v);
        });

    // --- Count ---
    auto* countSpin = new QSpinBox();
    countSpin->setObjectName("Count");
    countSpin->setRange(1, 10000);
    countSpin->setValue(p.count);
    m_formLayout->addRow(tr("Count"), countSpin);

    connect(countSpin, &QSpinBox::valueChanged, this,
        [this, featureId](int v) {
            schedulePropertyChanged(featureId, QStringLiteral("Count"), v);
        });

    // --- Total Angle ---
    auto* angleSpin = new QDoubleSpinBox();
    angleSpin->setObjectName("TotalAngle");
    angleSpin->setRange(0.0, 360.0);
    angleSpin->setDecimals(2);
    angleSpin->setSuffix(QStringLiteral(" \u00B0"));
    angleSpin->setValue(p.totalAngleDeg);
    m_formLayout->addRow(tr("Total Angle"), angleSpin);

    connect(angleSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("TotalAngle"), v);
        });
}

// ---------------------------------------------------------------------------
// buildMoveForm — mode combo, translate/rotate fields shown conditionally
// ---------------------------------------------------------------------------
void PropertiesPanel::buildMoveForm(const QString& featureId,
                                    const features::MoveFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("MoveFeature"));

    // --- Mode ---
    auto* modeCombo = new QComboBox();
    modeCombo->setObjectName("Mode");
    modeCombo->addItems({tr("FreeTransform"), tr("Translate"), tr("Rotate")});
    // MoveMode enum: FreeTransform=0, TranslateXYZ=1, Rotate=2
    modeCombo->setCurrentIndex(static_cast<int>(p.mode));
    m_formLayout->addRow(tr("Mode"), modeCombo);

    // --- Translate fields ---
    auto* dxSpin = new QDoubleSpinBox();
    dxSpin->setObjectName("DX");
    dxSpin->setRange(-1e9, 1e9);
    dxSpin->setDecimals(4);
    dxSpin->setSuffix(" mm");
    dxSpin->setValue(p.dx);
    m_formLayout->addRow(tr("DX"), dxSpin);

    connect(dxSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("DX"), v);
        });

    auto* dySpin = new QDoubleSpinBox();
    dySpin->setObjectName("DY");
    dySpin->setRange(-1e9, 1e9);
    dySpin->setDecimals(4);
    dySpin->setSuffix(" mm");
    dySpin->setValue(p.dy);
    m_formLayout->addRow(tr("DY"), dySpin);

    connect(dySpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("DY"), v);
        });

    auto* dzSpin = new QDoubleSpinBox();
    dzSpin->setObjectName("DZ");
    dzSpin->setRange(-1e9, 1e9);
    dzSpin->setDecimals(4);
    dzSpin->setSuffix(" mm");
    dzSpin->setValue(p.dz);
    m_formLayout->addRow(tr("DZ"), dzSpin);

    connect(dzSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("DZ"), v);
        });

    // --- Rotate fields ---
    auto* rAxOxSpin = new QDoubleSpinBox();
    rAxOxSpin->setObjectName("RotAxisOx");
    rAxOxSpin->setRange(-1e9, 1e9);
    rAxOxSpin->setDecimals(4);
    rAxOxSpin->setSuffix(" mm");
    rAxOxSpin->setValue(p.axisOx);
    m_formLayout->addRow(tr("Rot Axis Origin X"), rAxOxSpin);

    connect(rAxOxSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("RotAxisOx"), v);
        });

    auto* rAxOySpin = new QDoubleSpinBox();
    rAxOySpin->setObjectName("RotAxisOy");
    rAxOySpin->setRange(-1e9, 1e9);
    rAxOySpin->setDecimals(4);
    rAxOySpin->setSuffix(" mm");
    rAxOySpin->setValue(p.axisOy);
    m_formLayout->addRow(tr("Rot Axis Origin Y"), rAxOySpin);

    connect(rAxOySpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("RotAxisOy"), v);
        });

    auto* rAxOzSpin = new QDoubleSpinBox();
    rAxOzSpin->setObjectName("RotAxisOz");
    rAxOzSpin->setRange(-1e9, 1e9);
    rAxOzSpin->setDecimals(4);
    rAxOzSpin->setSuffix(" mm");
    rAxOzSpin->setValue(p.axisOz);
    m_formLayout->addRow(tr("Rot Axis Origin Z"), rAxOzSpin);

    connect(rAxOzSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("RotAxisOz"), v);
        });

    auto* rAxDxSpin = new QDoubleSpinBox();
    rAxDxSpin->setObjectName("RotAxisDx");
    rAxDxSpin->setRange(-1.0, 1.0);
    rAxDxSpin->setDecimals(6);
    rAxDxSpin->setSingleStep(0.1);
    rAxDxSpin->setValue(p.axisDx);
    m_formLayout->addRow(tr("Rot Axis Dir X"), rAxDxSpin);

    connect(rAxDxSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("RotAxisDx"), v);
        });

    auto* rAxDySpin = new QDoubleSpinBox();
    rAxDySpin->setObjectName("RotAxisDy");
    rAxDySpin->setRange(-1.0, 1.0);
    rAxDySpin->setDecimals(6);
    rAxDySpin->setSingleStep(0.1);
    rAxDySpin->setValue(p.axisDy);
    m_formLayout->addRow(tr("Rot Axis Dir Y"), rAxDySpin);

    connect(rAxDySpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("RotAxisDy"), v);
        });

    auto* rAxDzSpin = new QDoubleSpinBox();
    rAxDzSpin->setObjectName("RotAxisDz");
    rAxDzSpin->setRange(-1.0, 1.0);
    rAxDzSpin->setDecimals(6);
    rAxDzSpin->setSingleStep(0.1);
    rAxDzSpin->setValue(p.axisDz);
    m_formLayout->addRow(tr("Rot Axis Dir Z"), rAxDzSpin);

    connect(rAxDzSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("RotAxisDz"), v);
        });

    auto* rotAngleSpin = new QDoubleSpinBox();
    rotAngleSpin->setObjectName("RotAngle");
    rotAngleSpin->setRange(-360.0, 360.0);
    rotAngleSpin->setDecimals(2);
    rotAngleSpin->setSuffix(QStringLiteral(" \u00B0"));
    rotAngleSpin->setValue(p.angleDeg);
    m_formLayout->addRow(tr("Rotation Angle"), rotAngleSpin);

    connect(rotAngleSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("RotAngle"), v);
        });

    // Show/hide translate vs rotate fields based on mode
    auto updateMoveVisibility = [=]() {
        int idx = modeCombo->currentIndex();
        bool isTranslate = (idx == static_cast<int>(features::MoveMode::TranslateXYZ));
        bool isRotate    = (idx == static_cast<int>(features::MoveMode::Rotate));

        dxSpin->setVisible(isTranslate);
        m_formLayout->labelForField(dxSpin)->setVisible(isTranslate);
        dySpin->setVisible(isTranslate);
        m_formLayout->labelForField(dySpin)->setVisible(isTranslate);
        dzSpin->setVisible(isTranslate);
        m_formLayout->labelForField(dzSpin)->setVisible(isTranslate);

        rAxOxSpin->setVisible(isRotate);
        m_formLayout->labelForField(rAxOxSpin)->setVisible(isRotate);
        rAxOySpin->setVisible(isRotate);
        m_formLayout->labelForField(rAxOySpin)->setVisible(isRotate);
        rAxOzSpin->setVisible(isRotate);
        m_formLayout->labelForField(rAxOzSpin)->setVisible(isRotate);
        rAxDxSpin->setVisible(isRotate);
        m_formLayout->labelForField(rAxDxSpin)->setVisible(isRotate);
        rAxDySpin->setVisible(isRotate);
        m_formLayout->labelForField(rAxDySpin)->setVisible(isRotate);
        rAxDzSpin->setVisible(isRotate);
        m_formLayout->labelForField(rAxDzSpin)->setVisible(isRotate);
        rotAngleSpin->setVisible(isRotate);
        m_formLayout->labelForField(rotAngleSpin)->setVisible(isRotate);
    };

    updateMoveVisibility();

    connect(modeCombo, &QComboBox::currentIndexChanged, this,
        [this, featureId, modeCombo, updateMoveVisibility](int idx) {
            schedulePropertyChanged(featureId, QStringLiteral("Mode"),
                                 modeCombo->itemText(idx));
            updateMoveVisibility();
        });
}

// ---------------------------------------------------------------------------
// buildDraftForm — angle, pull direction XYZ
// ---------------------------------------------------------------------------
void PropertiesPanel::buildDraftForm(const QString& featureId,
                                     const features::DraftFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("DraftFeature"));

    // --- Angle ---
    auto* angleSpin = new QDoubleSpinBox();
    angleSpin->setObjectName("Angle");
    angleSpin->setRange(0.0, 89.9);
    angleSpin->setDecimals(2);
    angleSpin->setSuffix(QStringLiteral(" \u00B0"));
    angleSpin->setValue(parseExprValue(p.angleExpr));
    m_formLayout->addRow(tr("Angle"), angleSpin);

    connect(angleSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("Angle"), v);
        });

    // --- Pull Direction X ---
    auto* pdxSpin = new QDoubleSpinBox();
    pdxSpin->setObjectName("PullDirX");
    pdxSpin->setRange(-1.0, 1.0);
    pdxSpin->setDecimals(6);
    pdxSpin->setSingleStep(0.1);
    pdxSpin->setValue(p.pullDirX);
    m_formLayout->addRow(tr("Pull Dir X"), pdxSpin);

    connect(pdxSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("PullDirX"), v);
        });

    // --- Pull Direction Y ---
    auto* pdySpin = new QDoubleSpinBox();
    pdySpin->setObjectName("PullDirY");
    pdySpin->setRange(-1.0, 1.0);
    pdySpin->setDecimals(6);
    pdySpin->setSingleStep(0.1);
    pdySpin->setValue(p.pullDirY);
    m_formLayout->addRow(tr("Pull Dir Y"), pdySpin);

    connect(pdySpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("PullDirY"), v);
        });

    // --- Pull Direction Z ---
    auto* pdzSpin = new QDoubleSpinBox();
    pdzSpin->setObjectName("PullDirZ");
    pdzSpin->setRange(-1.0, 1.0);
    pdzSpin->setDecimals(6);
    pdzSpin->setSingleStep(0.1);
    pdzSpin->setValue(p.pullDirZ);
    m_formLayout->addRow(tr("Pull Dir Z"), pdzSpin);

    connect(pdzSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("PullDirZ"), v);
        });

    // --- Face count (read-only) ---
    auto* faceLbl = new QLabel(QString::number(static_cast<int>(p.faceIndices.size())));
    m_formLayout->addRow(tr("Faces"), faceLbl);
}

// ---------------------------------------------------------------------------
// buildThickenForm — thickness, isSymmetric
// ---------------------------------------------------------------------------
void PropertiesPanel::buildThickenForm(const QString& featureId,
                                       const features::ThickenFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("ThickenFeature"));

    // --- Thickness ---
    auto* thickSpin = new QDoubleSpinBox();
    thickSpin->setObjectName("Thickness");
    thickSpin->setRange(-100000.0, 100000.0);
    thickSpin->setDecimals(4);
    thickSpin->setSuffix(" mm");
    thickSpin->setValue(parseExprValue(p.thicknessExpr));
    m_formLayout->addRow(tr("Thickness"), thickSpin);

    connect(thickSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("Thickness"), v);
        });

    // --- Symmetric ---
    auto* symCheck = new QCheckBox();
    symCheck->setObjectName("IsSymmetric");
    symCheck->setChecked(p.isSymmetric);
    m_formLayout->addRow(tr("Symmetric"), symCheck);

    connect(symCheck, &QCheckBox::stateChanged, this,
        [this, featureId](int state) {
            schedulePropertyChanged(featureId, QStringLiteral("IsSymmetric"),
                                 state == Qt::Checked);
        });
}

// ---------------------------------------------------------------------------
// buildThreadForm — threadType, pitch, depth, checkboxes
// ---------------------------------------------------------------------------
void PropertiesPanel::buildThreadForm(const QString& featureId,
                                      const features::ThreadFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("ThreadFeature"));

    // --- Thread Type ---
    auto* typeCombo = new QComboBox();
    typeCombo->setObjectName("ThreadType");
    typeCombo->addItems({tr("Metric Coarse"), tr("Metric Fine"), tr("UNC"), tr("UNF")});
    typeCombo->setCurrentIndex(static_cast<int>(p.threadType));
    m_formLayout->addRow(tr("Thread Type"), typeCombo);

    connect(typeCombo, &QComboBox::currentIndexChanged, this,
        [this, featureId, typeCombo](int idx) {
            schedulePropertyChanged(featureId, QStringLiteral("ThreadType"),
                                 typeCombo->itemText(idx));
        });

    // --- Pitch ---
    auto* pitchSpin = new QDoubleSpinBox();
    pitchSpin->setObjectName("Pitch");
    pitchSpin->setRange(0.001, 1000.0);
    pitchSpin->setDecimals(4);
    pitchSpin->setSuffix(" mm");
    pitchSpin->setValue(p.pitch);
    m_formLayout->addRow(tr("Pitch"), pitchSpin);

    connect(pitchSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("Pitch"), v);
        });

    // --- Depth ---
    auto* depthSpin = new QDoubleSpinBox();
    depthSpin->setObjectName("Depth");
    depthSpin->setRange(0.001, 1000.0);
    depthSpin->setDecimals(4);
    depthSpin->setSuffix(" mm");
    depthSpin->setValue(p.depth);
    m_formLayout->addRow(tr("Depth"), depthSpin);

    connect(depthSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("Depth"), v);
        });

    // --- Internal ---
    auto* internalCheck = new QCheckBox();
    internalCheck->setObjectName("IsInternal");
    internalCheck->setChecked(p.isInternal);
    m_formLayout->addRow(tr("Internal"), internalCheck);

    connect(internalCheck, &QCheckBox::stateChanged, this,
        [this, featureId](int state) {
            schedulePropertyChanged(featureId, QStringLiteral("IsInternal"),
                                 state == Qt::Checked);
        });

    // --- Right-Handed ---
    auto* rhCheck = new QCheckBox();
    rhCheck->setObjectName("IsRightHanded");
    rhCheck->setChecked(p.isRightHanded);
    m_formLayout->addRow(tr("Right-Handed"), rhCheck);

    connect(rhCheck, &QCheckBox::stateChanged, this,
        [this, featureId](int state) {
            schedulePropertyChanged(featureId, QStringLiteral("IsRightHanded"),
                                 state == Qt::Checked);
        });

    // --- Modeled ---
    auto* modeledCheck = new QCheckBox();
    modeledCheck->setObjectName("IsModeled");
    modeledCheck->setChecked(p.isModeled);
    m_formLayout->addRow(tr("Modeled"), modeledCheck);

    connect(modeledCheck, &QCheckBox::stateChanged, this,
        [this, featureId](int state) {
            schedulePropertyChanged(featureId, QStringLiteral("IsModeled"),
                                 state == Qt::Checked);
        });
}

// ---------------------------------------------------------------------------
// buildScaleForm — scaleType combo, uniform/non-uniform factor fields
// ---------------------------------------------------------------------------
void PropertiesPanel::buildScaleForm(const QString& featureId,
                                     const features::ScaleFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("ScaleFeature"));

    // --- Scale Type ---
    auto* typeCombo = new QComboBox();
    typeCombo->setObjectName("ScaleType");
    typeCombo->addItems({tr("Uniform"), tr("Non-Uniform")});
    typeCombo->setCurrentIndex(static_cast<int>(p.scaleType));
    m_formLayout->addRow(tr("Scale Type"), typeCombo);

    // --- Uniform factor ---
    auto* factorSpin = new QDoubleSpinBox();
    factorSpin->setObjectName("Factor");
    factorSpin->setRange(0.001, 100000.0);
    factorSpin->setDecimals(6);
    factorSpin->setValue(p.factor);
    m_formLayout->addRow(tr("Factor"), factorSpin);

    connect(factorSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("Factor"), v);
        });

    // --- Non-uniform factors ---
    auto* fxSpin = new QDoubleSpinBox();
    fxSpin->setObjectName("FactorX");
    fxSpin->setRange(0.001, 100000.0);
    fxSpin->setDecimals(6);
    fxSpin->setValue(p.factorX);
    m_formLayout->addRow(tr("Factor X"), fxSpin);

    connect(fxSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("FactorX"), v);
        });

    auto* fySpin = new QDoubleSpinBox();
    fySpin->setObjectName("FactorY");
    fySpin->setRange(0.001, 100000.0);
    fySpin->setDecimals(6);
    fySpin->setValue(p.factorY);
    m_formLayout->addRow(tr("Factor Y"), fySpin);

    connect(fySpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("FactorY"), v);
        });

    auto* fzSpin = new QDoubleSpinBox();
    fzSpin->setObjectName("FactorZ");
    fzSpin->setRange(0.001, 100000.0);
    fzSpin->setDecimals(6);
    fzSpin->setValue(p.factorZ);
    m_formLayout->addRow(tr("Factor Z"), fzSpin);

    connect(fzSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("FactorZ"), v);
        });

    // Show/hide uniform vs non-uniform fields based on scale type
    auto updateScaleVisibility = [=]() {
        int idx = typeCombo->currentIndex();
        bool isUniform = (idx == static_cast<int>(features::ScaleType::Uniform));

        factorSpin->setVisible(isUniform);
        m_formLayout->labelForField(factorSpin)->setVisible(isUniform);

        fxSpin->setVisible(!isUniform);
        m_formLayout->labelForField(fxSpin)->setVisible(!isUniform);
        fySpin->setVisible(!isUniform);
        m_formLayout->labelForField(fySpin)->setVisible(!isUniform);
        fzSpin->setVisible(!isUniform);
        m_formLayout->labelForField(fzSpin)->setVisible(!isUniform);
    };

    updateScaleVisibility();

    connect(typeCombo, &QComboBox::currentIndexChanged, this,
        [this, featureId, typeCombo, updateScaleVisibility](int idx) {
            schedulePropertyChanged(featureId, QStringLiteral("ScaleType"),
                                 typeCombo->itemText(idx));
            updateScaleVisibility();
        });
}

// ---------------------------------------------------------------------------
// buildCombineForm — operation combo, keepToolBody
// ---------------------------------------------------------------------------
void PropertiesPanel::buildCombineForm(const QString& featureId,
                                       const features::CombineFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("CombineFeature"));

    // --- Operation ---
    auto* opCombo = new QComboBox();
    opCombo->setObjectName("Operation");
    opCombo->addItems({tr("Join"), tr("Cut"), tr("Intersect")});
    opCombo->setCurrentIndex(static_cast<int>(p.operation));
    m_formLayout->addRow(tr("Operation"), opCombo);

    connect(opCombo, &QComboBox::currentIndexChanged, this,
        [this, featureId, opCombo](int idx) {
            schedulePropertyChanged(featureId, QStringLiteral("Operation"),
                                 opCombo->itemText(idx));
        });

    // --- Keep Tool Body ---
    auto* keepCheck = new QCheckBox();
    keepCheck->setObjectName("KeepToolBody");
    keepCheck->setChecked(p.keepToolBody);
    m_formLayout->addRow(tr("Keep Tool Body"), keepCheck);

    connect(keepCheck, &QCheckBox::stateChanged, this,
        [this, featureId](int state) {
            schedulePropertyChanged(featureId, QStringLiteral("KeepToolBody"),
                                 state == Qt::Checked);
        });
}

// ---------------------------------------------------------------------------
// buildSplitBodyForm — plane origin/normal
// ---------------------------------------------------------------------------
void PropertiesPanel::buildSplitBodyForm(const QString& featureId,
                                          const features::SplitBodyFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("SplitBodyFeature"));

    // --- Plane Origin X ---
    auto* oxSpin = new QDoubleSpinBox();
    oxSpin->setObjectName("PlaneOx");
    oxSpin->setRange(-1e9, 1e9);
    oxSpin->setDecimals(4);
    oxSpin->setSuffix(" mm");
    oxSpin->setValue(p.planeOx);
    m_formLayout->addRow(tr("Plane Origin X"), oxSpin);

    connect(oxSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("PlaneOx"), v);
        });

    // --- Plane Origin Y ---
    auto* oySpin = new QDoubleSpinBox();
    oySpin->setObjectName("PlaneOy");
    oySpin->setRange(-1e9, 1e9);
    oySpin->setDecimals(4);
    oySpin->setSuffix(" mm");
    oySpin->setValue(p.planeOy);
    m_formLayout->addRow(tr("Plane Origin Y"), oySpin);

    connect(oySpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("PlaneOy"), v);
        });

    // --- Plane Origin Z ---
    auto* ozSpin = new QDoubleSpinBox();
    ozSpin->setObjectName("PlaneOz");
    ozSpin->setRange(-1e9, 1e9);
    ozSpin->setDecimals(4);
    ozSpin->setSuffix(" mm");
    ozSpin->setValue(p.planeOz);
    m_formLayout->addRow(tr("Plane Origin Z"), ozSpin);

    connect(ozSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("PlaneOz"), v);
        });

    // --- Plane Normal X ---
    auto* nxSpin = new QDoubleSpinBox();
    nxSpin->setObjectName("PlaneNx");
    nxSpin->setRange(-1.0, 1.0);
    nxSpin->setDecimals(6);
    nxSpin->setSingleStep(0.1);
    nxSpin->setValue(p.planeNx);
    m_formLayout->addRow(tr("Plane Normal X"), nxSpin);

    connect(nxSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("PlaneNx"), v);
        });

    // --- Plane Normal Y ---
    auto* nySpin = new QDoubleSpinBox();
    nySpin->setObjectName("PlaneNy");
    nySpin->setRange(-1.0, 1.0);
    nySpin->setDecimals(6);
    nySpin->setSingleStep(0.1);
    nySpin->setValue(p.planeNy);
    m_formLayout->addRow(tr("Plane Normal Y"), nySpin);

    connect(nySpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("PlaneNy"), v);
        });

    // --- Plane Normal Z ---
    auto* nzSpin = new QDoubleSpinBox();
    nzSpin->setObjectName("PlaneNz");
    nzSpin->setRange(-1.0, 1.0);
    nzSpin->setDecimals(6);
    nzSpin->setSingleStep(0.1);
    nzSpin->setValue(p.planeNz);
    m_formLayout->addRow(tr("Plane Normal Z"), nzSpin);

    connect(nzSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("PlaneNz"), v);
        });
}

// ---------------------------------------------------------------------------
// buildOffsetFacesForm — distance
// ---------------------------------------------------------------------------
void PropertiesPanel::buildOffsetFacesForm(const QString& featureId,
                                            const features::OffsetFacesFeature* feat)
{
    const auto& p = feat->params();
    setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("OffsetFacesFeature"));

    // --- Distance ---
    auto* distSpin = new QDoubleSpinBox();
    distSpin->setObjectName("Distance");
    distSpin->setRange(-100000.0, 100000.0);
    distSpin->setDecimals(4);
    distSpin->setSuffix(" mm");
    distSpin->setValue(p.distance);
    m_formLayout->addRow(tr("Distance"), distSpin);

    connect(distSpin, &QDoubleSpinBox::valueChanged, this,
        [this, featureId](double v) {
            schedulePropertyChanged(featureId, QStringLiteral("Distance"), v);
        });

    // --- Face count (read-only) ---
    auto* faceLbl = new QLabel(QString::number(static_cast<int>(p.faceIndices.size())));
    m_formLayout->addRow(tr("Faces"), faceLbl);
}

// ---------------------------------------------------------------------------
// buildGenericForm — fallback for unknown feature types
// ---------------------------------------------------------------------------
void PropertiesPanel::buildGenericForm(const QString& featureId,
                                       const features::Feature* feat)
{
    setHeaderText(QString::fromStdString(feat->name()),
                  QStringLiteral("Feature"));

    m_formLayout->addRow(tr("ID"), new QLabel(featureId));
    m_formLayout->addRow(tr("Type"),
        new QLabel(QString::number(static_cast<int>(feat->type()))));
}

// ---------------------------------------------------------------------------
// setProperties — generic property builder from tuples
// ---------------------------------------------------------------------------
void PropertiesPanel::setProperties(
    const std::vector<std::tuple<QString, QString, QVariant>>& props)
{
    clearFormWidgets();

    for (const auto& [label, type, value] : props) {

        if (type == QLatin1String("double")) {
            auto* spin = new QDoubleSpinBox();
            spin->setObjectName(label);
            spin->setRange(-1e9, 1e9);
            spin->setDecimals(4);
            spin->setValue(value.toDouble());
            m_formLayout->addRow(label, spin);

            connect(spin, &QDoubleSpinBox::valueChanged, this,
                [this, label](double v) {
                    schedulePropertyChanged(m_currentFeatureId, label, v);
                });

        } else if (type == QLatin1String("int")) {
            auto* spin = new QSpinBox();
            spin->setObjectName(label);
            spin->setRange(-1000000, 1000000);
            spin->setValue(value.toInt());
            m_formLayout->addRow(label, spin);

            connect(spin, &QSpinBox::valueChanged, this,
                [this, label](int v) {
                    schedulePropertyChanged(m_currentFeatureId, label, v);
                });

        } else if (type == QLatin1String("bool")) {
            auto* check = new QCheckBox();
            check->setObjectName(label);
            check->setChecked(value.toBool());
            m_formLayout->addRow(label, check);

            connect(check, &QCheckBox::stateChanged, this,
                [this, label](int state) {
                    schedulePropertyChanged(m_currentFeatureId, label,
                                         state == Qt::Checked);
                });

        } else if (type == QLatin1String("enum")) {
            auto* combo = new QComboBox();
            combo->setObjectName(label);
            combo->addItems(value.toStringList());
            m_formLayout->addRow(label, combo);

            connect(combo, &QComboBox::currentIndexChanged, this,
                [this, label, combo](int idx) {
                    schedulePropertyChanged(m_currentFeatureId, label,
                                         combo->itemText(idx));
                });

        } else if (type == QLatin1String("string")) {
            auto* edit = new QLineEdit();
            edit->setObjectName(label);
            edit->setText(value.toString());
            m_formLayout->addRow(label, edit);

            connect(edit, &QLineEdit::textChanged, this,
                [this, label](const QString& text) {
                    schedulePropertyChanged(m_currentFeatureId, label, text);
                });

        } else if (type == QLatin1String("label")) {
            // Section header / separator
            auto* headerLbl = new QLabel(label);
            headerLbl->setObjectName(label);
            headerLbl->setStyleSheet(QStringLiteral(
                "color: #e0e0e0; font-weight: bold; padding-top: 8px;"));
            m_formLayout->addRow(headerLbl);

        } else {
            // Unknown type — show as read-only label
            auto* lbl = new QLabel(value.toString());
            lbl->setObjectName(label);
            m_formLayout->addRow(label, lbl);
        }
    }
}

// ---------------------------------------------------------------------------
// setCurrentEnumIndex — select a specific item in an enum combo
// ---------------------------------------------------------------------------
void PropertiesPanel::setCurrentEnumIndex(const QString& propertyName, int index)
{
    auto* combo = m_formContainer->findChild<QComboBox*>(propertyName);
    if (combo && index >= 0 && index < combo->count())
        combo->setCurrentIndex(index);
}

// ---------------------------------------------------------------------------
// schedulePropertyChanged — debounced emission of propertyChanged
// ---------------------------------------------------------------------------
void PropertiesPanel::schedulePropertyChanged(const QString& featureId,
                                              const QString& propertyName,
                                              const QVariant& newValue)
{
    m_pendingFeatureId    = featureId;
    m_pendingPropertyName = propertyName;
    m_pendingValue        = newValue;
    m_debounceTimer.start(); // (re)starts the 50ms timer
}

// ---------------------------------------------------------------------------
// keyPressEvent — commit on Enter/Return, cancel on Escape
// ---------------------------------------------------------------------------
void PropertiesPanel::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        // Flush any pending debounced change immediately
        if (m_debounceTimer.isActive()) {
            m_debounceTimer.stop();
            if (!m_pendingFeatureId.isEmpty())
                emit propertyChanged(m_pendingFeatureId, m_pendingPropertyName, m_pendingValue);
        }
        if (!m_currentFeatureId.isEmpty())
            emit editingCommitted(m_currentFeatureId);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        // Cancel: discard any pending change
        m_debounceTimer.stop();
        if (!m_currentFeatureId.isEmpty())
            emit editingCancelled(m_currentFeatureId);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

// ---------------------------------------------------------------------------
// eventFilter — detect focus leaving the panel as an implicit commit
// ---------------------------------------------------------------------------
bool PropertiesPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::FocusOut) {
        // Check if the new focus target is outside this panel
        QWidget* focusWidget = QApplication::focusWidget();
        if (focusWidget && !isAncestorOf(focusWidget) && focusWidget != this) {
            // Flush any pending debounced change
            if (m_debounceTimer.isActive()) {
                m_debounceTimer.stop();
                if (!m_pendingFeatureId.isEmpty())
                    emit propertyChanged(m_pendingFeatureId, m_pendingPropertyName, m_pendingValue);
            }
            if (!m_currentFeatureId.isEmpty())
                emit editingCommitted(m_currentFeatureId);
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ---------------------------------------------------------------------------
// addMaterialDropdown — add a material combo box to the form
// ---------------------------------------------------------------------------
void PropertiesPanel::addMaterialDropdown(const QString& bodyId,
                                          const QString& currentMaterialName)
{
    if (!m_formLayout)
        return;

    auto* combo = new QComboBox();
    combo->setObjectName(QStringLiteral("MaterialCombo"));

    const auto& allMats = kernel::MaterialLibrary::all();
    int currentIdx = -1;
    for (size_t i = 0; i < allMats.size(); ++i) {
        QString name = QString::fromStdString(allMats[i].name);
        combo->addItem(name);
        if (name == currentMaterialName)
            currentIdx = static_cast<int>(i);
    }
    // Add "Default" entry if no material matches
    if (currentIdx < 0) {
        combo->insertItem(0, QStringLiteral("Default"));
        combo->setCurrentIndex(0);
    } else {
        combo->setCurrentIndex(currentIdx);
    }

    m_formLayout->insertRow(0, tr("Material"), combo);

    connect(combo, &QComboBox::currentTextChanged, this,
            [this, bodyId](const QString& text) {
        emit materialChanged(bodyId, text);
    });
}
