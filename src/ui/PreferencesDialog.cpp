#include "PreferencesDialog.h"

#include <QTabWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QSettings>

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    setMinimumSize(420, 380);
    setupUI();
    loadSettings();
}

// ---------------------------------------------------------------------------
void PreferencesDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    m_tabs = new QTabWidget(this);

    // ── General tab ────────────────────────────────────────────────────────
    {
        auto* page = new QWidget;
        auto* form = new QFormLayout(page);
        form->setContentsMargins(12, 12, 12, 12);
        form->setSpacing(8);

        m_unitSystem = new QComboBox;
        m_unitSystem->addItems({tr("mm"), tr("cm"), tr("inch")});
        form->addRow(tr("Unit system:"), m_unitSystem);

        m_gridSize = new QSpinBox;
        m_gridSize->setRange(1, 1000);
        m_gridSize->setSuffix(tr(" units"));
        form->addRow(tr("Grid size:"), m_gridSize);

        m_gridSnap = new QCheckBox(tr("Enable grid snap"));
        form->addRow(QString(), m_gridSnap);

        m_autoSaveInterval = new QSpinBox;
        m_autoSaveInterval->setRange(0, 60);
        m_autoSaveInterval->setSuffix(tr(" min"));
        m_autoSaveInterval->setSpecialValueText(tr("Disabled"));
        form->addRow(tr("Auto-save interval:"), m_autoSaveInterval);

        m_tabs->addTab(page, tr("General"));
    }

    // ── Display tab ────────────────────────────────────────────────────────
    {
        auto* page = new QWidget;
        auto* form = new QFormLayout(page);
        form->setContentsMargins(12, 12, 12, 12);
        form->setSpacing(8);

        m_defaultViewMode = new QComboBox;
        m_defaultViewMode->addItems({tr("Solid + Edges"), tr("Solid"), tr("Wireframe")});
        form->addRow(tr("Default view mode:"), m_defaultViewMode);

        m_showOrigin = new QCheckBox(tr("Show origin"));
        form->addRow(QString(), m_showOrigin);

        m_showGrid = new QCheckBox(tr("Show grid"));
        form->addRow(QString(), m_showGrid);

        m_msaaSamples = new QSpinBox;
        m_msaaSamples->setRange(0, 8);
        m_msaaSamples->setSingleStep(2);
        m_msaaSamples->setSpecialValueText(tr("Off"));
        form->addRow(tr("MSAA samples:"), m_msaaSamples);

        auto* hint = new QLabel(tr("(0, 2, 4, or 8 — requires restart)"));
        hint->setStyleSheet("color: #888; font-size: 10px;");
        form->addRow(QString(), hint);

        m_tabs->addTab(page, tr("Display"));
    }

    // ── Sketch tab ─────────────────────────────────────────────────────────
    {
        auto* page = new QWidget;
        auto* form = new QFormLayout(page);
        form->setContentsMargins(12, 12, 12, 12);
        form->setSpacing(8);

        m_snapTolerance = new QDoubleSpinBox;
        m_snapTolerance->setRange(0.01, 50.0);
        m_snapTolerance->setDecimals(2);
        m_snapTolerance->setSingleStep(0.5);
        m_snapTolerance->setSuffix(tr(" mm"));
        form->addRow(tr("Snap tolerance:"), m_snapTolerance);

        m_autoConstraintAngle = new QDoubleSpinBox;
        m_autoConstraintAngle->setRange(0.5, 45.0);
        m_autoConstraintAngle->setDecimals(1);
        m_autoConstraintAngle->setSingleStep(1.0);
        m_autoConstraintAngle->setSuffix(tr("\u00B0"));
        form->addRow(tr("Auto-constraint angle:"), m_autoConstraintAngle);

        m_autoConstrainOnDraw = new QCheckBox(tr("Auto-constrain while drawing"));
        form->addRow(QString(), m_autoConstrainOnDraw);

        m_tabs->addTab(page, tr("Sketch"));
    }

    mainLayout->addWidget(m_tabs);

    // ── Button box ─────────────────────────────────────────────────────────
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        saveSettings();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

// ---------------------------------------------------------------------------
void PreferencesDialog::loadSettings()
{
    QSettings s;

    // General
    m_unitSystem->setCurrentIndex(s.value("prefs/unitSystem", 0).toInt());
    m_gridSize->setValue(s.value("prefs/gridSize", 10).toInt());
    m_gridSnap->setChecked(s.value("prefs/gridSnap", true).toBool());
    m_autoSaveInterval->setValue(s.value("prefs/autoSaveInterval", 5).toInt());

    // Display
    m_defaultViewMode->setCurrentIndex(s.value("prefs/defaultViewMode", 0).toInt());
    m_showOrigin->setChecked(s.value("prefs/showOrigin", true).toBool());
    m_showGrid->setChecked(s.value("prefs/showGrid", true).toBool());
    m_msaaSamples->setValue(s.value("prefs/msaaSamples", 4).toInt());

    // Sketch
    m_snapTolerance->setValue(s.value("prefs/snapTolerance", 2.0).toDouble());
    m_autoConstraintAngle->setValue(s.value("prefs/autoConstraintAngle", 5.0).toDouble());
    m_autoConstrainOnDraw->setChecked(s.value("prefs/autoConstrainOnDraw", true).toBool());
}

// ---------------------------------------------------------------------------
void PreferencesDialog::saveSettings()
{
    QSettings s;

    // General
    s.setValue("prefs/unitSystem", m_unitSystem->currentIndex());
    s.setValue("prefs/gridSize", m_gridSize->value());
    s.setValue("prefs/gridSnap", m_gridSnap->isChecked());
    s.setValue("prefs/autoSaveInterval", m_autoSaveInterval->value());

    // Display
    s.setValue("prefs/defaultViewMode", m_defaultViewMode->currentIndex());
    s.setValue("prefs/showOrigin", m_showOrigin->isChecked());
    s.setValue("prefs/showGrid", m_showGrid->isChecked());
    s.setValue("prefs/msaaSamples", m_msaaSamples->value());

    // Sketch
    s.setValue("prefs/snapTolerance", m_snapTolerance->value());
    s.setValue("prefs/autoConstraintAngle", m_autoConstraintAngle->value());
    s.setValue("prefs/autoConstrainOnDraw", m_autoConstrainOnDraw->isChecked());
}
