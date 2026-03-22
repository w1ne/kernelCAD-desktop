#pragma once
#include <QObject>
#include <QMatrix4x4>
#include <QVector3D>
#include <QTimer>

enum class StandardView;

/// Manages camera state and navigation for a 3D viewport.
/// Extracted from Viewport3D to separate camera logic from rendering.
class CameraController : public QObject
{
    Q_OBJECT
public:
    explicit CameraController(QObject* parent = nullptr);

    // ── camera state accessors ───────────────────────────────────────────
    QVector3D eye() const { return m_eye; }
    QVector3D center() const { return m_center; }
    QVector3D up() const { return m_up; }
    float fov() const { return m_fov; }
    float nearPlane() const { return m_near; }
    float farPlane() const { return m_far; }
    float orbitDistance() const { return m_orbitDistance; }
    bool isPerspective() const { return m_perspectiveProjection; }

    // ── matrices ─────────────────────────────────────────────────────────
    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projectionMatrix(float aspect) const;
    void buildProjectionMatrix(QMatrix4x4& out, float aspect) const;

    /// Dynamically adjust near/far planes based on scene size and camera distance.
    void updateClipPlanes(const QVector3D& bboxMin, const QVector3D& bboxMax);

    // ── navigation ───────────────────────────────────────────────────────
    /// Orbit (arcball rotation) by screen-space deltas.
    /// @param va  arcball vector at previous mouse position
    /// @param vb  arcball vector at current mouse position
    void orbit(const QVector3D& va, const QVector3D& vb);

    /// Pan the camera by screen-space pixel deltas.
    void pan(float dx, float dy, float viewportWidth, float viewportHeight);

    /// Zoom by mouse-wheel delta, optionally toward cursor in NDC.
    void zoom(float delta, float ndcX, float ndcY, float viewportAspect);

    /// Frame the camera so the bounding box is visible.
    void fitAll(const QVector3D& bboxMin, const QVector3D& bboxMax);

    /// Set a standard view preset.
    void setStandardView(StandardView view);

    /// Position camera along direction at m_orbitDistance from m_center.
    void setStandardView(const QVector3D& direction, const QVector3D& up);

    /// Animate camera to target position.
    void animateTo(const QVector3D& targetEye, const QVector3D& targetCenter,
                   const QVector3D& targetUp, int durationMs = 300);

    /// Toggle perspective / orthographic projection.
    void togglePerspective();

    // ── save / restore (for sketch mode) ─────────────────────────────────
    void saveCameraState();
    void restoreCameraState(bool animate = true);
    void setLockRotation(bool lock) { m_lockRotation = lock; }
    bool lockRotation() const { return m_lockRotation; }

    // ── momentum ─────────────────────────────────────────────────────────
    void startMomentum(float dx, float dy);
    void stopMomentum();

    // ── direct state setters (used by Viewport3D for double-click orbit center, etc.) ──
    void setEye(const QVector3D& e) { m_eye = e; }
    void setCenter(const QVector3D& c) { m_center = c; }
    void setUp(const QVector3D& u) { m_up = u; }
    void setOrbitDistance(float d) { m_orbitDistance = d; }
    void setNear(float n) { m_near = n; }
    void setFar(float f) { m_far = f; }

signals:
    /// Emitted whenever camera state changes and the viewport needs a repaint.
    void cameraChanged();

private:
    // ── camera state ─────────────────────────────────────────────────────
    QVector3D m_eye    {0.0f, 0.0f, 5.0f};
    QVector3D m_center {0.0f, 0.0f, 0.0f};
    QVector3D m_up     {0.0f, 1.0f, 0.0f};
    float m_fov  = 45.0f;
    float m_near = 0.1f;
    float m_far  = 100000.0f;

    float m_orbitDistance = 5.0f;
    bool  m_perspectiveProjection = true;

    // ── smooth camera animation ──────────────────────────────────────────
    QTimer  m_animTimer;
    int     m_animDuration    = 300;
    int     m_animElapsed     = 0;
    bool    m_animating       = false;
    QVector3D m_animStartEye, m_animStartCenter, m_animStartUp;
    QVector3D m_animEndEye,   m_animEndCenter,   m_animEndUp;
    static constexpr int kAnimTickMs = 16;  // ~60 fps

    // ── orbit momentum ───────────────────────────────────────────────────
    QTimer m_momentumTimer;
    float m_momentumDx = 0.0f;
    float m_momentumDy = 0.0f;

    // Helper: compute arcball vector for a virtual screen position (used by momentum)
    QVector3D arcballVector(int x, int y, int w, int h) const;

    // ── saved camera state ───────────────────────────────────────────────
    QVector3D m_savedEye    {0.0f, 0.0f, 5.0f};
    QVector3D m_savedCenter {0.0f, 0.0f, 0.0f};
    QVector3D m_savedUp     {0.0f, 1.0f, 0.0f};
    bool      m_hasSavedCamera = false;

    // ── sketch mode lock ─────────────────────────────────────────────────
    bool m_lockRotation = false;
};
