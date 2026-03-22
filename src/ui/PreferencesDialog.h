#pragma once
#include <QDialog>

class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QTabWidget;

class PreferencesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget* parent = nullptr);

private:
    void setupUI();
    void loadSettings();
    void saveSettings();

    QTabWidget* m_tabs = nullptr;

    // General
    QComboBox*  m_unitSystem        = nullptr;  // mm, cm, inch
    QSpinBox*   m_gridSize          = nullptr;  // grid snap size in current units
    QCheckBox*  m_gridSnap          = nullptr;  // enable/disable grid snap
    QSpinBox*   m_autoSaveInterval  = nullptr;  // minutes (0 = disabled)

    // Display
    QComboBox*  m_defaultViewMode   = nullptr;  // Solid+Edges, Solid, Wireframe
    QCheckBox*  m_showOrigin        = nullptr;
    QCheckBox*  m_showGrid          = nullptr;
    QSpinBox*   m_msaaSamples       = nullptr;  // 0, 2, 4, 8

    // Sketch
    QDoubleSpinBox* m_snapTolerance       = nullptr;  // mm
    QDoubleSpinBox* m_autoConstraintAngle = nullptr;  // degrees
    QCheckBox*      m_autoConstrainOnDraw = nullptr;
};
