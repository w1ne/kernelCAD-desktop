#pragma once
#include <QWidget>
#include <QVariant>
#include <QString>
#include <QTimer>
#include <QPoint>
#include "../features/ExtrudeFeature.h"
#include "../features/FilletFeature.h"
#include "../features/ChamferFeature.h"
#include "../features/ShellFeature.h"
#include "../features/RevolveFeature.h"
#include "../features/OffsetFacesFeature.h"
#include "../features/ConstructionPlane.h"

class QFormLayout;
class QLabel;
class QVBoxLayout;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;

/// Floating non-modal command dialog that appears over the viewport
/// when the user initiates a feature creation command (Extrude, Fillet, etc.).
/// Styled as a dark panel positioned at the top-right of its parent widget.
class FeatureDialog : public QWidget
{
    Q_OBJECT
public:
    explicit FeatureDialog(QWidget* parent = nullptr);

    /// Show dialog configured for Extrude with given default params.
    void showExtrude(const features::ExtrudeParams& defaults);

    /// Show dialog configured for Fillet with given default params.
    void showFillet(const features::FilletParams& defaults);

    /// Show dialog configured for Chamfer with given default params.
    void showChamfer(const features::ChamferParams& defaults);

    /// Show dialog configured for Shell with given default params.
    void showShell(const features::ShellParams& defaults);

    /// Show dialog configured for Revolve with given default params.
    void showRevolve(const features::RevolveParams& defaults);

    /// Show dialog configured for Press/Pull (offset faces) with given default params.
    void showPressPull(const features::OffsetFacesParams& defaults);

    /// Show dialog configured for Construction Plane with given default params.
    void showConstructionPlane(const features::ConstructionPlaneParams& defaults);

    /// Hide and clear all form content.
    void dismiss();

    /// Returns true if the dialog is currently visible and active.
    bool isActive() const;

    /// Forward a key press (digit, period, minus) to the first QDoubleSpinBox.
    void forwardKeyToDistance(QKeyEvent* event);

    /// Reposition the dialog at the top-right of the parent widget.
    void repositionOverParent();

signals:
    void accepted();
    void cancelled();

    /// Emitted whenever a form field value changes, for live preview.
    void parameterChanged(const QString& name, const QVariant& value);

    /// Typed result signals emitted on OK.
    void extrudeAccepted(features::ExtrudeParams params);
    void filletAccepted(features::FilletParams params);
    void chamferAccepted(features::ChamferParams params);
    void shellAccepted(features::ShellParams params);
    void revolveAccepted(features::RevolveParams params);
    void pressPullAccepted(features::OffsetFacesParams params);
    void constructionPlaneAccepted(features::ConstructionPlaneParams params);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    enum class Mode { None, Extrude, Fillet, Chamfer, Shell, Revolve, PressPull, ConstructionPlane };

    void clearForm();
    void setupLayout();
    void showAtPosition();
    void applyStyleSheet();

    /// Build the form fields for each feature type.
    void buildExtrudeForm(const features::ExtrudeParams& defaults);
    void buildFilletForm(const features::FilletParams& defaults);
    void buildChamferForm(const features::ChamferParams& defaults);
    void buildShellForm(const features::ShellParams& defaults);
    void buildRevolveForm(const features::RevolveParams& defaults);
    void buildPressPullForm(const features::OffsetFacesParams& defaults);
    void buildConstructionPlaneForm(const features::ConstructionPlaneParams& defaults);

    /// Collect current form values into the typed params and emit the accepted signal.
    void emitAccepted();

    Mode           m_mode = Mode::None;
    QVBoxLayout*   m_rootLayout  = nullptr;
    QLabel*        m_titleLabel  = nullptr;
    QWidget*       m_formWidget  = nullptr;
    QFormLayout*   m_formLayout  = nullptr;
    QPushButton*   m_okButton    = nullptr;
    QPushButton*   m_cancelButton = nullptr;

    // Form field widgets (populated per-mode)
    // Extrude
    QLabel*         m_profileLabel      = nullptr;
    QComboBox*      m_directionCombo    = nullptr;
    QComboBox*      m_extentTypeCombo   = nullptr;
    QDoubleSpinBox* m_distanceSpin      = nullptr;
    QDoubleSpinBox* m_distance2Spin     = nullptr;
    QLabel*         m_distance2Label    = nullptr;
    QDoubleSpinBox* m_taperAngleSpin    = nullptr;
    QComboBox*      m_operationCombo    = nullptr;

    // Fillet
    QDoubleSpinBox* m_radiusSpin        = nullptr;
    QCheckBox*      m_tangentChainCheck = nullptr;
    QLabel*         m_edgeCountLabel    = nullptr;

    // Chamfer
    QDoubleSpinBox* m_chamferDistSpin   = nullptr;
    QComboBox*      m_chamferTypeCombo  = nullptr;
    QLabel*         m_chamferEdgeLabel  = nullptr;

    // Shell
    QDoubleSpinBox* m_thicknessSpin     = nullptr;
    QComboBox*      m_shellDirCombo     = nullptr;
    QLabel*         m_faceCountLabel    = nullptr;

    // Revolve
    QLabel*         m_revolveProfileLabel = nullptr;
    QComboBox*      m_axisCombo          = nullptr;
    QDoubleSpinBox* m_angleSpin          = nullptr;
    QCheckBox*      m_fullRevCheck       = nullptr;
    QComboBox*      m_revolveOpCombo     = nullptr;

    // PressPull
    QDoubleSpinBox* m_ppDistanceSpin     = nullptr;
    QLabel*         m_ppFaceCountLabel   = nullptr;

    // Construction Plane
    QComboBox*      m_cpTypeCombo        = nullptr;
    QComboBox*      m_cpRefCombo         = nullptr;
    QDoubleSpinBox* m_cpOffsetSpin       = nullptr;
    QDoubleSpinBox* m_cpAngleSpin        = nullptr;

    // Stored defaults for building the result
    features::ExtrudeParams  m_extrudeDefaults;
    features::FilletParams   m_filletDefaults;
    features::ChamferParams  m_chamferDefaults;
    features::ShellParams    m_shellDefaults;
    features::RevolveParams  m_revolveDefaults;
    features::OffsetFacesParams m_pressPullDefaults;
    features::ConstructionPlaneParams m_cpDefaults;

    // Drag support
    bool m_dragging = false;
    QPoint m_dragOffset;
};
