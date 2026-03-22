#pragma once

#include <QFormLayout>
#include <QString>
#include <QVariant>
#include <functional>
#include <string>

namespace document { class Document; }
namespace sketch { class Sketch; }
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

/// Factory that creates property-editing form rows for each feature type.
/// All methods are static; they populate a QFormLayout with the appropriate
/// widgets and connect change signals through a callback.
class PropertyFormFactory {
public:
    /// Callback signature: (propertyName, newValue).
    using ChangeCallback = std::function<void(const QString& propertyName, const QVariant& newValue)>;

    static void buildExtrudeForm(QFormLayout* layout, const features::ExtrudeFeature* feat, ChangeCallback onChange);
    static void buildRevolveForm(QFormLayout* layout, const features::RevolveFeature* feat, ChangeCallback onChange);
    static void buildFilletForm(QFormLayout* layout, const features::FilletFeature* feat, ChangeCallback onChange);
    static void buildChamferForm(QFormLayout* layout, const features::ChamferFeature* feat, ChangeCallback onChange);
    static void buildSketchForm(QFormLayout* layout, const features::SketchFeature* feat);
    static void buildRectangularPatternForm(QFormLayout* layout, const features::RectangularPatternFeature* feat);
    static void buildShellForm(QFormLayout* layout, const features::ShellFeature* feat, ChangeCallback onChange);
    static void buildSweepForm(QFormLayout* layout, const features::SweepFeature* feat, ChangeCallback onChange);
    static void buildLoftForm(QFormLayout* layout, const features::LoftFeature* feat, ChangeCallback onChange);
    static void buildHoleForm(QFormLayout* layout, const features::HoleFeature* feat, ChangeCallback onChange);
    static void buildMirrorForm(QFormLayout* layout, const features::MirrorFeature* feat, ChangeCallback onChange);
    static void buildCircularPatternForm(QFormLayout* layout, const features::CircularPatternFeature* feat, ChangeCallback onChange);
    static void buildMoveForm(QFormLayout* layout, const features::MoveFeature* feat, ChangeCallback onChange);
    static void buildDraftForm(QFormLayout* layout, const features::DraftFeature* feat, ChangeCallback onChange);
    static void buildThickenForm(QFormLayout* layout, const features::ThickenFeature* feat, ChangeCallback onChange);
    static void buildThreadForm(QFormLayout* layout, const features::ThreadFeature* feat, ChangeCallback onChange);
    static void buildScaleForm(QFormLayout* layout, const features::ScaleFeature* feat, ChangeCallback onChange);
    static void buildCombineForm(QFormLayout* layout, const features::CombineFeature* feat, ChangeCallback onChange);
    static void buildSplitBodyForm(QFormLayout* layout, const features::SplitBodyFeature* feat, ChangeCallback onChange);
    static void buildOffsetFacesForm(QFormLayout* layout, const features::OffsetFacesFeature* feat, ChangeCallback onChange);
    static void buildGenericForm(QFormLayout* layout, const QString& featureId, const features::Feature* feat);

private:
    /// Parse a numeric value from an expression string like "10 mm" or "360 deg".
    static double parseExprValue(const std::string& expr);
};
