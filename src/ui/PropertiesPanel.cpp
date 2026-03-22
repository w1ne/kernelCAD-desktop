#include "PropertiesPanel.h"
#include "PropertyFormFactory.h"

#include "../document/Document.h"
#include "../kernel/OCCTKernel.h"
#include "../kernel/BRepModel.h"
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
#include "../sketch/Sketch.h"
#include "SketchEditor.h"

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
        background-color: #1e1e1e;
    }
    QLabel#headerLabel {
        color: #d4d4d4;
        font-weight: 600;
        font-size: 13px;
        padding: 8px 8px 4px 8px;
    }
    QLabel {
        color: #bbb;
        font-size: 12px;
    }
    QDoubleSpinBox, QSpinBox, QComboBox, QLineEdit {
        background-color: #2a2a2a;
        color: #d4d4d4;
        border: 1px solid #3a3a3a;
        border-radius: 3px;
        padding: 3px 6px;
        min-height: 24px;
    }
    QDoubleSpinBox:focus, QSpinBox:focus, QComboBox:focus, QLineEdit:focus {
        border: 1px solid #0078d4;
    }
    QCheckBox {
        color: #bbb;
        spacing: 6px;
    }
    QCheckBox::indicator {
        width: 16px; height: 16px;
        border: 1px solid #3a3a3a;
        border-radius: 2px;
        background-color: #2a2a2a;
    }
    QCheckBox::indicator:checked {
        background-color: #0078d4;
        border-color: #0078d4;
    }
    QFrame#separator {
        color: #3a3a3a;
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
    m_scrollArea->setStyleSheet("QScrollArea { background-color: #1e1e1e; }");

    m_formContainer = new QWidget();
    m_formContainer->setStyleSheet("background-color: #1e1e1e;");
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
// showSketchPalettes — show toggles, stats and Finish button during sketch editing
// ---------------------------------------------------------------------------
void PropertiesPanel::showSketchPalettes(sketch::Sketch* sketch, SketchEditor* editor)
{
    m_paletteSketch = sketch;
    m_paletteEditor = editor;
    m_currentFeatureId.clear();
    clearFormWidgets();

    // Header
    m_headerLabel->setText(
        QStringLiteral("<b style='color:#e0e0e0;'>SKETCH PALETTES</b>"));

    // ── Options section ──────────────────────────────────────────────────
    auto* optLabel = new QLabel(QStringLiteral(
        "<span style='color:#aaa; font-weight:bold;'>Options</span>"));
    m_formLayout->addRow(optLabel);

    auto* snapCb = new QCheckBox(tr("Snap to Grid"));
    snapCb->setChecked(editor->gridSnapEnabled());
    connect(snapCb, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_paletteEditor)
            m_paletteEditor->setGridSnap(checked);
    });
    m_formLayout->addRow(snapCb);

    auto* gridSpin = new QDoubleSpinBox();
    gridSpin->setRange(0.1, 1000.0);
    gridSpin->setDecimals(1);
    gridSpin->setSuffix(QStringLiteral(" mm"));
    gridSpin->setValue(5.0);
    connect(gridSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double val) {
        emit propertyChanged(QString(), QStringLiteral("Grid Size"), val);
    });
    m_formLayout->addRow(tr("Grid Size"), gridSpin);

    auto* inferCb = new QCheckBox(tr("Show Inference Lines"));
    inferCb->setChecked(true);
    connect(inferCb, &QCheckBox::toggled, this, [this](bool checked) {
        emit propertyChanged(QString(), QStringLiteral("Show Inference Lines"), checked);
    });
    m_formLayout->addRow(inferCb);

    // Separator
    auto* sep1 = new QFrame();
    sep1->setObjectName("separator");
    sep1->setFrameShape(QFrame::HLine);
    sep1->setFrameShadow(QFrame::Sunken);
    m_formLayout->addRow(sep1);

    // ── Display section ──────────────────────────────────────────────────
    auto* dispLabel = new QLabel(QStringLiteral(
        "<span style='color:#aaa; font-weight:bold;'>Display</span>"));
    m_formLayout->addRow(dispLabel);

    auto* ptsCb = new QCheckBox(tr("Show Points"));
    ptsCb->setChecked(true);
    connect(ptsCb, &QCheckBox::toggled, this, [this](bool checked) {
        emit propertyChanged(QString(), QStringLiteral("Show Points"), checked);
    });
    m_formLayout->addRow(ptsCb);

    auto* dimCb = new QCheckBox(tr("Show Dimensions"));
    dimCb->setChecked(true);
    connect(dimCb, &QCheckBox::toggled, this, [this](bool checked) {
        emit propertyChanged(QString(), QStringLiteral("Show Dimensions"), checked);
    });
    m_formLayout->addRow(dimCb);

    auto* conCb = new QCheckBox(tr("Show Constraints"));
    conCb->setChecked(true);
    connect(conCb, &QCheckBox::toggled, this, [this](bool checked) {
        emit propertyChanged(QString(), QStringLiteral("Show Constraints"), checked);
    });
    m_formLayout->addRow(conCb);

    auto* cstrCb = new QCheckBox(tr("Show Construction"));
    cstrCb->setChecked(true);
    connect(cstrCb, &QCheckBox::toggled, this, [this](bool checked) {
        emit propertyChanged(QString(), QStringLiteral("Show Construction"), checked);
    });
    m_formLayout->addRow(cstrCb);

    // Separator
    auto* sep2 = new QFrame();
    sep2->setObjectName("separator");
    sep2->setFrameShape(QFrame::HLine);
    sep2->setFrameShadow(QFrame::Sunken);
    m_formLayout->addRow(sep2);

    // ── Sketch Info section ──────────────────────────────────────────────
    auto* infoLabel = new QLabel(QStringLiteral(
        "<span style='color:#aaa; font-weight:bold;'>Sketch Info</span>"));
    m_formLayout->addRow(infoLabel);

    m_statsPoints      = new QLabel();
    m_statsLines       = new QLabel();
    m_statsCircles     = new QLabel();
    m_statsConstraints = new QLabel();
    m_statsDOF         = new QLabel();

    m_formLayout->addRow(tr("Points"),      m_statsPoints);
    m_formLayout->addRow(tr("Lines"),       m_statsLines);
    m_formLayout->addRow(tr("Circles"),     m_statsCircles);
    m_formLayout->addRow(tr("Constraints"), m_statsConstraints);
    m_formLayout->addRow(tr("DOF"),         m_statsDOF);

    refreshSketchStats();

    // Separator
    auto* sep3 = new QFrame();
    sep3->setObjectName("separator");
    sep3->setFrameShape(QFrame::HLine);
    sep3->setFrameShadow(QFrame::Sunken);
    m_formLayout->addRow(sep3);

    // ── Finish Sketch button ─────────────────────────────────────────────
    auto* finishBtn = new QPushButton(tr("Finish Sketch"));
    finishBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #5294e2; color: #fff; border: none;"
        "  border-radius: 4px; padding: 6px 16px; font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #6aa5ef; }"
        "QPushButton:pressed { background-color: #3a7bd5; }");
    connect(finishBtn, &QPushButton::clicked, this, [this]() {
        emit finishSketchClicked();
    });
    m_formLayout->addRow(finishBtn);
}

// ---------------------------------------------------------------------------
// refreshSketchStats — update entity counts and DOF in the sketch palette
// ---------------------------------------------------------------------------
void PropertiesPanel::refreshSketchStats()
{
    if (!m_paletteSketch || !m_statsPoints)
        return;

    const auto& sk = *m_paletteSketch;
    m_statsPoints->setText(QString::number(static_cast<int>(sk.points().size())));
    m_statsLines->setText(QString::number(static_cast<int>(sk.lines().size())));
    m_statsCircles->setText(QString::number(
        static_cast<int>(sk.circles().size()) + static_cast<int>(sk.arcs().size())));
    m_statsConstraints->setText(QString::number(static_cast<int>(sk.constraints().size())));

    int dof = sk.freeDOF();
    if (dof == 0) {
        m_statsDOF->setText(QStringLiteral("0 (Fully constrained)"));
        m_statsDOF->setStyleSheet("color: #4caf50;");
    } else {
        m_statsDOF->setText(QString::number(dof));
        m_statsDOF->setStyleSheet("color: #ff9800;");
    }
}

// ---------------------------------------------------------------------------
// clear()
// ---------------------------------------------------------------------------
void PropertiesPanel::clear()
{
    m_currentFeatureId.clear();
    m_paletteSketch = nullptr;
    m_paletteEditor = nullptr;
    m_statsPoints = nullptr;
    m_statsLines = nullptr;
    m_statsCircles = nullptr;
    m_statsConstraints = nullptr;
    m_statsDOF = nullptr;
    m_headerLabel->setText(
        QStringLiteral("<div style='text-align:center; padding-top:30px;'>"
                       "<div style='color:#888; font-size:13px;'>Select a feature, body, or face</div>"
                       "<div style='color:#666; font-size:11px; padding-top:4px;'>to view and edit its properties</div>"
                       "</div>"));
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

    // Create a change callback that emits the propertyChanged signal
    auto onChange = [this, featureId](const QString& propertyName, const QVariant& newValue) {
        schedulePropertyChanged(featureId, propertyName, newValue);
    };

    // Set header and delegate form building to PropertyFormFactory
    auto setHeader = [&](const QString& typeName) {
        setHeaderText(QString::fromStdString(feat->name()), typeName);
    };

    using FT = features::FeatureType;
    switch (feat->type()) {
    case FT::Extrude:
        setHeader(QStringLiteral("ExtrudeFeature"));
        PropertyFormFactory::buildExtrudeForm(m_formLayout, static_cast<const features::ExtrudeFeature*>(feat), onChange);
        break;
    case FT::Revolve:
        setHeader(QStringLiteral("RevolveFeature"));
        PropertyFormFactory::buildRevolveForm(m_formLayout, static_cast<const features::RevolveFeature*>(feat), onChange);
        break;
    case FT::Fillet:
        setHeader(QStringLiteral("FilletFeature"));
        PropertyFormFactory::buildFilletForm(m_formLayout, static_cast<const features::FilletFeature*>(feat), onChange);
        break;
    case FT::Chamfer:
        setHeader(QStringLiteral("ChamferFeature"));
        PropertyFormFactory::buildChamferForm(m_formLayout, static_cast<const features::ChamferFeature*>(feat), onChange);
        break;
    case FT::Sketch:
        setHeader(QStringLiteral("SketchFeature"));
        PropertyFormFactory::buildSketchForm(m_formLayout, static_cast<const features::SketchFeature*>(feat));
        break;
    case FT::RectangularPattern:
        setHeader(QStringLiteral("RectangularPatternFeature"));
        PropertyFormFactory::buildRectangularPatternForm(m_formLayout, static_cast<const features::RectangularPatternFeature*>(feat));
        break;
    case FT::Shell:
        setHeader(QStringLiteral("ShellFeature"));
        PropertyFormFactory::buildShellForm(m_formLayout, static_cast<const features::ShellFeature*>(feat), onChange);
        break;
    case FT::Sweep:
        setHeader(QStringLiteral("SweepFeature"));
        PropertyFormFactory::buildSweepForm(m_formLayout, static_cast<const features::SweepFeature*>(feat), onChange);
        break;
    case FT::Loft:
        setHeader(QStringLiteral("LoftFeature"));
        PropertyFormFactory::buildLoftForm(m_formLayout, static_cast<const features::LoftFeature*>(feat), onChange);
        break;
    case FT::Hole:
        setHeader(QStringLiteral("HoleFeature"));
        PropertyFormFactory::buildHoleForm(m_formLayout, static_cast<const features::HoleFeature*>(feat), onChange);
        break;
    case FT::Mirror:
        setHeader(QStringLiteral("MirrorFeature"));
        PropertyFormFactory::buildMirrorForm(m_formLayout, static_cast<const features::MirrorFeature*>(feat), onChange);
        break;
    case FT::CircularPattern:
        setHeader(QStringLiteral("CircularPatternFeature"));
        PropertyFormFactory::buildCircularPatternForm(m_formLayout, static_cast<const features::CircularPatternFeature*>(feat), onChange);
        break;
    case FT::Move:
        setHeader(QStringLiteral("MoveFeature"));
        PropertyFormFactory::buildMoveForm(m_formLayout, static_cast<const features::MoveFeature*>(feat), onChange);
        break;
    case FT::Draft:
        setHeader(QStringLiteral("DraftFeature"));
        PropertyFormFactory::buildDraftForm(m_formLayout, static_cast<const features::DraftFeature*>(feat), onChange);
        break;
    case FT::Thicken:
        setHeader(QStringLiteral("ThickenFeature"));
        PropertyFormFactory::buildThickenForm(m_formLayout, static_cast<const features::ThickenFeature*>(feat), onChange);
        break;
    case FT::Thread:
        setHeader(QStringLiteral("ThreadFeature"));
        PropertyFormFactory::buildThreadForm(m_formLayout, static_cast<const features::ThreadFeature*>(feat), onChange);
        break;
    case FT::Scale:
        setHeader(QStringLiteral("ScaleFeature"));
        PropertyFormFactory::buildScaleForm(m_formLayout, static_cast<const features::ScaleFeature*>(feat), onChange);
        break;
    case FT::Combine:
        setHeader(QStringLiteral("CombineFeature"));
        PropertyFormFactory::buildCombineForm(m_formLayout, static_cast<const features::CombineFeature*>(feat), onChange);
        break;
    case FT::SplitBody:
        setHeader(QStringLiteral("SplitBodyFeature"));
        PropertyFormFactory::buildSplitBodyForm(m_formLayout, static_cast<const features::SplitBodyFeature*>(feat), onChange);
        break;
    case FT::OffsetFaces:
        setHeader(QStringLiteral("OffsetFacesFeature"));
        PropertyFormFactory::buildOffsetFacesForm(m_formLayout, static_cast<const features::OffsetFacesFeature*>(feat), onChange);
        break;
    default:
        setHeaderText(QString::fromStdString(feat->name()), QStringLiteral("Feature"));
        PropertyFormFactory::buildGenericForm(m_formLayout, featureId, feat);
        break;
    }
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
