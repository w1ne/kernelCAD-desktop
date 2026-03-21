#pragma once
#include <QObject>
#include <QString>
#include "SelectionManager.h"

namespace kernel { class BRepModel; }

/// Measure tool that computes distances, angles, and areas between
/// selected 3D entities.  Results are displayed in the status bar
/// and as a viewport overlay.
class MeasureTool : public QObject
{
    Q_OBJECT
public:
    enum class MeasureMode {
        PointToPoint,
        PointToEdge,
        EdgeToEdge,
        FaceToFace,
        Angle
    };

    explicit MeasureTool(QObject* parent = nullptr);
    ~MeasureTool() override;

    void setMode(MeasureMode mode);
    MeasureMode mode() const { return m_mode; }

    void setFirstEntity(const SelectionHit& hit);
    void setSecondEntity(const SelectionHit& hit);

    void setBRepModel(kernel::BRepModel* model) { m_brepModel = model; }

    bool hasFirstEntity() const { return m_hasFirst; }
    bool hasResult() const { return m_hasResult; }

    struct MeasureResult {
        double distance = 0;   // mm
        double angle = 0;      // degrees
        double area = 0;       // mm^2
        QString description;
        // Endpoint coordinates for overlay rendering
        float p1x = 0, p1y = 0, p1z = 0;
        float p2x = 0, p2y = 0, p2z = 0;
    };

    MeasureResult compute() const;

    /// Clear state, ready for next measurement.
    void reset();

signals:
    void measurementReady(const MeasureResult& result);

private:
    MeasureMode m_mode = MeasureMode::PointToPoint;
    SelectionHit m_first;
    SelectionHit m_second;
    bool m_hasFirst = false;
    bool m_hasSecond = false;
    bool m_hasResult = false;
    kernel::BRepModel* m_brepModel = nullptr;
    MeasureResult m_lastResult;
};
