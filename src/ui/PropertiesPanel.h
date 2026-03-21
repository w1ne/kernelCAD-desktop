#pragma once
#include <QWidget>
#include <QString>
#include <QVariant>
#include <QTimer>
#include <vector>
#include <tuple>

class QFormLayout;
class QLabel;
class QVBoxLayout;
class QScrollArea;

namespace document { class Document; }
namespace features {
    class Feature;
    class ExtrudeFeature;
    class RevolveFeature;
    class FilletFeature;
    class ChamferFeature;
    class SketchFeature;
    class RectangularPatternFeature;
    class ShellFeature;
    class SweepFeature;
    class LoftFeature;
    class HoleFeature;
    class MirrorFeature;
    class CircularPatternFeature;
    class MoveFeature;
    class DraftFeature;
    class ThickenFeature;
    class ThreadFeature;
    class ScaleFeature;
    class CombineFeature;
    class SplitBodyFeature;
    class OffsetFacesFeature;
}

class PropertiesPanel : public QWidget
{
    Q_OBJECT
public:
    explicit PropertiesPanel(QWidget* parent = nullptr);

    /// Set the document pointer so showFeature() can look up actual feature params.
    void setDocument(document::Document* doc);

    /// Display properties for the given feature ID.
    /// Looks up the feature in the document timeline and builds a form
    /// populated with actual parameter values.
    void showFeature(const QString& featureId);

    /// Generic property setter: each tuple is (label, type, value).
    /// Supported types: "double", "int", "bool", "enum", "string".
    /// For "enum", value is a QStringList of choices; currentIndex controls
    /// which item is selected (passed via setCurrentEnumIndex after calling this).
    void setProperties(const std::vector<std::tuple<QString, QString, QVariant>>& props);

    /// Set current selection for an enum property previously added via setProperties.
    void setCurrentEnumIndex(const QString& propertyName, int index);

    /// Add a material dropdown for the given body at the top of the property form.
    /// When the user selects a different material, materialChanged is emitted.
    void addMaterialDropdown(const QString& bodyId, const QString& currentMaterialName);

    /// Remove all property widgets and reset header.
    void clear();

    /// Returns the feature ID currently being displayed (empty if none).
    QString currentFeatureId() const { return m_currentFeatureId; }

    /// Enable or disable edit mode. When active, the header shows an
    /// "Editing:" prefix to indicate that parameter changes will be applied
    /// to an existing feature rather than creating something new.
    void setEditMode(bool editing);

signals:
    /// Emitted whenever the user edits any property value (debounced for sliders/spinboxes).
    void propertyChanged(const QString& featureId, const QString& propertyName, const QVariant& newValue);

    /// Emitted when the user commits edits (Enter pressed or focus leaves the panel).
    void editingCommitted(const QString& featureId);

    /// Emitted when the user cancels edits (Escape pressed).
    void editingCancelled(const QString& featureId);

    /// Emitted when the user selects a different material from the dropdown.
    void materialChanged(const QString& bodyId, const QString& materialName);

private:
    void buildExtrudeForm(const QString& featureId, const features::ExtrudeFeature* feat);
    void buildRevolveForm(const QString& featureId, const features::RevolveFeature* feat);
    void buildFilletForm(const QString& featureId, const features::FilletFeature* feat);
    void buildChamferForm(const QString& featureId, const features::ChamferFeature* feat);
    void buildSketchForm(const QString& featureId, const features::SketchFeature* feat);
    void buildRectangularPatternForm(const QString& featureId, const features::RectangularPatternFeature* feat);
    void buildShellForm(const QString& featureId, const features::ShellFeature* feat);
    void buildSweepForm(const QString& featureId, const features::SweepFeature* feat);
    void buildLoftForm(const QString& featureId, const features::LoftFeature* feat);
    void buildHoleForm(const QString& featureId, const features::HoleFeature* feat);
    void buildMirrorForm(const QString& featureId, const features::MirrorFeature* feat);
    void buildCircularPatternForm(const QString& featureId, const features::CircularPatternFeature* feat);
    void buildMoveForm(const QString& featureId, const features::MoveFeature* feat);
    void buildDraftForm(const QString& featureId, const features::DraftFeature* feat);
    void buildThickenForm(const QString& featureId, const features::ThickenFeature* feat);
    void buildThreadForm(const QString& featureId, const features::ThreadFeature* feat);
    void buildScaleForm(const QString& featureId, const features::ScaleFeature* feat);
    void buildCombineForm(const QString& featureId, const features::CombineFeature* feat);
    void buildSplitBodyForm(const QString& featureId, const features::SplitBodyFeature* feat);
    void buildOffsetFacesForm(const QString& featureId, const features::OffsetFacesFeature* feat);
    void buildGenericForm(const QString& featureId, const features::Feature* feat);
    void clearFormWidgets();
    void setHeaderText(const QString& featureName, const QString& featureType);

    /// Parse a numeric value from an expression string like "10 mm" or "360 deg".
    static double parseExprValue(const std::string& expr);

    /// Handle key presses for commit (Enter/Return) and cancel (Escape).
    void keyPressEvent(QKeyEvent* event) override;

    /// Handle focus-out as implicit commit.
    bool eventFilter(QObject* watched, QEvent* event) override;

    /// Schedule a debounced propertyChanged emission.
    void schedulePropertyChanged(const QString& featureId, const QString& propertyName,
                                 const QVariant& newValue);

    document::Document* m_document = nullptr;
    QString        m_currentFeatureId;
    bool           m_editMode = false;
    QVBoxLayout*   m_rootLayout   = nullptr;
    QLabel*        m_headerLabel  = nullptr;
    QScrollArea*   m_scrollArea   = nullptr;
    QWidget*       m_formContainer = nullptr;
    QFormLayout*   m_formLayout   = nullptr;

    // Debounce timer for preview updates during slider/spinbox dragging
    QTimer         m_debounceTimer;
    QString        m_pendingFeatureId;
    QString        m_pendingPropertyName;
    QVariant       m_pendingValue;
};
