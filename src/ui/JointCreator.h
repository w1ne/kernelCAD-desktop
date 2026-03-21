#pragma once
#include <QObject>
#include <string>
#include "../features/Joint.h"
#include "../ui/SelectionManager.h"

namespace kernel { class BRepModel; }

/// Interactive two-click face-to-face joint creation workflow.
/// The user presses 'J', clicks a face on component A, clicks a face
/// on component B, and a rigid joint is auto-created aligning those faces.
class JointCreator : public QObject
{
    Q_OBJECT
public:
    explicit JointCreator(QObject* parent = nullptr);

    enum class State { Idle, PickingFace1, PickingFace2 };

    /// Start an interactive joint creation session.
    void begin(features::JointType type);

    /// Cancel the current session.
    void cancel();

    /// Called when a face is selected in the viewport.
    void onFaceSelected(const SelectionHit& hit);

    State state() const;

    /// Set the BRepModel used for face geometry queries.
    void setBRepModel(kernel::BRepModel* brep);

signals:
    void jointReady(features::JointParams params);
    void stateChanged(JointCreator::State state);
    void cancelled();

private:
    State m_state = State::Idle;
    features::JointType m_jointType = features::JointType::Rigid;
    kernel::BRepModel* m_brep = nullptr;

    // First face selection
    std::string m_body1Id;
    int m_face1Index = -1;
    double m_face1NormalX = 0, m_face1NormalY = 0, m_face1NormalZ = 0;
    double m_face1CenterX = 0, m_face1CenterY = 0, m_face1CenterZ = 0;

    /// Compute joint geometry from face info.
    features::JointGeometry geometryFromFace(const std::string& bodyId, int faceIndex) const;
};
