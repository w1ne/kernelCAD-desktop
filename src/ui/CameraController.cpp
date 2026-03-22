#include "CameraController.h"
#include "Viewport3D.h"  // for StandardView enum
#include <QtMath>
#include <cmath>
#include <algorithm>

// =============================================================================
// Construction
// =============================================================================

CameraController::CameraController(QObject* parent)
    : QObject(parent)
{
    // Orbit momentum timer (~60fps, decaying rotation after releasing middle-button)
    m_momentumTimer.setInterval(kAnimTickMs);
    connect(&m_momentumTimer, &QTimer::timeout, this, [this]() {
        // Apply diminishing rotation
        if (std::abs(m_momentumDx) < 0.1f && std::abs(m_momentumDy) < 0.1f) {
            m_momentumTimer.stop();
            return;
        }
        // Simulate small arcball rotation from momentum deltas
        // Use a virtual 500x500 viewport for the arcball calculation
        const int vw = 500, vh = 500;
        QVector3D va = arcballVector(vw / 2, vh / 2, vw, vh);
        QVector3D vb = arcballVector(vw / 2 + static_cast<int>(m_momentumDx),
                                     vh / 2 + static_cast<int>(m_momentumDy), vw, vh);
        float angle = std::acos(std::min(1.0f, QVector3D::dotProduct(va, vb)));
        QVector3D axis = QVector3D::crossProduct(va, vb);
        QMatrix4x4 viewMat;
        viewMat.lookAt(m_eye, m_center, m_up);
        QVector3D worldAxis = viewMat.inverted().mapVector(axis);
        if (worldAxis.length() > 1e-6f) {
            worldAxis.normalize();
            QVector3D offset = m_eye - m_center;
            QMatrix4x4 rot;
            rot.rotate(qRadiansToDegrees(angle) * 2.0f, worldAxis);
            offset = rot.map(offset);
            m_up = rot.mapVector(m_up).normalized();
            m_eye = m_center + offset;
            m_orbitDistance = offset.length();

            // Clamp up vector to prevent barrel roll
            QVector3D fwd = (m_center - m_eye).normalized();
            QVector3D worldUp(0.0f, 1.0f, 0.0f);
            if (std::abs(QVector3D::dotProduct(fwd, worldUp)) < 0.95f) {
                QVector3D rt = QVector3D::crossProduct(fwd, worldUp).normalized();
                m_up = QVector3D::crossProduct(rt, fwd).normalized();
            }
        }
        m_momentumDx *= 0.9f;
        m_momentumDy *= 0.9f;
        emit cameraChanged();
    });

    // Camera animation timer
    m_animTimer.setInterval(kAnimTickMs);
    connect(&m_animTimer, &QTimer::timeout, this, [this]() {
        m_animElapsed += kAnimTickMs;
        float t = std::min(1.0f, static_cast<float>(m_animElapsed) / m_animDuration);
        // Smooth ease-in-out (cubic)
        float s = (t < 0.5f) ? 4.0f * t * t * t
                              : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;

        m_eye    = m_animStartEye    + s * (m_animEndEye    - m_animStartEye);
        m_center = m_animStartCenter + s * (m_animEndCenter - m_animStartCenter);
        // Slerp-like up vector interpolation (normalize after lerp)
        QVector3D upLerp = m_animStartUp + s * (m_animEndUp - m_animStartUp);
        float upLen = upLerp.length();
        m_up = (upLen > 1e-6f) ? upLerp / upLen : m_animEndUp;

        emit cameraChanged();
        if (t >= 1.0f) {
            m_animating = false;
            m_animTimer.stop();
        }
    });
}

// =============================================================================
// Matrices
// =============================================================================

QMatrix4x4 CameraController::viewMatrix() const
{
    QMatrix4x4 v;
    v.lookAt(m_eye, m_center, m_up);
    return v;
}

QMatrix4x4 CameraController::projectionMatrix(float aspect) const
{
    QMatrix4x4 p;
    buildProjectionMatrix(p, aspect);
    return p;
}

void CameraController::buildProjectionMatrix(QMatrix4x4& out, float aspect) const
{
    out.setToIdentity();
    if (m_perspectiveProjection) {
        out.perspective(m_fov, aspect, m_near, m_far);
    } else {
        // Orthographic: size matches what perspective would show at orbit distance
        const float halfH = m_orbitDistance * std::tan(qDegreesToRadians(m_fov) * 0.5f);
        const float halfW = halfH * aspect;
        out.ortho(-halfW, halfW, -halfH, halfH, m_near, m_far);
    }
}

// =============================================================================
// Dynamic clip planes
// =============================================================================

void CameraController::updateClipPlanes(const QVector3D& bboxMin, const QVector3D& bboxMax)
{
    QVector3D bboxCenter = (bboxMin + bboxMax) * 0.5f;
    float sceneRadius = (bboxMax - bboxMin).length() * 0.5f;
    if (sceneRadius < 1.0f) sceneRadius = 100.0f;
    float camDist = (m_eye - bboxCenter).length();
    m_near = std::max(0.1f, camDist * 0.001f);
    m_far  = std::max(10000.0f, camDist + sceneRadius * 3.0f);
}

// =============================================================================
// Navigation
// =============================================================================

void CameraController::orbit(const QVector3D& va, const QVector3D& vb)
{
    float angle = std::acos(std::min(1.0f, QVector3D::dotProduct(va, vb)));
    QVector3D axis = QVector3D::crossProduct(va, vb);

    // The axis is in screen/camera space -- transform to world space.
    QMatrix4x4 viewMat;
    viewMat.lookAt(m_eye, m_center, m_up);
    QMatrix4x4 invView = viewMat.inverted();

    // Transform axis from camera space to world space (rotation only)
    QVector3D worldAxis = invView.mapVector(axis);
    if (worldAxis.length() > 1e-6f) {
        worldAxis.normalize();

        // Rotate eye around center
        QVector3D offset = m_eye - m_center;
        QMatrix4x4 rot;
        rot.rotate(qRadiansToDegrees(angle) * 2.0f, worldAxis);
        offset = rot.map(offset);
        m_up   = rot.mapVector(m_up).normalized();
        m_eye  = m_center + offset;
        m_orbitDistance = offset.length();

        // Clamp up vector to prevent barrel roll (keep world-up stable)
        QVector3D fwd = (m_center - m_eye).normalized();
        QVector3D worldUp(0.0f, 1.0f, 0.0f);
        if (std::abs(QVector3D::dotProduct(fwd, worldUp)) < 0.95f) {
            QVector3D rt = QVector3D::crossProduct(fwd, worldUp).normalized();
            m_up = QVector3D::crossProduct(rt, fwd).normalized();
        }
    }
    emit cameraChanged();
}

void CameraController::pan(float dx, float dy, float /*viewportWidth*/, float /*viewportHeight*/)
{
    QVector3D forward = (m_center - m_eye).normalized();
    QVector3D right   = QVector3D::crossProduct(forward, m_up).normalized();
    QVector3D camUp   = QVector3D::crossProduct(right, forward).normalized();

    const float panSpeed = m_orbitDistance * 0.002f;
    QVector3D shift = (-dx * right + dy * camUp) * panSpeed;

    m_eye    += shift;
    m_center += shift;
    emit cameraChanged();
}

void CameraController::zoom(float delta, float ndcX, float ndcY, float viewportAspect)
{
    const float factor = std::pow(1.1f, delta);

    // Compute the world-space point under the cursor for zoom-to-cursor
    QMatrix4x4 view;
    view.lookAt(m_eye, m_center, m_up);
    QMatrix4x4 proj;
    buildProjectionMatrix(proj, viewportAspect);
    QMatrix4x4 invVP = (proj * view).inverted();

    QVector4D nearPt = invVP * QVector4D(ndcX, ndcY, 0.0f, 1.0f);
    if (std::abs(nearPt.w()) > 1e-7f) nearPt /= nearPt.w();
    QVector3D cursorWorld = nearPt.toVector3D();

    // Compute new orbit distance
    float newDist = m_orbitDistance / factor;
    newDist = std::max(m_near * 2.0f, std::min(100000.0f, newDist));

    // Shift the orbit center toward the cursor on zoom-in, away on zoom-out
    float shiftAmount = (1.0f - 1.0f / factor) * 0.3f;
    QVector3D centerShift = (cursorWorld - m_center) * shiftAmount;
    m_center += centerShift;

    QVector3D direction = (m_eye - m_center).normalized();
    m_orbitDistance = newDist;
    m_eye = m_center + direction * m_orbitDistance;

    emit cameraChanged();
}

// =============================================================================
// fitAll -- frame the bounding box
// =============================================================================

void CameraController::fitAll(const QVector3D& bboxMin, const QVector3D& bboxMax)
{
    const QVector3D center = (bboxMin + bboxMax) * 0.5f;
    const float     radius = (bboxMax - bboxMin).length() * 0.5f;

    // Place the camera so the bounding sphere just fits inside the frustum.
    const float fovRad   = qDegreesToRadians(m_fov);
    const float distance = radius / std::sin(fovRad * 0.5f);

    float newOrbitDist = distance * 1.1f;
    QVector3D targetCenter = center;
    QVector3D targetEye = targetCenter + QVector3D(0.0f, 0.0f, newOrbitDist);
    QVector3D targetUp(0.0f, 1.0f, 0.0f);

    // Update near/far and orbit distance immediately (animation only moves camera position)
    m_orbitDistance = newOrbitDist;
    m_near = std::max(0.001f, m_orbitDistance - radius * 2.0f);
    m_far  = m_orbitDistance + radius * 2.0f;

    animateTo(targetEye, targetCenter, targetUp, 300);
}

// =============================================================================
// Standard views
// =============================================================================

void CameraController::setStandardView(const QVector3D& direction, const QVector3D& up)
{
    QVector3D targetEye = m_center + direction * m_orbitDistance;
    animateTo(targetEye, m_center, up, 300);
}

void CameraController::setStandardView(StandardView view)
{
    const QVector3D upY(0.0f, 1.0f, 0.0f);
    const QVector3D upZ(0.0f, 0.0f, 1.0f);

    switch (view) {
    case StandardView::Front:
        setStandardView(QVector3D(0,  1, 0), upZ);
        break;
    case StandardView::Back:
        setStandardView(QVector3D(0, -1, 0), upZ);
        break;
    case StandardView::Right:
        setStandardView(QVector3D(-1, 0, 0), upZ);
        break;
    case StandardView::Left:
        setStandardView(QVector3D( 1, 0, 0), upZ);
        break;
    case StandardView::Top:
        setStandardView(QVector3D(0, 0,  1), upY);
        break;
    case StandardView::Bottom:
        setStandardView(QVector3D(0, 0, -1), -upY);
        break;
    case StandardView::Isometric: {
        const float k = 0.577350269f; // 1/sqrt(3)
        QVector3D dir(k, k, k);
        // Compute an up vector perpendicular to the view direction
        QVector3D right = QVector3D::crossProduct(dir, upZ).normalized();
        QVector3D up    = QVector3D::crossProduct(right, dir).normalized();
        setStandardView(dir, up);
        break;
    }
    }
}

// =============================================================================
// Smooth camera animation
// =============================================================================

void CameraController::animateTo(const QVector3D& targetEye, const QVector3D& targetCenter,
                                  const QVector3D& targetUp, int durationMs)
{
    // If the move is very small, snap immediately (avoids jitter)
    float dist = (targetEye - m_eye).length() + (targetCenter - m_center).length();
    if (dist < 1e-4f || durationMs <= 0) {
        m_eye    = targetEye;
        m_center = targetCenter;
        m_up     = targetUp;
        emit cameraChanged();
        return;
    }

    m_animStartEye    = m_eye;
    m_animStartCenter = m_center;
    m_animStartUp     = m_up;
    m_animEndEye      = targetEye;
    m_animEndCenter   = targetCenter;
    m_animEndUp       = targetUp;
    m_animDuration    = durationMs;
    m_animElapsed     = 0;
    m_animating       = true;
    m_animTimer.start();
}

// =============================================================================
// Perspective toggle
// =============================================================================

void CameraController::togglePerspective()
{
    m_perspectiveProjection = !m_perspectiveProjection;
    emit cameraChanged();
}

// =============================================================================
// Save / restore
// =============================================================================

void CameraController::saveCameraState()
{
    m_savedEye    = m_eye;
    m_savedCenter = m_center;
    m_savedUp     = m_up;
    m_hasSavedCamera = true;
}

void CameraController::restoreCameraState(bool animate)
{
    if (!m_hasSavedCamera)
        return;
    if (animate) {
        animateTo(m_savedEye, m_savedCenter, m_savedUp, 400);
    } else {
        m_eye    = m_savedEye;
        m_center = m_savedCenter;
        m_up     = m_savedUp;
    }
    m_hasSavedCamera = false;
}

// =============================================================================
// Momentum
// =============================================================================

void CameraController::startMomentum(float dx, float dy)
{
    m_momentumDx = dx;
    m_momentumDy = dy;
    m_momentumTimer.start();
}

void CameraController::stopMomentum()
{
    m_momentumTimer.stop();
}

// =============================================================================
// Arcball helper (for momentum -- uses virtual viewport dimensions)
// =============================================================================

QVector3D CameraController::arcballVector(int x, int y, int w, int h) const
{
    const float fw = static_cast<float>(w);
    const float fh = static_cast<float>(h);

    // Map to [-1, 1]
    float ax =  (2.0f * x - fw) / fw;
    float ay = -(2.0f * y - fh) / fh;   // flip Y

    float lenSq = ax * ax + ay * ay;
    float az;
    if (lenSq <= 1.0f)
        az = std::sqrt(1.0f - lenSq);
    else {
        // outside the sphere -- project onto it
        float len = std::sqrt(lenSq);
        ax /= len;
        ay /= len;
        az = 0.0f;
    }
    return QVector3D(ax, ay, az);
}
