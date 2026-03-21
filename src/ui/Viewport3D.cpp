#include "Viewport3D.h"
#include "ViewportManipulator.h"
#include "SketchEditor.h"
#include "SelectionManager.h"
#include "../sketch/Sketch.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QSurfaceFormat>
#include <QOpenGLFramebufferObject>
#include <QPainter>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

// =============================================================================
// Inline shaders -- Blinn-Phong lighting
// =============================================================================

static const char* const kVertexShaderSrc = R"glsl(
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in float aFaceId;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

out vec3 vFragPos;
out vec3 vNormal;
flat out int vFaceId;

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vFragPos = worldPos.xyz;
    vNormal  = normalize(uNormalMatrix * aNormal);
    vFaceId  = int(aFaceId);
    gl_Position = uProjection * uView * worldPos;
}
)glsl";

static const char* const kFragmentShaderSrc = R"glsl(
#version 330 core

in vec3 vFragPos;
in vec3 vNormal;
flat in int vFaceId;

uniform vec3 uViewPos;
uniform vec3 uLightDir;      // directional light (world-space, normalised)
uniform vec3 uLightColor;
uniform vec3 uObjectColor;

// Section / clipping plane
uniform vec4 uClipPlane;     // (nx, ny, nz, d)
uniform bool uClipEnabled;

// Selection/highlight data
uniform int uHighlightedFaces[64];
uniform int uHighlightedFaceCount;
uniform int uPreSelectedFace;
uniform float uAlpha;          // overall alpha (1.0 for solid, 0.5 for preview)

out vec4 FragColor;

bool isFaceHighlighted(int faceId)
{
    for (int i = 0; i < uHighlightedFaceCount && i < 64; ++i) {
        if (uHighlightedFaces[i] == faceId)
            return true;
    }
    return false;
}

void main()
{
    // Clip against section plane
    if (uClipEnabled && dot(vec4(vFragPos, 1.0), uClipPlane) < 0.0)
        discard;

    // ambient
    float ambientStrength = 0.15;
    vec3 ambient = ambientStrength * uLightColor;

    // diffuse
    vec3 norm = normalize(vNormal);
    float diff = max(dot(norm, uLightDir), 0.0);
    vec3 diffuse = diff * uLightColor;

    // specular (Blinn-Phong)
    vec3 viewDir  = normalize(uViewPos - vFragPos);
    vec3 halfDir  = normalize(uLightDir + viewDir);
    float spec    = pow(max(dot(norm, halfDir), 0.0), 64.0);
    vec3 specular = 0.5 * spec * uLightColor;

    vec3 baseColor = uObjectColor;

    // Apply selection highlight (blue tint) for selected faces
    if (isFaceHighlighted(vFaceId)) {
        baseColor = mix(baseColor, vec3(0.2, 0.4, 0.9), 0.45);
    }
    // Apply pre-selection highlight (lighter blue tint) for hovered face
    else if (vFaceId == uPreSelectedFace && uPreSelectedFace >= 0) {
        baseColor = mix(baseColor, vec3(0.3, 0.5, 0.85), 0.25);
    }

    vec3 result = (ambient + diffuse + specular) * baseColor;
    float a = (uAlpha > 0.0) ? uAlpha : 1.0;
    FragColor = vec4(result, a);
}
)glsl";

// =============================================================================
// Pick shaders -- encode face ID as color
// =============================================================================

static const char* const kPickVertexShaderSrc = R"glsl(
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in float aFaceId;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

flat out int vFaceId;

void main()
{
    vFaceId = int(aFaceId);
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
)glsl";

static const char* const kPickFragmentShaderSrc = R"glsl(
#version 330 core

flat in int vFaceId;

out vec4 FragColor;

void main()
{
    // Encode face ID + 1 as color (0 = background/miss)
    int id = vFaceId + 1;
    float r = float(id & 0xFF) / 255.0;
    float g = float((id >> 8) & 0xFF) / 255.0;
    float b = float((id >> 16) & 0xFF) / 255.0;
    FragColor = vec4(r, g, b, 1.0);
}
)glsl";

// =============================================================================
// Edge shaders -- simple solid-color lines
// =============================================================================

static const char* const kEdgeVertexShaderSrc = R"glsl(
#version 330 core

layout(location = 0) in vec3 aPos;

uniform mat4 uMVP;
uniform float uDepthBias;   // small bias to push edges in front of faces

void main()
{
    vec4 pos = uMVP * vec4(aPos, 1.0);
    // Apply a small depth bias toward the camera to prevent z-fighting
    pos.z -= uDepthBias * pos.w;
    gl_Position = pos;
}
)glsl";

static const char* const kEdgeFragmentShaderSrc = R"glsl(
#version 330 core

uniform vec3 uEdgeColor;
uniform float uEdgeAlpha;   // alpha channel (default 1.0, used for grid fading)

out vec4 FragColor;

void main()
{
    FragColor = vec4(uEdgeColor, uEdgeAlpha);
}
)glsl";

// =============================================================================
// Sketch overlay shaders -- simple 2D geometry on sketch plane
// =============================================================================

static const char* const kSketchOverlayVertSrc = R"glsl(
#version 330 core

layout(location = 0) in vec3 aPos;

uniform mat4 uMVP;

void main()
{
    gl_Position = uMVP * vec4(aPos, 1.0);
    gl_PointSize = 6.0;
}
)glsl";

static const char* const kSketchOverlayFragSrc = R"glsl(
#version 330 core

uniform vec4 uColor;

out vec4 FragColor;

void main()
{
    FragColor = uColor;
}
)glsl";

// =============================================================================
// Construction / Destruction
// =============================================================================

Viewport3D::Viewport3D(QWidget* parent)
    : QOpenGLWidget(parent)
{
    // Request an OpenGL 3.3 Core context
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(4);
    setFormat(fmt);

    // Enable mouse tracking so mouseMoveEvent fires without buttons pressed
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

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

        update();
        if (t >= 1.0f) {
            m_animating = false;
            m_animTimer.stop();
        }
    });
}

Viewport3D::~Viewport3D()
{
    makeCurrent();
    m_vao.destroy();
    m_vboPos.destroy();
    m_vboNorm.destroy();
    m_vboFaceId.destroy();
    m_ebo.destroy();
    m_edgeVao.destroy();
    m_edgeVboPos.destroy();
    m_edgeEbo.destroy();
    m_gridVao.destroy();
    m_gridVbo.destroy();
    // Destroy per-body GPU resources
    for (auto& bg : m_bodyGPUs) {
        bg->vao.destroy();
        bg->vboPos.destroy();
        bg->vboNorm.destroy();
        bg->vboFaceId.destroy();
        bg->ebo.destroy();
        bg->edgeVao.destroy();
        bg->edgeVboPos.destroy();
        bg->edgeEbo.destroy();
    }
    m_bodyGPUs.clear();
    m_pickFBO.reset();
    m_sectionQuadVao.destroy();
    m_sectionQuadVbo.destroy();
    delete m_program;
    delete m_pickProgram;
    delete m_edgeProgram;
    delete m_sketchOverlayProgram;
    delete m_sectionQuadProgram;
    doneCurrent();
}

// =============================================================================
// OpenGL lifecycle
// =============================================================================

void Viewport3D::initializeGL()
{
    initializeOpenGLFunctions();   // QOpenGLFunctions_3_3_Core

    glClearColor(0.18f, 0.18f, 0.18f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    buildShaderProgram();
    buildPickShaderProgram();
    buildEdgeShaderProgram();
    buildSketchOverlayShader();
    buildSectionQuadShader();

    // Create (but do not yet populate) GPU buffers for solid mesh
    m_vao.create();
    m_vboPos.create();
    m_vboNorm.create();
    m_vboFaceId.create();
    m_ebo.create();

    // Create edge GPU buffers
    m_edgeVao.create();
    m_edgeVboPos.create();
    m_edgeEbo.create();

    // Create ground grid and origin axes buffers
    initGridBuffers();
}

void Viewport3D::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    // Invalidate pick FBO so it gets recreated at new size
    m_pickFBO.reset();
}

void Viewport3D::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // -- common matrices -------------------------------------------------
    QMatrix4x4 model;                           // identity
    QMatrix4x4 view;
    view.lookAt(m_eye, m_center, m_up);

    QMatrix4x4 projection;
    buildProjectionMatrix(projection);

    QMatrix4x4 mvp = projection * view * model;

    // ====================================================================
    // 0. Draw ground grid and origin axes (behind everything)
    // ====================================================================
    if (m_showGrid && m_gridInitialized && m_edgeProgram) {
        drawGrid(mvp);
        drawOriginAxes(mvp);
    }

    // ====================================================================
    // Determine what to draw based on view mode
    // ====================================================================
    const bool drawFaces = (m_viewMode == ViewMode::SolidWithEdges ||
                            m_viewMode == ViewMode::Solid ||
                            m_viewMode == ViewMode::HiddenLine);
    const bool drawEdges = (m_viewMode == ViewMode::SolidWithEdges ||
                            m_viewMode == ViewMode::Wireframe ||
                            m_viewMode == ViewMode::HiddenLine);

    // ====================================================================
    // 1. Draw solid faces (per-body or legacy single-mesh)
    // ====================================================================
    if (drawFaces && m_bodiesLoaded && m_program) {
        if (m_viewMode == ViewMode::HiddenLine) {
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        }

        m_program->bind();

        QMatrix3x3 normalMatrix = model.normalMatrix();

        m_program->setUniformValue("uModel",        model);
        m_program->setUniformValue("uView",         view);
        m_program->setUniformValue("uProjection",   projection);
        m_program->setUniformValue("uNormalMatrix", normalMatrix);

        // -- clipping plane -----------------------------------------------
        m_program->setUniformValue("uClipEnabled", m_clipEnabled);
        m_program->setUniformValue("uClipPlane", m_clipPlane);

        // -- lighting ----------------------------------------------------
        QVector3D lightDir = QVector3D(0.3f, 1.0f, 0.5f).normalized();
        m_program->setUniformValue("uViewPos",     m_eye);
        m_program->setUniformValue("uLightDir",    lightDir);
        m_program->setUniformValue("uLightColor",  QVector3D(1.0f, 1.0f, 1.0f));

        // -- highlight uniforms ------------------------------------------
        int highlightCount = std::min(static_cast<int>(m_highlightedFaces.size()), 64);
        int highlightArray[64] = {};
        for (int i = 0; i < highlightCount; ++i)
            highlightArray[i] = m_highlightedFaces[static_cast<size_t>(i)];

        GLint loc = m_program->uniformLocation("uHighlightedFaces");
        if (loc >= 0)
            glUniform1iv(loc, 64, highlightArray);
        m_program->setUniformValue("uHighlightedFaceCount", highlightCount);

        // Pre-selected face (hover)
        int preSelectedFace = -1;
        if (m_selectionMgr && m_selectionMgr->hasPreSelection())
            preSelectedFace = m_selectionMgr->preSelection()->faceIndex;
        m_program->setUniformValue("uPreSelectedFace", preSelectedFace);
        m_program->setUniformValue("uAlpha", 1.0f);

        // -- compute assembly center for exploded view -------------------
        QVector3D assemblyCenter(0, 0, 0);
        float maxBboxSize = 1.0f;
        if (m_explodeFactor > 0.0f && !m_bodyGPUs.empty()) {
            assemblyCenter = (m_bboxMin + m_bboxMax) * 0.5f;
            maxBboxSize = (m_bboxMax - m_bboxMin).length();
            if (maxBboxSize < 1e-6f) maxBboxSize = 1.0f;
        }

        // -- draw each visible body with its own color -------------------
        for (size_t bgi = 0; bgi < m_bodyGPUs.size(); ++bgi) {
            const auto& bg = m_bodyGPUs[bgi];
            if (!bg->isVisible)
                continue;

            // Compute per-body model matrix with explode offset
            QMatrix4x4 bodyModel;
            if (m_explodeFactor > 0.0f) {
                // Approximate body center from overall bbox partitioning:
                // use the body index to spread bodies outward from center.
                // For a more accurate approach we would need per-body bboxes,
                // but this works well for the viewport-level explode.
                QVector3D bodyCenter = assemblyCenter; // default
                // Try to find per-body center from stored vertex data
                // A simple proxy: use the first vertex of each body as a seed
                // This is recalculated each frame but only when explode > 0.
                (void)bodyCenter; // Use actual direction from assembly center
                float angle = static_cast<float>(bgi) * 6.2832f / static_cast<float>(m_bodyGPUs.size());
                QVector3D dir(std::cos(angle), 0.0f, std::sin(angle));
                if (m_bodyGPUs.size() == 1) dir = QVector3D(0, 0, 0);
                bodyModel.translate(dir * m_explodeFactor * maxBboxSize * 0.5f);
            }

            QMatrix3x3 bodyNormalMatrix = bodyModel.normalMatrix();
            m_program->setUniformValue("uModel", bodyModel);
            m_program->setUniformValue("uNormalMatrix", bodyNormalMatrix);

            // Apply red tint for bodies with errored features
            if (bg->hasError) {
                float tintR = bg->colorR * 0.4f + 0.6f;  // shift toward red
                float tintG = bg->colorG * 0.3f;
                float tintB = bg->colorB * 0.3f;
                m_program->setUniformValue("uObjectColor",
                    QVector3D(tintR, tintG, tintB));
            } else {
                m_program->setUniformValue("uObjectColor",
                    QVector3D(bg->colorR, bg->colorG, bg->colorB));
            }

            bg->vao.bind();
            glDrawElements(GL_TRIANGLES, bg->indexCount, GL_UNSIGNED_INT, nullptr);
            bg->vao.release();
        }

        // Reset model matrix after body loop
        m_program->setUniformValue("uModel", model);
        m_program->setUniformValue("uNormalMatrix", model.normalMatrix());

        m_program->release();

        if (m_viewMode == ViewMode::HiddenLine) {
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }
    }
    else if (drawFaces && m_meshLoaded && m_program) {
        // Legacy single-mesh fallback path
        if (m_viewMode == ViewMode::HiddenLine) {
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        }

        m_program->bind();

        QMatrix3x3 normalMatrix = model.normalMatrix();

        m_program->setUniformValue("uModel",        model);
        m_program->setUniformValue("uView",         view);
        m_program->setUniformValue("uProjection",   projection);
        m_program->setUniformValue("uNormalMatrix", normalMatrix);

        // -- clipping plane -----------------------------------------------
        m_program->setUniformValue("uClipEnabled", m_clipEnabled);
        m_program->setUniformValue("uClipPlane", m_clipPlane);

        QVector3D lightDir = QVector3D(0.3f, 1.0f, 0.5f).normalized();
        m_program->setUniformValue("uViewPos",     m_eye);
        m_program->setUniformValue("uLightDir",    lightDir);
        m_program->setUniformValue("uLightColor",  QVector3D(1.0f, 1.0f, 1.0f));
        m_program->setUniformValue("uObjectColor", QVector3D(0.6f, 0.65f, 0.7f));
        m_program->setUniformValue("uAlpha", 1.0f);

        int highlightCount = std::min(static_cast<int>(m_highlightedFaces.size()), 64);
        int highlightArray[64] = {};
        for (int i = 0; i < highlightCount; ++i)
            highlightArray[i] = m_highlightedFaces[static_cast<size_t>(i)];

        GLint loc = m_program->uniformLocation("uHighlightedFaces");
        if (loc >= 0)
            glUniform1iv(loc, 64, highlightArray);
        m_program->setUniformValue("uHighlightedFaceCount", highlightCount);

        int preSelectedFace = -1;
        if (m_selectionMgr && m_selectionMgr->hasPreSelection())
            preSelectedFace = m_selectionMgr->preSelection()->faceIndex;
        m_program->setUniformValue("uPreSelectedFace", preSelectedFace);

        m_vao.bind();
        glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
        m_vao.release();

        m_program->release();

        if (m_viewMode == ViewMode::HiddenLine) {
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }
    }

    // ====================================================================
    // 2. Draw edges (per-body or legacy single-mesh)
    // ====================================================================
    if (drawEdges && m_bodiesLoaded && m_edgeProgram) {
        m_edgeProgram->bind();

        m_edgeProgram->setUniformValue("uMVP", mvp);

        float depthBias = (m_viewMode == ViewMode::Wireframe) ? 0.0f : 0.0005f;
        m_edgeProgram->setUniformValue("uDepthBias", depthBias);

        QVector3D edgeColor;
        if (m_viewMode == ViewMode::Wireframe || m_viewMode == ViewMode::HiddenLine)
            edgeColor = QVector3D(0.85f, 0.85f, 0.85f);
        else
            edgeColor = QVector3D(0.1f, 0.1f, 0.1f);

        m_edgeProgram->setUniformValue("uEdgeColor", edgeColor);
        m_edgeProgram->setUniformValue("uEdgeAlpha", 1.0f);

        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        glLineWidth(1.5f);

        for (const auto& bg : m_bodyGPUs) {
            if (!bg->isVisible || bg->edgeIndexCount == 0)
                continue;

            bg->edgeVao.bind();
            glDrawElements(GL_LINES, bg->edgeIndexCount, GL_UNSIGNED_INT, nullptr);
            bg->edgeVao.release();
        }

        glDisable(GL_LINE_SMOOTH);

        m_edgeProgram->release();
    }
    else if (drawEdges && m_edgesLoaded && m_edgeProgram) {
        // Legacy single-mesh edge fallback path
        m_edgeProgram->bind();

        m_edgeProgram->setUniformValue("uMVP", mvp);

        float depthBias = (m_viewMode == ViewMode::Wireframe) ? 0.0f : 0.0005f;
        m_edgeProgram->setUniformValue("uDepthBias", depthBias);

        QVector3D edgeColor;
        if (m_viewMode == ViewMode::Wireframe || m_viewMode == ViewMode::HiddenLine)
            edgeColor = QVector3D(0.85f, 0.85f, 0.85f);
        else
            edgeColor = QVector3D(0.1f, 0.1f, 0.1f);

        m_edgeProgram->setUniformValue("uEdgeColor", edgeColor);
        m_edgeProgram->setUniformValue("uEdgeAlpha", 1.0f);

        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        glLineWidth(1.5f);

        m_edgeVao.bind();
        glDrawElements(GL_LINES, m_edgeIndexCount, GL_UNSIGNED_INT, nullptr);
        m_edgeVao.release();

        glDisable(GL_LINE_SMOOTH);

        m_edgeProgram->release();
    }

    // -- section plane quad (semi-transparent cut surface) ------------------
    if (m_clipEnabled && m_sectionQuadProgram) {
        drawSectionQuad(view, projection);
    }

    // -- preview mesh overlay (semi-transparent blue) ----------------------
    if (m_previewLoaded && m_program) {
        drawPreviewMesh(model, view, projection);
    }

    // -- viewport manipulator (drag handles, drawn on top of bodies) ------
    if (m_manipulator && m_manipulator->isVisible() && m_edgeProgram) {
        m_manipulator->draw(this, m_edgeProgram, view, projection);
    }

    // -- sketch overlay (drawn on top of 3D scene) -----------------------
    if (m_sketchEditor) {
        drawSketchOverlay();
        drawSketchConstraintOverlay();
        drawSketchSnapAndDimensionOverlay();
    }

    // -- ViewCube overlay (top-right corner) ------------------------------
    drawViewCubeOverlay();

    // -- manipulator 2D overlay (value label, flip arrow) ----------------
    drawManipulatorOverlay();
}

// =============================================================================
// Shader compilation
// =============================================================================

void Viewport3D::buildShaderProgram()
{
    m_program = new QOpenGLShaderProgram(this);
    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShaderSrc)) {
        qWarning("Viewport3D: vertex shader compile error:\n%s",
                 qPrintable(m_program->log()));
    }
    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShaderSrc)) {
        qWarning("Viewport3D: fragment shader compile error:\n%s",
                 qPrintable(m_program->log()));
    }
    if (!m_program->link()) {
        qWarning("Viewport3D: shader link error:\n%s",
                 qPrintable(m_program->log()));
    }
}

void Viewport3D::buildPickShaderProgram()
{
    m_pickProgram = new QOpenGLShaderProgram(this);
    if (!m_pickProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, kPickVertexShaderSrc)) {
        qWarning("Viewport3D: pick vertex shader compile error:\n%s",
                 qPrintable(m_pickProgram->log()));
    }
    if (!m_pickProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, kPickFragmentShaderSrc)) {
        qWarning("Viewport3D: pick fragment shader compile error:\n%s",
                 qPrintable(m_pickProgram->log()));
    }
    if (!m_pickProgram->link()) {
        qWarning("Viewport3D: pick shader link error:\n%s",
                 qPrintable(m_pickProgram->log()));
    }
}

void Viewport3D::buildEdgeShaderProgram()
{
    m_edgeProgram = new QOpenGLShaderProgram(this);
    if (!m_edgeProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, kEdgeVertexShaderSrc)) {
        qWarning("Viewport3D: edge vertex shader compile error:\n%s",
                 qPrintable(m_edgeProgram->log()));
    }
    if (!m_edgeProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, kEdgeFragmentShaderSrc)) {
        qWarning("Viewport3D: edge fragment shader compile error:\n%s",
                 qPrintable(m_edgeProgram->log()));
    }
    if (!m_edgeProgram->link()) {
        qWarning("Viewport3D: edge shader link error:\n%s",
                 qPrintable(m_edgeProgram->log()));
    }
}

// =============================================================================
// View mode
// =============================================================================

// =============================================================================
// Section quad shader -- flat-color with alpha for the cut plane visual
// =============================================================================

static const char* const kSectionQuadVertSrc = R"glsl(
#version 330 core

layout(location = 0) in vec3 aPos;

uniform mat4 uMVP;

void main()
{
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)glsl";

static const char* const kSectionQuadFragSrc = R"glsl(
#version 330 core

uniform vec4 uQuadColor;

out vec4 FragColor;

void main()
{
    FragColor = uQuadColor;
}
)glsl";

void Viewport3D::buildSectionQuadShader()
{
    m_sectionQuadProgram = new QOpenGLShaderProgram(this);
    m_sectionQuadProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, kSectionQuadVertSrc);
    m_sectionQuadProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, kSectionQuadFragSrc);
    if (!m_sectionQuadProgram->link()) {
        qWarning("Viewport3D: section quad shader link error:\n%s",
                 qPrintable(m_sectionQuadProgram->log()));
    }

    m_sectionQuadVao.create();
    m_sectionQuadVbo.create();
}

void Viewport3D::drawSectionQuad(const QMatrix4x4& viewMat, const QMatrix4x4& projMat)
{
    QVector3D normal(m_clipPlane.x(), m_clipPlane.y(), m_clipPlane.z());
    float d = m_clipPlane.w();
    float nLen = normal.length();
    if (nLen < 1e-6f) return;
    normal /= nLen;

    QVector3D center = -normal * d;

    QVector3D up(0, 1, 0);
    if (std::abs(QVector3D::dotProduct(normal, up)) > 0.99f)
        up = QVector3D(1, 0, 0);
    QVector3D tangent = QVector3D::crossProduct(normal, up).normalized();
    QVector3D bitangent = QVector3D::crossProduct(normal, tangent).normalized();

    float halfSize = 50.0f;
    if (m_bodiesLoaded || m_meshLoaded) {
        QVector3D diag = m_bboxMax - m_bboxMin;
        halfSize = diag.length() * 0.75f;
        QVector3D bboxCenter = (m_bboxMin + m_bboxMax) * 0.5f;
        float dist = QVector3D::dotProduct(bboxCenter, normal) + d;
        center = bboxCenter - normal * dist;
    }

    QVector3D v0 = center - tangent * halfSize - bitangent * halfSize;
    QVector3D v1 = center + tangent * halfSize - bitangent * halfSize;
    QVector3D v2 = center + tangent * halfSize + bitangent * halfSize;
    QVector3D v3 = center - tangent * halfSize + bitangent * halfSize;

    float quadVerts[] = {
        v0.x(), v0.y(), v0.z(),
        v1.x(), v1.y(), v1.z(),
        v2.x(), v2.y(), v2.z(),
        v0.x(), v0.y(), v0.z(),
        v2.x(), v2.y(), v2.z(),
        v3.x(), v3.y(), v3.z(),
    };

    m_sectionQuadVao.bind();
    m_sectionQuadVbo.bind();
    m_sectionQuadVbo.allocate(quadVerts, static_cast<int>(sizeof(quadVerts)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    m_sectionQuadVbo.release();
    m_sectionQuadVao.release();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    m_sectionQuadProgram->bind();

    QMatrix4x4 modelMat;
    QMatrix4x4 mvp = projMat * viewMat * modelMat;
    m_sectionQuadProgram->setUniformValue("uMVP", mvp);
    m_sectionQuadProgram->setUniformValue("uQuadColor", QVector4D(0.3f, 0.6f, 0.9f, 0.25f));

    m_sectionQuadVao.bind();
    glDrawArrays(GL_TRIANGLES, 0, 6);
    m_sectionQuadVao.release();

    m_sectionQuadProgram->release();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Viewport3D::setSectionPlane(bool enabled, float planeX, float planeY, float planeZ, float planeD)
{
    m_clipEnabled = enabled;
    if (enabled) {
        m_clipPlane = QVector4D(planeX, planeY, planeZ, planeD);
    }
    update();
}

void Viewport3D::setViewMode(ViewMode mode)
{
    if (m_viewMode != mode) {
        m_viewMode = mode;
        update();
    }
}

ViewMode Viewport3D::viewMode() const
{
    return m_viewMode;
}

// =============================================================================
// Mesh upload
// =============================================================================

void Viewport3D::setMesh(const std::vector<float>& vertices,
                         const std::vector<float>& normals,
                         const std::vector<uint32_t>& indices)
{
    if (vertices.empty() || indices.empty())
        return;

    makeCurrent();

    // Store vertex data for later unproject
    m_vertexData = vertices;

    // -- compute bounding box --------------------------------------------
    m_bboxMin = QVector3D( std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::max());
    m_bboxMax = QVector3D(-std::numeric_limits<float>::max(),
                          -std::numeric_limits<float>::max(),
                          -std::numeric_limits<float>::max());

    const size_t vertCount = vertices.size() / 3;
    for (size_t i = 0; i < vertCount; ++i) {
        const float x = vertices[i * 3 + 0];
        const float y = vertices[i * 3 + 1];
        const float z = vertices[i * 3 + 2];
        m_bboxMin.setX(std::min(m_bboxMin.x(), x));
        m_bboxMin.setY(std::min(m_bboxMin.y(), y));
        m_bboxMin.setZ(std::min(m_bboxMin.z(), z));
        m_bboxMax.setX(std::max(m_bboxMax.x(), x));
        m_bboxMax.setY(std::max(m_bboxMax.y(), y));
        m_bboxMax.setZ(std::max(m_bboxMax.z(), z));
    }

    // -- build per-vertex face ID array (default: all -1) ----------------
    // If setFaceMap has been called with faceIds, we expand per-triangle IDs
    // to per-vertex IDs. Otherwise default face ID = triangle_index / 1
    // (each triangle gets its own unique ID based on triangle index).
    const size_t triCount = indices.size() / 3;
    std::vector<float> faceIdPerVertex(vertCount, -1.0f);

    if (m_faceMapLoaded && m_faceIdPerTriangle.size() == triCount) {
        // Assign per-vertex face ID from the per-triangle map.
        // If a vertex is shared by multiple faces, last-write-wins (acceptable
        // for picking since adjacent faces share edge vertices).
        for (size_t t = 0; t < triCount; ++t) {
            float fid = static_cast<float>(m_faceIdPerTriangle[t]);
            faceIdPerVertex[indices[t * 3 + 0]] = fid;
            faceIdPerVertex[indices[t * 3 + 1]] = fid;
            faceIdPerVertex[indices[t * 3 + 2]] = fid;
        }
    } else {
        // Auto-generate: every triangle is its own "face"
        for (size_t t = 0; t < triCount; ++t) {
            float fid = static_cast<float>(t);
            faceIdPerVertex[indices[t * 3 + 0]] = fid;
            faceIdPerVertex[indices[t * 3 + 1]] = fid;
            faceIdPerVertex[indices[t * 3 + 2]] = fid;
        }
    }

    // -- upload to GPU ---------------------------------------------------
    m_vao.bind();

    // positions -> location 0
    m_vboPos.bind();
    m_vboPos.allocate(vertices.data(),
                      static_cast<int>(vertices.size() * sizeof(float)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    m_vboPos.release();

    // normals -> location 1
    m_vboNorm.bind();
    m_vboNorm.allocate(normals.data(),
                       static_cast<int>(normals.size() * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    m_vboNorm.release();

    // face IDs -> location 2
    m_vboFaceId.bind();
    m_vboFaceId.allocate(faceIdPerVertex.data(),
                         static_cast<int>(faceIdPerVertex.size() * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(float), nullptr);
    m_vboFaceId.release();

    // index buffer
    m_ebo.bind();
    m_ebo.allocate(indices.data(),
                   static_cast<int>(indices.size() * sizeof(uint32_t)));
    // Note: EBO stays bound inside the VAO -- do NOT release it here.

    m_vao.release();

    m_indexCount = static_cast<GLsizei>(indices.size());
    m_meshLoaded = true;

    doneCurrent();

    fitAll();
    update();   // schedule repaint
}

void Viewport3D::setFaceMap(const std::vector<int>& faceIds,
                            const std::vector<std::string>& bodyIds)
{
    m_faceIdPerTriangle = faceIds;
    m_bodyIdPerFace = bodyIds;
    m_faceMapLoaded = true;
}

void Viewport3D::setEdges(const std::vector<float>& edgeVertices,
                           const std::vector<uint32_t>& edgeIndices)
{
    if (edgeVertices.empty() || edgeIndices.empty()) {
        m_edgesLoaded = false;
        return;
    }

    makeCurrent();

    m_edgeVao.bind();

    // Edge positions -> location 0
    m_edgeVboPos.bind();
    m_edgeVboPos.allocate(edgeVertices.data(),
                          static_cast<int>(edgeVertices.size() * sizeof(float)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    m_edgeVboPos.release();

    // Edge index buffer
    m_edgeEbo.bind();
    m_edgeEbo.allocate(edgeIndices.data(),
                       static_cast<int>(edgeIndices.size() * sizeof(uint32_t)));
    // EBO stays bound inside the VAO

    m_edgeVao.release();

    m_edgeIndexCount = static_cast<GLsizei>(edgeIndices.size());
    m_edgesLoaded = true;

    doneCurrent();
    update();
}

// =============================================================================
// Per-body color palette
// =============================================================================

const float Viewport3D::kBodyColors[][3] = {
    {0.6f, 0.65f, 0.7f},   // steel blue-gray (default)
    {0.7f, 0.5f, 0.3f},    // bronze
    {0.3f, 0.7f, 0.4f},    // green
    {0.7f, 0.3f, 0.3f},    // red
    {0.4f, 0.4f, 0.7f},    // purple
    {0.7f, 0.7f, 0.3f},    // gold
};

// =============================================================================
// Multi-body upload
// =============================================================================

void Viewport3D::setBodies(const std::vector<BodyRenderData>& bodies)
{
    makeCurrent();

    // Destroy old per-body GPU resources
    for (auto& bg : m_bodyGPUs) {
        bg->vao.destroy();
        bg->vboPos.destroy();
        bg->vboNorm.destroy();
        bg->vboFaceId.destroy();
        bg->ebo.destroy();
        bg->edgeVao.destroy();
        bg->edgeVboPos.destroy();
        bg->edgeEbo.destroy();
    }
    m_bodyGPUs.clear();

    if (bodies.empty()) {
        m_bodiesLoaded = false;
        m_meshLoaded = false;
        m_edgesLoaded = false;
        doneCurrent();
        return;
    }

    // Compute global bounding box and build face map across all bodies
    m_bboxMin = QVector3D( std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::max());
    m_bboxMax = QVector3D(-std::numeric_limits<float>::max(),
                          -std::numeric_limits<float>::max(),
                          -std::numeric_limits<float>::max());

    // Also rebuild the merged face map for picking (face index -> body ID)
    m_faceIdPerTriangle.clear();
    m_bodyIdPerFace.clear();
    m_vertexData.clear();

    int globalFaceBase = 0;  // running face index offset across all bodies

    for (size_t bi = 0; bi < bodies.size(); ++bi) {
        const auto& body = bodies[bi];

        // Update global bounding box
        const size_t vertCount = body.vertices.size() / 3;
        for (size_t i = 0; i < vertCount; ++i) {
            const float x = body.vertices[i * 3 + 0];
            const float y = body.vertices[i * 3 + 1];
            const float z = body.vertices[i * 3 + 2];
            m_bboxMin.setX(std::min(m_bboxMin.x(), x));
            m_bboxMin.setY(std::min(m_bboxMin.y(), y));
            m_bboxMin.setZ(std::min(m_bboxMin.z(), z));
            m_bboxMax.setX(std::max(m_bboxMax.x(), x));
            m_bboxMax.setY(std::max(m_bboxMax.y(), y));
            m_bboxMax.setZ(std::max(m_bboxMax.z(), z));
        }

        // Append to merged vertex data (for unproject)
        m_vertexData.insert(m_vertexData.end(),
                            body.vertices.begin(), body.vertices.end());

        // Find the max face ID in this body's faceIds to know how many faces
        int maxFaceInBody = -1;
        const size_t triCount = body.indices.size() / 3;
        for (size_t t = 0; t < triCount; ++t) {
            int fid = (t < body.faceIds.size())
                      ? static_cast<int>(body.faceIds[t]) : static_cast<int>(t);
            if (fid > maxFaceInBody) maxFaceInBody = fid;
        }

        // Build per-vertex face ID array (with global offset)
        std::vector<float> faceIdPerVertex(vertCount, -1.0f);
        for (size_t t = 0; t < triCount; ++t) {
            int localFace = (t < body.faceIds.size())
                            ? static_cast<int>(body.faceIds[t]) : static_cast<int>(t);
            int globalFace = localFace + globalFaceBase;
            float fid = static_cast<float>(globalFace);
            faceIdPerVertex[body.indices[t * 3 + 0]] = fid;
            faceIdPerVertex[body.indices[t * 3 + 1]] = fid;
            faceIdPerVertex[body.indices[t * 3 + 2]] = fid;

            m_faceIdPerTriangle.push_back(globalFace);
        }

        // Build bodyIdPerFace entries for new face indices
        for (int f = 0; f <= maxFaceInBody; ++f) {
            m_bodyIdPerFace.push_back(body.bodyId);
        }

        globalFaceBase += (maxFaceInBody + 1);

        // Create GPU resources for this body
        auto bg = std::make_unique<BodyGPU>();
        bg->bodyId = body.bodyId;
        bg->colorR = body.colorR;
        bg->colorG = body.colorG;
        bg->colorB = body.colorB;
        bg->isVisible = body.isVisible;
        bg->hasError  = body.hasError;

        bg->vao.create();
        bg->vboPos.create();
        bg->vboNorm.create();
        bg->vboFaceId.create();
        bg->ebo.create();

        bg->vao.bind();

        // positions -> location 0
        bg->vboPos.bind();
        bg->vboPos.allocate(body.vertices.data(),
                            static_cast<int>(body.vertices.size() * sizeof(float)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        bg->vboPos.release();

        // normals -> location 1
        bg->vboNorm.bind();
        bg->vboNorm.allocate(body.normals.data(),
                             static_cast<int>(body.normals.size() * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        bg->vboNorm.release();

        // face IDs -> location 2
        bg->vboFaceId.bind();
        bg->vboFaceId.allocate(faceIdPerVertex.data(),
                               static_cast<int>(faceIdPerVertex.size() * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(float), nullptr);
        bg->vboFaceId.release();

        // index buffer
        bg->ebo.bind();
        bg->ebo.allocate(body.indices.data(),
                         static_cast<int>(body.indices.size() * sizeof(uint32_t)));

        bg->vao.release();
        bg->indexCount = static_cast<GLsizei>(body.indices.size());

        // Edge buffers
        if (!body.edgeVertices.empty() && !body.edgeIndices.empty()) {
            bg->edgeVao.create();
            bg->edgeVboPos.create();
            bg->edgeEbo.create();

            bg->edgeVao.bind();

            bg->edgeVboPos.bind();
            bg->edgeVboPos.allocate(body.edgeVertices.data(),
                                    static_cast<int>(body.edgeVertices.size() * sizeof(float)));
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
            bg->edgeVboPos.release();

            bg->edgeEbo.bind();
            bg->edgeEbo.allocate(body.edgeIndices.data(),
                                 static_cast<int>(body.edgeIndices.size() * sizeof(uint32_t)));

            bg->edgeVao.release();
            bg->edgeIndexCount = static_cast<GLsizei>(body.edgeIndices.size());
        }

        m_bodyGPUs.push_back(std::move(bg));
    }

    m_faceMapLoaded = true;
    m_bodiesLoaded = true;
    // Mark legacy single-mesh as not loaded so paintGL uses per-body path
    m_meshLoaded = false;
    m_edgesLoaded = false;

    doneCurrent();
    fitAll();
    update();
}

// =============================================================================
// fitAll -- frame the bounding box
// =============================================================================

void Viewport3D::fitAll()
{
    if (!m_meshLoaded && !m_bodiesLoaded)
        return;

    const QVector3D center = (m_bboxMin + m_bboxMax) * 0.5f;
    const float     radius = (m_bboxMax - m_bboxMin).length() * 0.5f;

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
// Projection matrix helper (perspective / orthographic)
// =============================================================================

void Viewport3D::buildProjectionMatrix(QMatrix4x4& out) const
{
    out.setToIdentity();
    const float aspect = static_cast<float>(width()) /
                         std::max(1.0f, static_cast<float>(height()));
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
// Standard view helper
// =============================================================================

void Viewport3D::setStandardView(const QVector3D& direction, const QVector3D& up)
{
    QVector3D targetEye = m_center + direction * m_orbitDistance;
    animateTo(targetEye, m_center, up, 300);
}

void Viewport3D::setStandardView(StandardView view)
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
// Selection manager
// =============================================================================

void Viewport3D::setSelectionManager(SelectionManager* mgr)
{
    m_selectionMgr = mgr;
}

void Viewport3D::setManipulator(ViewportManipulator* manipulator)
{
    m_manipulator = manipulator;
}

void Viewport3D::setHighlightedFaces(const std::vector<int>& faceIndices)
{
    m_highlightedFaces = faceIndices;
    update();
}

// =============================================================================
// GPU picking
// =============================================================================

void Viewport3D::ensurePickFBO()
{
    const int w = width();
    const int h = height();

    if (m_pickFBO && m_pickFBO->width() == w && m_pickFBO->height() == h)
        return;

    // Create a non-multisampled FBO for pixel-exact color reads
    QOpenGLFramebufferObjectFormat fmt;
    fmt.setAttachment(QOpenGLFramebufferObject::Depth);
    fmt.setSamples(0);  // no MSAA -- we need exact pixel values
    m_pickFBO = std::make_unique<QOpenGLFramebufferObject>(w, h, fmt);
}

int Viewport3D::pickAtScreenPos(const QPoint& pos)
{
    if (!m_meshLoaded && !m_bodiesLoaded)
        return 0;
    if (!m_pickProgram)
        return 0;

    makeCurrent();
    ensurePickFBO();

    m_pickFBO->bind();

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);  // background = id 0 = miss
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_MULTISAMPLE);

    m_pickProgram->bind();

    // -- matrices (same as paintGL) ------------------------------------
    QMatrix4x4 model;
    QMatrix4x4 view;
    view.lookAt(m_eye, m_center, m_up);

    QMatrix4x4 projection;
    buildProjectionMatrix(projection);

    m_pickProgram->setUniformValue("uModel",      model);
    m_pickProgram->setUniformValue("uView",        view);
    m_pickProgram->setUniformValue("uProjection",  projection);

    // -- draw to pick FBO -----------------------------------------------
    if (m_bodiesLoaded) {
        for (const auto& bg : m_bodyGPUs) {
            if (!bg->isVisible)
                continue;
            bg->vao.bind();
            glDrawElements(GL_TRIANGLES, bg->indexCount, GL_UNSIGNED_INT, nullptr);
            bg->vao.release();
        }
    } else {
        m_vao.bind();
        glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
        m_vao.release();
    }

    m_pickProgram->release();

    // -- read pixel at mouse position ------------------------------------
    // Qt screen coords have Y=0 at top; OpenGL FBO has Y=0 at bottom.
    int px = pos.x();
    int py = m_pickFBO->height() - pos.y() - 1;

    // Clamp to FBO bounds
    px = std::clamp(px, 0, m_pickFBO->width() - 1);
    py = std::clamp(py, 0, m_pickFBO->height() - 1);

    unsigned char pixel[4] = {0, 0, 0, 0};
    glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);

    // Also read the depth buffer value at this pixel for world-position recovery
    float depthVal = 1.0f;
    glReadPixels(px, py, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depthVal);
    m_lastPickDepth = depthVal;

    m_pickFBO->release();

    glEnable(GL_MULTISAMPLE);
    doneCurrent();

    // Decode face ID from RGB
    int id = pixel[0] | (pixel[1] << 8) | (pixel[2] << 16);
    return id;  // 0 = miss, else face_index + 1
}

void Viewport3D::handlePick(const QPoint& screenPos, bool addToSelection)
{
    if (!m_selectionMgr)
        return;

    int pickId = pickAtScreenPos(screenPos);

    if (pickId <= 0) {
        // Clicked on background -- clear selection
        if (!addToSelection)
            m_selectionMgr->clearSelection();
        setHighlightedFaces({});
        return;
    }

    int faceIndex = pickId - 1;

    // Build a SelectionHit
    SelectionHit hit;
    hit.faceIndex = faceIndex;

    // Look up body ID from face map
    if (m_faceMapLoaded &&
        faceIndex >= 0 &&
        static_cast<size_t>(faceIndex) < m_bodyIdPerFace.size())
    {
        hit.bodyId = m_bodyIdPerFace[static_cast<size_t>(faceIndex)];
    }

    // Compute accurate world position by unprojecting the screen position
    // using the depth value read from the pick FBO's depth buffer.
    QMatrix4x4 view;
    view.lookAt(m_eye, m_center, m_up);
    QMatrix4x4 projection;
    buildProjectionMatrix(projection);
    QMatrix4x4 mvp = projection * view;
    QMatrix4x4 invMvp = mvp.inverted();

    // NDC coordinates: map screen pos to [-1,1] and depth [0,1] -> [-1,1]
    float ndcX = (2.0f * screenPos.x()) / width() - 1.0f;
    float ndcY = 1.0f - (2.0f * screenPos.y()) / height();
    float ndcZ = m_lastPickDepth * 2.0f - 1.0f;  // GL depth [0,1] -> NDC [-1,1]

    QVector4D clipPt(ndcX, ndcY, ndcZ, 1.0f);
    QVector4D worldPt = invMvp * clipPt;
    if (std::abs(worldPt.w()) > 1e-7f)
        worldPt /= worldPt.w();

    QVector3D hitPos = worldPt.toVector3D();
    hit.worldX = hitPos.x();
    hit.worldY = hitPos.y();
    hit.worldZ = hitPos.z();
    hit.depth  = (m_eye - hitPos).length();

    if (addToSelection) {
        m_selectionMgr->addToSelection(hit);
    } else {
        m_selectionMgr->select(hit);
    }

    // Update highlighted faces from current selection
    std::vector<int> highlighted;
    for (const auto& sel : m_selectionMgr->selection()) {
        if (sel.faceIndex >= 0)
            highlighted.push_back(sel.faceIndex);
    }
    setHighlightedFaces(highlighted);
}

void Viewport3D::handlePreSelection(const QPoint& screenPos)
{
    if (!m_selectionMgr)
        return;

    int pickId = pickAtScreenPos(screenPos);

    if (pickId <= 0) {
        m_selectionMgr->clearPreSelection();
        update();
        return;
    }

    int faceIndex = pickId - 1;

    SelectionHit hit;
    hit.faceIndex = faceIndex;

    if (m_faceMapLoaded &&
        faceIndex >= 0 &&
        static_cast<size_t>(faceIndex) < m_bodyIdPerFace.size())
    {
        hit.bodyId = m_bodyIdPerFace[static_cast<size_t>(faceIndex)];
    }

    m_selectionMgr->setPreSelection(hit);
    update();
}

// =============================================================================
// Arcball helper
// =============================================================================

QVector3D Viewport3D::arcballVector(const QPoint& screenPos) const
{
    const float w = static_cast<float>(width());
    const float h = static_cast<float>(height());

    // Map to [-1, 1]
    float x =  (2.0f * screenPos.x() - w) / w;
    float y = -(2.0f * screenPos.y() - h) / h;   // flip Y

    float lenSq = x * x + y * y;
    float z;
    if (lenSq <= 1.0f)
        z = std::sqrt(1.0f - lenSq);
    else {
        // outside the sphere -- project onto it
        float len = std::sqrt(lenSq);
        x /= len;
        y /= len;
        z = 0.0f;
    }
    return QVector3D(x, y, z);
}

// =============================================================================
// Mouse interaction
// =============================================================================

void Viewport3D::mousePressEvent(QMouseEvent* event)
{
    // Delegate to sketch editor first
    if (m_sketchEditor && m_sketchEditor->isEditing()) {
        if (m_sketchEditor->handleMousePress(event)) {
            event->accept();
            return;
        }
    }

    // Delegate to viewport manipulator (drag handles)
    if (event->button() == Qt::LeftButton && m_manipulator && m_manipulator->isVisible()) {
        QMatrix4x4 v = viewMatrix();
        QMatrix4x4 p = projectionMatrix();
        if (m_manipulator->handleMousePress(event->pos(), v, p, width(), height())) {
            event->accept();
            update();
            return;
        }
    }

    // Check ViewCube click (top-right corner)
    if (event->button() == Qt::LeftButton && handleViewCubeClick(event->pos())) {
        event->accept();
        return;
    }

    m_lastMousePos = event->pos();
    m_mousePressPos = event->pos();
    m_activeButton = event->button();
    m_isDragging = false;
    event->accept();
}

void Viewport3D::mouseReleaseEvent(QMouseEvent* event)
{
    // Delegate to sketch editor first -- suppress 3D picking during sketch editing
    if (m_sketchEditor && m_sketchEditor->isEditing()) {
        m_sketchEditor->handleMouseRelease(event);
        m_activeButton = Qt::NoButton;
        m_isDragging = false;
        event->accept();
        return;
    }

    // Delegate to viewport manipulator
    if (m_manipulator && m_manipulator->isDragging()) {
        m_manipulator->handleMouseRelease();
        m_activeButton = Qt::NoButton;
        m_isDragging = false;
        event->accept();
        update();
        return;
    }

    if (event->button() == Qt::LeftButton && !m_isDragging) {
        // Single click without drag -- perform a pick
        bool shiftHeld = event->modifiers() & Qt::ShiftModifier;
        handlePick(event->pos(), shiftHeld);
    }

    m_activeButton = Qt::NoButton;
    m_isDragging = false;
    event->accept();
}

void Viewport3D::mouseMoveEvent(QMouseEvent* event)
{
    // Delegate to sketch editor first
    if (m_sketchEditor && m_sketchEditor->isEditing()) {
        // Always forward to sketch editor for rubber-band tracking
        m_sketchEditor->handleMouseMove(event);
        // During sketch editing, suppress left-button orbit --
        // only allow middle-button pan and wheel zoom.
        if (m_activeButton == Qt::LeftButton) {
            m_lastMousePos = event->pos();
            event->accept();
            return;
        }
    }

    // Delegate to viewport manipulator for drag tracking and hover
    if (m_manipulator && m_manipulator->isVisible()) {
        QMatrix4x4 v = viewMatrix();
        QMatrix4x4 p = projectionMatrix();
        if (m_manipulator->handleMouseMove(event->pos(), v, p, width(), height())) {
            m_lastMousePos = event->pos();
            event->accept();
            update();
            return;
        }
        // Even if not consumed (hover), trigger repaint for hover feedback
        update();
    }

    const QPoint pos = event->pos();

    // Check if we are dragging (exceeded threshold)
    if (m_activeButton != Qt::NoButton && !m_isDragging) {
        int dx = pos.x() - m_mousePressPos.x();
        int dy = pos.y() - m_mousePressPos.y();
        if (dx * dx + dy * dy > kDragThreshold * kDragThreshold)
            m_isDragging = true;
    }

    if (m_activeButton == Qt::LeftButton && m_isDragging &&
        (event->modifiers() & Qt::ControlModifier) && m_selectionMgr && m_selectionMgr->hasSelection()) {
        // -- Ctrl+drag: move selected body (occurrence drag) -----------------
        const float dx = static_cast<float>(pos.x() - m_lastMousePos.x());
        const float dy = static_cast<float>(pos.y() - m_lastMousePos.y());

        QVector3D forward = (m_center - m_eye).normalized();
        QVector3D right   = QVector3D::crossProduct(forward, m_up).normalized();
        QVector3D camUp   = QVector3D::crossProduct(right, forward).normalized();

        const float moveSpeed = m_orbitDistance * 0.002f;
        QVector3D delta = (dx * right - dy * camUp) * moveSpeed;

        emit occurrenceDragged(delta.x(), delta.y(), delta.z());
    }
    else if (m_activeButton == Qt::LeftButton && m_isDragging) {
        // -- Arcball rotation ------------------------------------------------
        QVector3D va = arcballVector(m_lastMousePos);
        QVector3D vb = arcballVector(pos);

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
        }
    }
    else if (m_activeButton == Qt::MiddleButton) {
        // -- Pan -------------------------------------------------------------
        const float dx = static_cast<float>(pos.x() - m_lastMousePos.x());
        const float dy = static_cast<float>(pos.y() - m_lastMousePos.y());

        // Compute camera right/up vectors
        QVector3D forward = (m_center - m_eye).normalized();
        QVector3D right   = QVector3D::crossProduct(forward, m_up).normalized();
        QVector3D camUp   = QVector3D::crossProduct(right, forward).normalized();

        // Scale pan speed proportional to orbit distance
        const float panSpeed = m_orbitDistance * 0.002f;
        QVector3D shift = (-dx * right + dy * camUp) * panSpeed;

        m_eye    += shift;
        m_center += shift;
    }
    else if (m_activeButton == Qt::NoButton) {
        // No button pressed -- perform pre-selection (hover highlight)
        handlePreSelection(pos);
    }

    m_lastMousePos = pos;
    update();
    event->accept();
}

void Viewport3D::wheelEvent(QWheelEvent* event)
{
    // angleDelta().y() is typically +/-120 per notch
    const float delta = static_cast<float>(event->angleDelta().y()) / 120.0f;
    const float factor = 1.0f - delta * 0.1f;           // 10 % per notch

    QVector3D direction = (m_eye - m_center).normalized();
    m_orbitDistance *= factor;
    m_orbitDistance  = std::max(m_near * 2.0f, m_orbitDistance);  // clamp
    m_eye = m_center + direction * m_orbitDistance;

    update();
    event->accept();
}

void Viewport3D::keyPressEvent(QKeyEvent* event)
{
    if (m_sketchEditor && m_sketchEditor->isEditing()) {
        if (m_sketchEditor->handleKeyPress(event)) {
            event->accept();
            return;
        }
    }

    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    const QVector3D upY(0.0f, 1.0f, 0.0f);
    const QVector3D upZ(0.0f, 0.0f, 1.0f);

    switch (event->key()) {
    // Numpad 1: Front (look along -Y) / Ctrl: Back (look along +Y)
    case Qt::Key_1:
        if (ctrl)
            setStandardView(QVector3D(0, -1, 0), upZ);   // Back
        else
            setStandardView(QVector3D(0,  1, 0), upZ);   // Front
        event->accept();
        return;

    // Numpad 3: Right (look along -X) / Ctrl: Left (look along +X)
    case Qt::Key_3:
        if (ctrl)
            setStandardView(QVector3D( 1, 0, 0), upZ);   // Left
        else
            setStandardView(QVector3D(-1, 0, 0), upZ);   // Right
        event->accept();
        return;

    // Numpad 7: Top (look along -Z) / Ctrl: Bottom (look along +Z)
    case Qt::Key_7:
        if (ctrl)
            setStandardView(QVector3D(0, 0, -1), -upY);  // Bottom
        else
            setStandardView(QVector3D(0, 0,  1),  upY);  // Top
        event->accept();
        return;

    // Numpad 5: Toggle perspective / orthographic
    case Qt::Key_5:
        m_perspectiveProjection = !m_perspectiveProjection;
        update();
        event->accept();
        return;

    // Numpad 0: Isometric view
    case Qt::Key_0:
        setStandardView(StandardView::Isometric);
        event->accept();
        return;

    // Home / F: fitAll
    case Qt::Key_Home:
    case Qt::Key_F:
        fitAll();
        event->accept();
        return;

    default:
        break;
    }

    QOpenGLWidget::keyPressEvent(event);
}

// =============================================================================
// Sketch editor integration
// =============================================================================

void Viewport3D::setSketchEditor(SketchEditor* editor)
{
    m_sketchEditor = editor;
    update();
}

QMatrix4x4 Viewport3D::viewMatrix() const
{
    QMatrix4x4 v;
    v.lookAt(m_eye, m_center, m_up);
    return v;
}

QMatrix4x4 Viewport3D::projectionMatrix() const
{
    QMatrix4x4 p;
    buildProjectionMatrix(p);
    return p;
}

void Viewport3D::buildSketchOverlayShader()
{
    m_sketchOverlayProgram = new QOpenGLShaderProgram(this);
    if (!m_sketchOverlayProgram->addShaderFromSourceCode(
            QOpenGLShader::Vertex, kSketchOverlayVertSrc)) {
        qWarning("Viewport3D: sketch overlay vertex shader error:\n%s",
                 qPrintable(m_sketchOverlayProgram->log()));
    }
    if (!m_sketchOverlayProgram->addShaderFromSourceCode(
            QOpenGLShader::Fragment, kSketchOverlayFragSrc)) {
        qWarning("Viewport3D: sketch overlay fragment shader error:\n%s",
                 qPrintable(m_sketchOverlayProgram->log()));
    }
    if (!m_sketchOverlayProgram->link()) {
        qWarning("Viewport3D: sketch overlay shader link error:\n%s",
                 qPrintable(m_sketchOverlayProgram->log()));
    }
}

// =============================================================================
// Ground grid & origin axes
// =============================================================================

void Viewport3D::setExplodeFactor(float factor)
{
    m_explodeFactor = std::max(0.0f, std::min(1.0f, factor));
    update();
}

void Viewport3D::setShowGrid(bool show)
{
    m_showGrid = show;
    update();
}

void Viewport3D::initGridBuffers()
{
    // Build vertex data: minor grid lines, major grid lines, then origin axes.
    // All stored in a single VBO, drawn as GL_LINES in separate draw calls.
    // Layout in VBO: [minor lines] [major lines] [axis lines]

    std::vector<float> verts;
    const float extent = 100.0f;

    // --- Minor grid lines (10mm spacing), excluding lines that fall on major ---
    for (float v = -extent; v <= extent; v += 10.0f) {
        // Skip lines that fall on major spacing (multiples of 50)
        if (std::fmod(std::fabs(v), 50.0f) < 0.001f)
            continue;
        // Horizontal line (along X) at Y=v
        verts.push_back(-extent); verts.push_back(v); verts.push_back(0.0f);
        verts.push_back( extent); verts.push_back(v); verts.push_back(0.0f);
        // Vertical line (along Y) at X=v
        verts.push_back(v); verts.push_back(-extent); verts.push_back(0.0f);
        verts.push_back(v); verts.push_back( extent); verts.push_back(0.0f);
    }
    m_gridMinorVertexCount = static_cast<GLsizei>(verts.size() / 3);

    // --- Major grid lines (50mm spacing) ---
    for (float v = -extent; v <= extent; v += 50.0f) {
        // Horizontal line (along X) at Y=v
        verts.push_back(-extent); verts.push_back(v); verts.push_back(0.0f);
        verts.push_back( extent); verts.push_back(v); verts.push_back(0.0f);
        // Vertical line (along Y) at X=v
        verts.push_back(v); verts.push_back(-extent); verts.push_back(0.0f);
        verts.push_back(v); verts.push_back( extent); verts.push_back(0.0f);
    }
    m_gridMajorVertexCount = static_cast<GLsizei>(verts.size() / 3) - m_gridMinorVertexCount;

    // --- Origin axes (three colored lines) ---
    // X axis: (0,0,0) -> (50,0,0)
    verts.push_back(0.0f); verts.push_back(0.0f); verts.push_back(0.0f);
    verts.push_back(50.0f); verts.push_back(0.0f); verts.push_back(0.0f);
    // Y axis: (0,0,0) -> (0,50,0)
    verts.push_back(0.0f); verts.push_back(0.0f); verts.push_back(0.0f);
    verts.push_back(0.0f); verts.push_back(50.0f); verts.push_back(0.0f);
    // Z axis: (0,0,0) -> (0,0,50)
    verts.push_back(0.0f); verts.push_back(0.0f); verts.push_back(0.0f);
    verts.push_back(0.0f); verts.push_back(0.0f); verts.push_back(50.0f);
    m_gridAxesVertexCount = 6; // 3 lines x 2 verts

    // Upload to GPU
    m_gridVao.create();
    m_gridVbo.create();

    m_gridVao.bind();
    m_gridVbo.bind();
    m_gridVbo.allocate(verts.data(), static_cast<int>(verts.size() * sizeof(float)));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    m_gridVbo.release();
    m_gridVao.release();

    m_gridInitialized = true;
}

void Viewport3D::drawGrid(const QMatrix4x4& mvp)
{
    // Compute grid opacity: fade out as camera gets very far
    float alpha = 1.0f;
    if (m_orbitDistance > 200.0f)
        alpha = 0.0f;
    else if (m_orbitDistance > 100.0f)
        alpha = 1.0f - (m_orbitDistance - 100.0f) / 100.0f;

    if (alpha <= 0.0f)
        return;

    m_edgeProgram->bind();
    m_edgeProgram->setUniformValue("uMVP", mvp);
    m_edgeProgram->setUniformValue("uDepthBias", 0.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    m_gridVao.bind();

    // Draw minor grid lines (thin, dark gray #444444)
    glLineWidth(1.0f);
    m_edgeProgram->setUniformValue("uEdgeColor",
        QVector3D(0.267f, 0.267f, 0.267f));  // #444444
    m_edgeProgram->setUniformValue("uEdgeAlpha", alpha);
    glDrawArrays(GL_LINES, 0, m_gridMinorVertexCount);

    // Draw major grid lines (slightly brighter #666666)
    glLineWidth(1.0f);
    m_edgeProgram->setUniformValue("uEdgeColor",
        QVector3D(0.4f, 0.4f, 0.4f));  // #666666
    m_edgeProgram->setUniformValue("uEdgeAlpha", alpha);
    glDrawArrays(GL_LINES, m_gridMinorVertexCount, m_gridMajorVertexCount);

    m_gridVao.release();

    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);
    m_edgeProgram->release();
}

void Viewport3D::drawOriginAxes(const QMatrix4x4& mvp)
{
    m_edgeProgram->bind();
    m_edgeProgram->setUniformValue("uMVP", mvp);
    m_edgeProgram->setUniformValue("uDepthBias", 0.0f);
    m_edgeProgram->setUniformValue("uEdgeAlpha", 1.0f);

    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(2.5f);

    m_gridVao.bind();

    GLint axisStart = m_gridMinorVertexCount + m_gridMajorVertexCount;

    // X axis -- Red
    m_edgeProgram->setUniformValue("uEdgeColor", QVector3D(0.9f, 0.2f, 0.2f));
    glDrawArrays(GL_LINES, axisStart, 2);

    // Y axis -- Green
    m_edgeProgram->setUniformValue("uEdgeColor", QVector3D(0.2f, 0.85f, 0.2f));
    glDrawArrays(GL_LINES, axisStart + 2, 2);

    // Z axis -- Blue
    m_edgeProgram->setUniformValue("uEdgeColor", QVector3D(0.3f, 0.4f, 0.95f));
    glDrawArrays(GL_LINES, axisStart + 4, 2);

    m_gridVao.release();

    glDisable(GL_LINE_SMOOTH);
    glLineWidth(1.5f);
    m_edgeProgram->release();
}

// =============================================================================
// Sketch overlay rendering
// =============================================================================

void Viewport3D::drawSketchOverlay()
{
    if (!m_sketchEditor || !m_sketchOverlayProgram)
        return;

    sketch::Sketch* sk = m_sketchEditor->currentSketch();
    if (!sk)
        return;

    QMatrix4x4 vMat = viewMatrix();
    QMatrix4x4 pMat = projectionMatrix();
    QMatrix4x4 mvp = pMat * vMat;

    m_sketchOverlayProgram->bind();
    m_sketchOverlayProgram->setUniformValue("uMVP", mvp);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    auto skToWorld = [&](double ssx, double ssy) -> QVector3D {
        double wx, wy, wz;
        sk->sketchToWorld(ssx, ssy, wx, wy, wz);
        return QVector3D(static_cast<float>(wx),
                         static_cast<float>(wy),
                         static_cast<float>(wz));
    };

    QOpenGLVertexArrayObject tempVao;
    QOpenGLBuffer tempVbo(QOpenGLBuffer::VertexBuffer);
    tempVao.create();
    tempVbo.create();

    auto uploadAndDraw = [&](const std::vector<float>& verts,
                             const QVector4D& color, GLenum mode) {
        if (verts.empty()) return;
        tempVao.bind();
        tempVbo.bind();
        tempVbo.allocate(verts.data(),
                         static_cast<int>(verts.size() * sizeof(float)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              3 * sizeof(float), nullptr);
        m_sketchOverlayProgram->setUniformValue("uColor", color);
        glDrawArrays(mode, 0, static_cast<GLsizei>(verts.size() / 3));
        tempVbo.release();
        tempVao.release();
    };

    // Colors
    const QVector4D cGreen(0.1f, 0.8f, 0.1f, 1.0f);
    const QVector4D cBlue(0.2f, 0.5f, 1.0f, 1.0f);
    const QVector4D cOrange(0.6f, 0.4f, 0.1f, 0.7f);
    const QVector4D cWhite(1.0f, 1.0f, 1.0f, 1.0f);
    const QVector4D cRubber(0.7f, 0.7f, 0.7f, 0.6f);
    const QVector4D cGrid(0.35f, 0.35f, 0.35f, 0.25f);

    QVector4D lineColor = sk->isFullyConstrained() ? cGreen : cBlue;

    // -- Grid ----------------------------------------------------------------
    {
        std::vector<float> gv;
        for (double g = -100.0; g <= 100.0; g += 10.0) {
            QVector3D h1 = skToWorld(-100, g), h2 = skToWorld(100, g);
            gv.push_back(h1.x()); gv.push_back(h1.y()); gv.push_back(h1.z());
            gv.push_back(h2.x()); gv.push_back(h2.y()); gv.push_back(h2.z());
            QVector3D v1 = skToWorld(g, -100), v2 = skToWorld(g, 100);
            gv.push_back(v1.x()); gv.push_back(v1.y()); gv.push_back(v1.z());
            gv.push_back(v2.x()); gv.push_back(v2.y()); gv.push_back(v2.z());
        }
        glLineWidth(1.0f);
        uploadAndDraw(gv, cGrid, GL_LINES);
    }

    // -- Lines ---------------------------------------------------------------
    {
        std::vector<float> lv, cv;
        for (const auto& [lid, ln] : sk->lines()) {
            const auto& p1 = sk->point(ln.startPointId);
            const auto& p2 = sk->point(ln.endPointId);
            QVector3D w1 = skToWorld(p1.x, p1.y);
            QVector3D w2 = skToWorld(p2.x, p2.y);
            auto& t = ln.isConstruction ? cv : lv;
            t.push_back(w1.x()); t.push_back(w1.y()); t.push_back(w1.z());
            t.push_back(w2.x()); t.push_back(w2.y()); t.push_back(w2.z());
        }
        glLineWidth(2.0f);
        uploadAndDraw(lv, lineColor, GL_LINES);
        uploadAndDraw(cv, cOrange, GL_LINES);
    }

    // -- Circles -------------------------------------------------------------
    {
        for (const auto& [cid, circ] : sk->circles()) {
            const auto& cp = sk->point(circ.centerPointId);
            std::vector<float> cv2;
            constexpr int S = 64;
            for (int i = 0; i < S; ++i) {
                double a = 2.0 * M_PI * i / S;
                QVector3D w = skToWorld(cp.x + circ.radius * std::cos(a),
                                        cp.y + circ.radius * std::sin(a));
                cv2.push_back(w.x()); cv2.push_back(w.y()); cv2.push_back(w.z());
            }
            glLineWidth(2.0f);
            uploadAndDraw(cv2, circ.isConstruction ? cOrange : lineColor,
                          GL_LINE_LOOP);
        }
    }

    // -- Arcs ----------------------------------------------------------------
    {
        for (const auto& [aid, arc] : sk->arcs()) {
            const auto& cp = sk->point(arc.centerPointId);
            const auto& sp = sk->point(arc.startPointId);
            const auto& ep = sk->point(arc.endPointId);
            double sa = std::atan2(sp.y - cp.y, sp.x - cp.x);
            double ea = std::atan2(ep.y - cp.y, ep.x - cp.x);
            if (ea <= sa) ea += 2.0 * M_PI;

            std::vector<float> av;
            constexpr int S = 32;
            for (int i = 0; i < S; ++i) {
                double t1 = static_cast<double>(i) / S;
                double t2 = static_cast<double>(i + 1) / S;
                QVector3D w1 = skToWorld(cp.x + arc.radius * std::cos(sa + t1 * (ea - sa)),
                                         cp.y + arc.radius * std::sin(sa + t1 * (ea - sa)));
                QVector3D w2 = skToWorld(cp.x + arc.radius * std::cos(sa + t2 * (ea - sa)),
                                         cp.y + arc.radius * std::sin(sa + t2 * (ea - sa)));
                av.push_back(w1.x()); av.push_back(w1.y()); av.push_back(w1.z());
                av.push_back(w2.x()); av.push_back(w2.y()); av.push_back(w2.z());
            }
            glLineWidth(2.0f);
            uploadAndDraw(av, arc.isConstruction ? cOrange : lineColor, GL_LINES);
        }
    }

    // -- Ellipses ------------------------------------------------------------
    {
        for (const auto& [eid, ell] : sk->ellipses()) {
            const auto& cp = sk->point(ell.centerPointId);
            std::vector<float> ev;
            constexpr int S = 64;
            double cosA = std::cos(ell.rotationAngle);
            double sinA = std::sin(ell.rotationAngle);
            for (int i = 0; i < S; ++i) {
                double t1 = 2.0 * M_PI * i / S;
                double t2 = 2.0 * M_PI * (i + 1) / S;
                // Parametric ellipse in local frame, then rotate
                auto ellPt = [&](double t) -> QVector3D {
                    double lx = ell.majorRadius * std::cos(t);
                    double ly = ell.minorRadius * std::sin(t);
                    double rx = lx * cosA - ly * sinA;
                    double ry = lx * sinA + ly * cosA;
                    return skToWorld(cp.x + rx, cp.y + ry);
                };
                QVector3D w1 = ellPt(t1);
                QVector3D w2 = ellPt(t2);
                ev.push_back(w1.x()); ev.push_back(w1.y()); ev.push_back(w1.z());
                ev.push_back(w2.x()); ev.push_back(w2.y()); ev.push_back(w2.z());
            }
            glLineWidth(2.0f);
            uploadAndDraw(ev, ell.isConstruction ? cOrange : lineColor, GL_LINES);
        }
    }

    // -- Splines -------------------------------------------------------------
    {
        for (const auto& [sid, spl] : sk->splines()) {
            if (spl.controlPointIds.size() < 2) continue;
            std::vector<float> sv;

            // Gather 2D control points
            std::vector<double> cpx, cpy;
            for (const auto& pid : spl.controlPointIds) {
                const auto& pt = sk->point(pid);
                cpx.push_back(pt.x);
                cpy.push_back(pt.y);
            }
            int n = static_cast<int>(cpx.size());

            // Catmull-Rom subdivision for smooth rendering
            constexpr int SUB = 20;
            for (int seg = 0; seg < n - 1; ++seg) {
                int i0 = std::max(0, seg - 1);
                int i1 = seg;
                int i2 = std::min(n - 1, seg + 1);
                int i3 = std::min(n - 1, seg + 2);

                double p0x = cpx[i0], p0y = cpy[i0];
                double p1x = cpx[i1], p1y = cpy[i1];
                double p2x = cpx[i2], p2y = cpy[i2];
                double p3x = cpx[i3], p3y = cpy[i3];

                for (int s = 0; s < SUB; ++s) {
                    double t1 = static_cast<double>(s) / SUB;
                    double t2 = static_cast<double>(s + 1) / SUB;

                    auto catmull = [](double t, double q0, double q1,
                                      double q2, double q3) -> double {
                        return 0.5 * ((2.0 * q1) +
                                      (-q0 + q2) * t +
                                      (2.0 * q0 - 5.0 * q1 + 4.0 * q2 - q3) * t * t +
                                      (-q0 + 3.0 * q1 - 3.0 * q2 + q3) * t * t * t);
                    };

                    double cx1 = catmull(t1, p0x, p1x, p2x, p3x);
                    double cy1 = catmull(t1, p0y, p1y, p2y, p3y);
                    double cx2 = catmull(t2, p0x, p1x, p2x, p3x);
                    double cy2 = catmull(t2, p0y, p1y, p2y, p3y);

                    QVector3D w1 = skToWorld(cx1, cy1);
                    QVector3D w2 = skToWorld(cx2, cy2);
                    sv.push_back(w1.x()); sv.push_back(w1.y()); sv.push_back(w1.z());
                    sv.push_back(w2.x()); sv.push_back(w2.y()); sv.push_back(w2.z());
                }
            }
            glLineWidth(2.0f);
            uploadAndDraw(sv, spl.isConstruction ? cOrange : lineColor, GL_LINES);
        }
    }

    // -- Points --------------------------------------------------------------
    {
        std::vector<float> pv;
        for (const auto& [pid, pt] : sk->points()) {
            QVector3D w = skToWorld(pt.x, pt.y);
            pv.push_back(w.x()); pv.push_back(w.y()); pv.push_back(w.z());
        }
        uploadAndDraw(pv, cWhite, GL_POINTS);
    }

    // -- Inference lines (snap guides) ----------------------------------------
    {
        const auto& infLines = m_sketchEditor->inferenceLines();
        if (!infLines.empty()) {
            const QVector4D cInfGreen(0.2f, 0.9f, 0.2f, 0.5f);
            const QVector4D cInfCyan(0.2f, 0.9f, 0.9f, 0.6f);

            for (const auto& inf : infLines) {
                std::vector<float> iv;
                if (inf.type == InferenceLine::Midpoint) {
                    // Draw a small cross at the midpoint location
                    const float cs = 2.0f;  // cross half-size in sketch units
                    QVector3D wh1 = skToWorld(inf.x1 - cs, inf.y1);
                    QVector3D wh2 = skToWorld(inf.x1 + cs, inf.y1);
                    QVector3D wv1 = skToWorld(inf.x1, inf.y1 - cs);
                    QVector3D wv2 = skToWorld(inf.x1, inf.y1 + cs);
                    iv = {wh1.x(), wh1.y(), wh1.z(), wh2.x(), wh2.y(), wh2.z(),
                          wv1.x(), wv1.y(), wv1.z(), wv2.x(), wv2.y(), wv2.z()};
                    glLineWidth(1.5f);
                    uploadAndDraw(iv, cInfCyan, GL_LINES);

                    // Draw a diamond marker
                    const float ds = 1.5f;
                    QVector3D wt = skToWorld(inf.x1, inf.y1 + ds);
                    QVector3D wr = skToWorld(inf.x1 + ds, inf.y1);
                    QVector3D wb = skToWorld(inf.x1, inf.y1 - ds);
                    QVector3D wl = skToWorld(inf.x1 - ds, inf.y1);
                    std::vector<float> dv = {
                        wt.x(), wt.y(), wt.z(), wr.x(), wr.y(), wr.z(),
                        wr.x(), wr.y(), wr.z(), wb.x(), wb.y(), wb.z(),
                        wb.x(), wb.y(), wb.z(), wl.x(), wl.y(), wl.z(),
                        wl.x(), wl.y(), wl.z(), wt.x(), wt.y(), wt.z()
                    };
                    glLineWidth(2.0f);
                    uploadAndDraw(dv, cInfCyan, GL_LINES);
                } else {
                    // Horizontal or Vertical inference line
                    QVector3D w1 = skToWorld(inf.x1, inf.y1);
                    QVector3D w2 = skToWorld(inf.x2, inf.y2);
                    iv = {w1.x(), w1.y(), w1.z(), w2.x(), w2.y(), w2.z()};
                    glLineWidth(1.0f);
                    uploadAndDraw(iv, cInfGreen, GL_LINES);
                }
            }
        }
    }

    // -- Rubber-band ---------------------------------------------------------
    if (m_sketchEditor->isDrawingInProgress()) {
        SketchTool tool = m_sketchEditor->currentTool();
        double rx1 = m_sketchEditor->rubberStartX();
        double ry1 = m_sketchEditor->rubberStartY();
        double rx2 = m_sketchEditor->rubberCurrentX();
        double ry2 = m_sketchEditor->rubberCurrentY();

        std::vector<float> rv;
        if (tool == SketchTool::DrawLine) {
            QVector3D w1 = skToWorld(rx1, ry1), w2 = skToWorld(rx2, ry2);
            rv = {w1.x(), w1.y(), w1.z(), w2.x(), w2.y(), w2.z()};
            glLineWidth(1.5f);
            uploadAndDraw(rv, cRubber, GL_LINES);
        } else if (tool == SketchTool::DrawRectangle) {
            QVector3D c0 = skToWorld(rx1, ry1), c1 = skToWorld(rx2, ry1);
            QVector3D c2 = skToWorld(rx2, ry2), c3 = skToWorld(rx1, ry2);
            rv = {c0.x(), c0.y(), c0.z(), c1.x(), c1.y(), c1.z(),
                  c1.x(), c1.y(), c1.z(), c2.x(), c2.y(), c2.z(),
                  c2.x(), c2.y(), c2.z(), c3.x(), c3.y(), c3.z(),
                  c3.x(), c3.y(), c3.z(), c0.x(), c0.y(), c0.z()};
            glLineWidth(1.5f);
            uploadAndDraw(rv, cRubber, GL_LINES);
        } else if (tool == SketchTool::DrawCircle) {
            double rdx = rx2 - rx1, rdy = ry2 - ry1;
            double rr = std::sqrt(rdx * rdx + rdy * rdy);
            constexpr int S = 48;
            for (int i = 0; i < S; ++i) {
                double a1 = 2.0 * M_PI * i / S;
                double a2 = 2.0 * M_PI * (i + 1) / S;
                QVector3D w1 = skToWorld(rx1 + rr * std::cos(a1),
                                         ry1 + rr * std::sin(a1));
                QVector3D w2 = skToWorld(rx1 + rr * std::cos(a2),
                                         ry1 + rr * std::sin(a2));
                rv.push_back(w1.x()); rv.push_back(w1.y()); rv.push_back(w1.z());
                rv.push_back(w2.x()); rv.push_back(w2.y()); rv.push_back(w2.z());
            }
            glLineWidth(1.5f);
            uploadAndDraw(rv, cRubber, GL_LINES);
        } else if (tool == SketchTool::DrawArc) {
            QVector3D wc = skToWorld(rx1, ry1), wp = skToWorld(rx2, ry2);
            rv = {wc.x(), wc.y(), wc.z(), wp.x(), wp.y(), wp.z()};
            glLineWidth(1.5f);
            uploadAndDraw(rv, cRubber, GL_LINES);
        } else if (tool == SketchTool::DrawSpline) {
            // Draw accumulated spline control points as a polyline + current mouse pos
            const auto& splPts = m_sketchEditor->splinePoints();
            if (!splPts.empty()) {
                for (size_t i = 0; i + 1 < splPts.size(); ++i) {
                    QVector3D w1 = skToWorld(splPts[i].first, splPts[i].second);
                    QVector3D w2 = skToWorld(splPts[i+1].first, splPts[i+1].second);
                    rv.push_back(w1.x()); rv.push_back(w1.y()); rv.push_back(w1.z());
                    rv.push_back(w2.x()); rv.push_back(w2.y()); rv.push_back(w2.z());
                }
                // Line from last point to current mouse position
                QVector3D wLast = skToWorld(splPts.back().first, splPts.back().second);
                QVector3D wCur  = skToWorld(rx2, ry2);
                rv.push_back(wLast.x()); rv.push_back(wLast.y()); rv.push_back(wLast.z());
                rv.push_back(wCur.x());  rv.push_back(wCur.y());  rv.push_back(wCur.z());
                glLineWidth(1.5f);
                uploadAndDraw(rv, cRubber, GL_LINES);
            }
        } else if (tool == SketchTool::DrawEllipse) {
            // Ellipse rubber-band: show ellipse with major axis from center to cursor
            double dx = rx2 - rx1, dy = ry2 - ry1;
            double majorR = std::sqrt(dx * dx + dy * dy);
            double minorR = majorR * 0.5;
            double rot = std::atan2(dy, dx);
            double cosA = std::cos(rot), sinA = std::sin(rot);
            constexpr int S = 48;
            for (int i = 0; i < S; ++i) {
                double t1 = 2.0 * M_PI * i / S;
                double t2 = 2.0 * M_PI * (i + 1) / S;
                auto ellPt = [&](double t) -> QVector3D {
                    double lx = majorR * std::cos(t);
                    double ly = minorR * std::sin(t);
                    double ex = lx * cosA - ly * sinA;
                    double ey = lx * sinA + ly * cosA;
                    return skToWorld(rx1 + ex, ry1 + ey);
                };
                QVector3D w1 = ellPt(t1), w2 = ellPt(t2);
                rv.push_back(w1.x()); rv.push_back(w1.y()); rv.push_back(w1.z());
                rv.push_back(w2.x()); rv.push_back(w2.y()); rv.push_back(w2.z());
            }
            glLineWidth(1.5f);
            uploadAndDraw(rv, cRubber, GL_LINES);
        } else if (tool == SketchTool::DrawPolygon) {
            // Polygon rubber-band: show N-sided polygon from center to cursor
            double dx = rx2 - rx1, dy = ry2 - ry1;
            double radius = std::sqrt(dx * dx + dy * dy);
            double baseAngle = std::atan2(dy, dx);
            int n = m_sketchEditor->polygonSides();
            for (int i = 0; i < n; ++i) {
                double a1 = baseAngle + 2.0 * M_PI * i / n;
                double a2 = baseAngle + 2.0 * M_PI * (i + 1) / n;
                QVector3D w1 = skToWorld(rx1 + radius * std::cos(a1),
                                         ry1 + radius * std::sin(a1));
                QVector3D w2 = skToWorld(rx1 + radius * std::cos(a2),
                                         ry1 + radius * std::sin(a2));
                rv.push_back(w1.x()); rv.push_back(w1.y()); rv.push_back(w1.z());
                rv.push_back(w2.x()); rv.push_back(w2.y()); rv.push_back(w2.z());
            }
            glLineWidth(1.5f);
            uploadAndDraw(rv, cRubber, GL_LINES);
        } else if (tool == SketchTool::DrawSlot) {
            // Slot rubber-band: show center1-center2 axis line + cursor
            QVector3D w1 = skToWorld(rx1, ry1), w2 = skToWorld(rx2, ry2);
            rv = {w1.x(), w1.y(), w1.z(), w2.x(), w2.y(), w2.z()};
            glLineWidth(1.5f);
            uploadAndDraw(rv, cRubber, GL_LINES);
        } else if (tool == SketchTool::DrawCircle3Point ||
                   tool == SketchTool::DrawArc3Point) {
            // Show guide lines between clicked points and cursor
            QVector3D wc = skToWorld(rx1, ry1), wp = skToWorld(rx2, ry2);
            rv = {wc.x(), wc.y(), wc.z(), wp.x(), wp.y(), wp.z()};
            glLineWidth(1.5f);
            uploadAndDraw(rv, cRubber, GL_LINES);
        } else if (tool == SketchTool::DrawRectangleCenter) {
            // Center rectangle rubber-band
            double dx = std::abs(rx2 - rx1), dy = std::abs(ry2 - ry1);
            QVector3D c0 = skToWorld(rx1 - dx, ry1 - dy);
            QVector3D c1 = skToWorld(rx1 + dx, ry1 - dy);
            QVector3D c2 = skToWorld(rx1 + dx, ry1 + dy);
            QVector3D c3 = skToWorld(rx1 - dx, ry1 + dy);
            rv = {c0.x(), c0.y(), c0.z(), c1.x(), c1.y(), c1.z(),
                  c1.x(), c1.y(), c1.z(), c2.x(), c2.y(), c2.z(),
                  c2.x(), c2.y(), c2.z(), c3.x(), c3.y(), c3.z(),
                  c3.x(), c3.y(), c3.z(), c0.x(), c0.y(), c0.z()};
            glLineWidth(1.5f);
            uploadAndDraw(rv, cRubber, GL_LINES);
        }
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_PROGRAM_POINT_SIZE);

    m_sketchOverlayProgram->release();
    tempVbo.destroy();
    tempVao.destroy();
}

// =============================================================================
// World-to-screen projection helper
// =============================================================================

QPointF Viewport3D::worldToScreen(const QVector3D& worldPt) const
{
    QMatrix4x4 view;
    view.lookAt(m_eye, m_center, m_up);
    QMatrix4x4 proj;
    buildProjectionMatrix(proj);
    QMatrix4x4 mvp = proj * view;

    QVector4D clip = mvp * QVector4D(worldPt, 1.0f);
    if (std::abs(clip.w()) < 1e-7f)
        return QPointF(-1, -1);

    float ndcX = clip.x() / clip.w();
    float ndcY = clip.y() / clip.w();

    float screenX = (ndcX * 0.5f + 0.5f) * width();
    float screenY = (1.0f - (ndcY * 0.5f + 0.5f)) * height();

    return QPointF(static_cast<double>(screenX), static_cast<double>(screenY));
}

// =============================================================================
// Sketch constraint / dimension overlay (QPainter 2D)
// =============================================================================

void Viewport3D::drawSketchConstraintOverlay()
{
    if (!m_sketchEditor)
        return;

    sketch::Sketch* sk = m_sketchEditor->currentSketch();
    if (!sk)
        return;

    const auto& constraints = sk->constraints();
    if (constraints.empty() && !m_sketchEditor->isDragging() &&
        !m_sketchEditor->hasFirstPick())
        return;

    // Helper: sketch 2D -> screen 2D
    auto skToScreen = [&](double ssx, double ssy) -> QPointF {
        double wx, wy, wz;
        sk->sketchToWorld(ssx, ssy, wx, wy, wz);
        return worldToScreen(QVector3D(static_cast<float>(wx),
                                       static_cast<float>(wy),
                                       static_cast<float>(wz)));
    };

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Fonts
    QFont dimFont("Monospace", 10);
    dimFont.setStyleHint(QFont::Monospace);
    QFont iconFont("Monospace", 8, QFont::Bold);

    // Colors
    const QColor cDimLine(100, 220, 255, 200);    // thin cyan lines for dimensions
    const QColor cDimText(255, 255, 255, 255);    // white text
    const QColor cDimBg(30, 30, 30, 180);         // dark background for readability
    const QColor cConMarker(180, 100, 255, 220);
    const QColor cDragHighlight(255, 255, 0, 180);
    const QColor cPickHighlight(255, 140, 0, 200);

    // Helper lambda: draw dimension label with dark background rectangle
    auto drawDimLabel = [&](QPainter& p, const QPointF& pos, const QString& text) {
        p.setFont(dimFont);
        QFontMetricsF fm(dimFont);
        QRectF br = fm.boundingRect(text);
        double pad = 3.0;
        QRectF bgRect(pos.x() - br.width() / 2.0 - pad,
                       pos.y() - br.height() / 2.0 - pad,
                       br.width() + 2.0 * pad,
                       br.height() + 2.0 * pad);
        p.setPen(Qt::NoPen);
        p.setBrush(cDimBg);
        p.drawRoundedRect(bgRect, 3, 3);
        p.setPen(cDimText);
        p.setBrush(Qt::NoBrush);
        p.drawText(bgRect, Qt::AlignCenter, text);
    };

    // ── Draw dimension constraints ──────────────────────────────────────
    for (const auto& [cid, con] : constraints) {
        using CT = sketch::ConstraintType;

        if (con.type == CT::Distance) {
            // Distance between two points: entityIds = {pt1, pt2}
            if (con.entityIds.size() < 2) continue;
            try {
                const auto& p1 = sk->point(con.entityIds[0]);
                const auto& p2 = sk->point(con.entityIds[1]);

                QPointF s1 = skToScreen(p1.x, p1.y);
                QPointF s2 = skToScreen(p2.x, p2.y);

                // Midpoint for text
                QPointF mid((s1.x() + s2.x()) / 2.0, (s1.y() + s2.y()) / 2.0);

                // Offset the dimension line perpendicular to the segment
                double dx = s2.x() - s1.x();
                double dy = s2.y() - s1.y();
                double len = std::sqrt(dx * dx + dy * dy);
                if (len < 1e-3) continue;
                double nx = -dy / len * 12.0;
                double ny =  dx / len * 12.0;

                QPointF d1(s1.x() + nx, s1.y() + ny);
                QPointF d2(s2.x() + nx, s2.y() + ny);
                QPointF dimMid((d1.x() + d2.x()) / 2.0, (d1.y() + d2.y()) / 2.0);

                // Extension lines
                QPen dimPen(cDimLine, 1.0, Qt::DashLine);
                painter.setPen(dimPen);
                painter.drawLine(s1, d1);
                painter.drawLine(s2, d2);

                // Dimension line with arrowheads
                painter.setPen(QPen(cDimLine, 1.5));
                painter.drawLine(d1, d2);

                // Arrowheads (small triangles)
                double adx = d2.x() - d1.x();
                double ady = d2.y() - d1.y();
                double alen = std::sqrt(adx * adx + ady * ady);
                if (alen > 10) {
                    double ux = adx / alen, uy = ady / alen;
                    double px = -uy, py = ux;
                    const double as = 5.0;  // arrow size
                    // Arrow at d1 (pointing toward d2)
                    QPointF a1a(d1.x() + ux * as + px * as * 0.4,
                                d1.y() + uy * as + py * as * 0.4);
                    QPointF a1b(d1.x() + ux * as - px * as * 0.4,
                                d1.y() + uy * as - py * as * 0.4);
                    painter.drawLine(d1, a1a);
                    painter.drawLine(d1, a1b);
                    // Arrow at d2 (pointing toward d1)
                    QPointF a2a(d2.x() - ux * as + px * as * 0.4,
                                d2.y() - uy * as + py * as * 0.4);
                    QPointF a2b(d2.x() - ux * as - px * as * 0.4,
                                d2.y() - uy * as - py * as * 0.4);
                    painter.drawLine(d2, a2a);
                    painter.drawLine(d2, a2b);
                }

                // Dimension value text with background
                QString txt = QString::number(con.value, 'f', 2) + " mm";
                drawDimLabel(painter, dimMid, txt);
            } catch (...) {
                continue;
            }

        } else if (con.type == CT::DistancePointLine) {
            // entityIds = {pointId, lineId}
            if (con.entityIds.size() < 2) continue;
            try {
                const auto& pt = sk->point(con.entityIds[0]);
                const auto& ln = sk->line(con.entityIds[1]);
                const auto& lp1 = sk->point(ln.startPointId);
                const auto& lp2 = sk->point(ln.endPointId);

                // Project point onto line to find foot
                double lx = lp2.x - lp1.x, ly = lp2.y - lp1.y;
                double lenSq = lx * lx + ly * ly;
                double t = 0.5;
                if (lenSq > 1e-12)
                    t = std::clamp(((pt.x - lp1.x) * lx + (pt.y - lp1.y) * ly) / lenSq, 0.0, 1.0);
                double footX = lp1.x + t * lx;
                double footY = lp1.y + t * ly;

                QPointF sPt = skToScreen(pt.x, pt.y);
                QPointF sFoot = skToScreen(footX, footY);
                QPointF mid((sPt.x() + sFoot.x()) / 2.0, (sPt.y() + sFoot.y()) / 2.0);

                painter.setPen(QPen(cDimLine, 1.0, Qt::DashLine));
                painter.drawLine(sPt, sFoot);

                QString txt = QString::number(con.value, 'f', 2) + " mm";
                drawDimLabel(painter, mid, txt);
            } catch (...) {
                continue;
            }

        } else if (con.type == CT::Radius) {
            // entityIds = {circleOrArcId}
            if (con.entityIds.empty()) continue;
            try {
                // Try circle first, then arc
                double cx = 0, cy = 0, r = con.value;
                bool found = false;
                for (const auto& [circId, circ] : sk->circles()) {
                    if (circId == con.entityIds[0]) {
                        const auto& cp = sk->point(circ.centerPointId);
                        cx = cp.x; cy = cp.y; r = circ.radius;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    for (const auto& [arcId, arc] : sk->arcs()) {
                        if (arcId == con.entityIds[0]) {
                            const auto& cp = sk->point(arc.centerPointId);
                            cx = cp.x; cy = cp.y; r = arc.radius;
                            found = true;
                            break;
                        }
                    }
                }
                if (!found) continue;

                // Leader line from center to point on circumference (rightward)
                QPointF sCenter = skToScreen(cx, cy);
                QPointF sEdge = skToScreen(cx + r, cy);

                painter.setPen(QPen(cDimLine, 1.5));
                painter.drawLine(sCenter, sEdge);

                // Arrowhead at sEdge pointing outward
                double adx = sEdge.x() - sCenter.x();
                double ady = sEdge.y() - sCenter.y();
                double alen = std::sqrt(adx * adx + ady * ady);
                if (alen > 5) {
                    double ux = adx / alen, uy = ady / alen;
                    double px = -uy, py = ux;
                    const double as = 5.0;
                    QPointF aa(sEdge.x() - ux * as + px * as * 0.4,
                               sEdge.y() - uy * as + py * as * 0.4);
                    QPointF ab(sEdge.x() - ux * as - px * as * 0.4,
                               sEdge.y() - uy * as - py * as * 0.4);
                    painter.drawLine(sEdge, aa);
                    painter.drawLine(sEdge, ab);
                }

                // "R value" text with background
                QPointF mid((sCenter.x() + sEdge.x()) / 2.0,
                            (sCenter.y() + sEdge.y()) / 2.0 - 12.0);
                QString txt = "R " + QString::number(con.value, 'f', 2) + " mm";
                drawDimLabel(painter, mid, txt);
            } catch (...) {
                continue;
            }

        } else if (con.type == CT::FixedAngle) {
            // entityIds = {lineId}, value = angle in degrees
            if (con.entityIds.empty()) continue;
            try {
                const auto& ln = sk->line(con.entityIds[0]);
                const auto& p1 = sk->point(ln.startPointId);
                const auto& p2 = sk->point(ln.endPointId);
                double mx = (p1.x + p2.x) / 2.0;
                double my = (p1.y + p2.y) / 2.0;
                QPointF sMid = skToScreen(mx, my);
                // Offset slightly above the line
                QPointF labelPos(sMid.x(), sMid.y() - 18.0);

                // Draw a small angle arc indicator
                double arcR = 15.0;
                painter.setPen(QPen(cDimLine, 1.0));
                painter.setBrush(Qt::NoBrush);
                QRectF arcRect(sMid.x() - arcR, sMid.y() - arcR, 2 * arcR, 2 * arcR);
                int startA = static_cast<int>(0 * 16);  // from horizontal axis
                int spanA = static_cast<int>(con.value * 16);
                painter.drawArc(arcRect, startA, spanA);

                QString txt = QString::number(con.value, 'f', 1) + QChar(0x00B0);
                drawDimLabel(painter, labelPos, txt);
            } catch (...) {
                continue;
            }

        } else if (con.type == CT::AngleBetween) {
            // entityIds = {line1Id, line2Id}, value = angle in degrees
            if (con.entityIds.size() < 2) continue;
            try {
                const auto& ln1 = sk->line(con.entityIds[0]);
                const auto& ln2 = sk->line(con.entityIds[1]);
                const auto& l1p1 = sk->point(ln1.startPointId);
                const auto& l1p2 = sk->point(ln1.endPointId);
                const auto& l2p1 = sk->point(ln2.startPointId);
                const auto& l2p2 = sk->point(ln2.endPointId);

                // Find the intersection / closest point (use midpoint of both lines' midpoints)
                double m1x = (l1p1.x + l1p2.x) / 2.0;
                double m1y = (l1p1.y + l1p2.y) / 2.0;
                double m2x = (l2p1.x + l2p2.x) / 2.0;
                double m2y = (l2p1.y + l2p2.y) / 2.0;
                double cx = (m1x + m2x) / 2.0;
                double cy = (m1y + m2y) / 2.0;
                QPointF sCenter = skToScreen(cx, cy);

                // Draw a small angle arc
                double arcR = 18.0;
                painter.setPen(QPen(cDimLine, 1.0));
                painter.setBrush(Qt::NoBrush);
                double a1 = std::atan2(l1p2.y - l1p1.y, l1p2.x - l1p1.x) * 180.0 / M_PI;
                int startA = static_cast<int>(-a1 * 16);
                int spanA = static_cast<int>(-con.value * 16);
                QRectF arcRect(sCenter.x() - arcR, sCenter.y() - arcR, 2 * arcR, 2 * arcR);
                painter.drawArc(arcRect, startA, spanA);

                // Label
                QPointF labelPos(sCenter.x(), sCenter.y() - arcR - 8.0);
                QString txt = QString::number(con.value, 'f', 1) + QChar(0x00B0);
                drawDimLabel(painter, labelPos, txt);
            } catch (...) {
                continue;
            }

        } else if (con.type == CT::Horizontal || con.type == CT::Vertical ||
                   con.type == CT::Parallel || con.type == CT::Perpendicular) {
            // Geometric constraints: draw a small colored marker at midpoint of first entity
            if (con.entityIds.empty()) continue;
            try {
                const auto& ln = sk->line(con.entityIds[0]);
                const auto& p1 = sk->point(ln.startPointId);
                const auto& p2 = sk->point(ln.endPointId);
                double mx = (p1.x + p2.x) / 2.0;
                double my = (p1.y + p2.y) / 2.0;
                QPointF sMid = skToScreen(mx, my);

                // Offset up a bit
                sMid.setY(sMid.y() - 14.0);

                // Draw marker background
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(60, 30, 100, 180));
                painter.drawRoundedRect(QRectF(sMid.x() - 10, sMid.y() - 8, 20, 16), 3, 3);

                // Label
                painter.setFont(iconFont);
                painter.setPen(cConMarker);
                QString sym;
                if (con.type == CT::Horizontal)    sym = "H";
                else if (con.type == CT::Vertical) sym = "V";
                else if (con.type == CT::Parallel) sym = "//";
                else                               sym = QChar(0x27C2);  // perpendicular symbol
                QRectF iconRect(sMid.x() - 10, sMid.y() - 8, 20, 16);
                painter.drawText(iconRect, Qt::AlignCenter, sym);
            } catch (...) {
                continue;
            }

        } else if (con.type == CT::Coincident || con.type == CT::PointOnLine ||
                   con.type == CT::PointOnCircle || con.type == CT::Concentric ||
                   con.type == CT::Tangent || con.type == CT::Equal ||
                   con.type == CT::Symmetric || con.type == CT::Midpoint) {
            // Draw a small dot marker at the first entity (point)
            if (con.entityIds.empty()) continue;
            try {
                const auto& pt = sk->point(con.entityIds[0]);
                QPointF sPt = skToScreen(pt.x, pt.y);
                painter.setPen(Qt::NoPen);
                painter.setBrush(cConMarker);
                painter.drawEllipse(sPt, 4.0, 4.0);
            } catch (...) {
                continue;
            }
        }
    }

    // ── Highlight dragged point ─────────────────────────────────────────
    if (m_sketchEditor->isDragging()) {
        std::string dpId = m_sketchEditor->dragPointId();
        if (!dpId.empty()) {
            try {
                const auto& dp = sk->point(dpId);
                QPointF sDp = skToScreen(dp.x, dp.y);
                painter.setPen(QPen(cDragHighlight, 2.0));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(sDp, 8.0, 8.0);
            } catch (...) {}
        }
    }

    // ── Highlight first pick (Dimension / Constraint tool) ──────────────
    if (m_sketchEditor->hasFirstPick()) {
        const auto& fp = m_sketchEditor->firstPick();
        if (fp.kind == SketchPickResult::Point) {
            try {
                const auto& pt = sk->point(fp.entityId);
                QPointF sPt = skToScreen(pt.x, pt.y);
                painter.setPen(QPen(cPickHighlight, 2.5));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(sPt, 7.0, 7.0);
            } catch (...) {}
        } else if (fp.kind == SketchPickResult::Line) {
            try {
                const auto& ln = sk->line(fp.entityId);
                const auto& p1 = sk->point(ln.startPointId);
                const auto& p2 = sk->point(ln.endPointId);
                QPointF s1 = skToScreen(p1.x, p1.y);
                QPointF s2 = skToScreen(p2.x, p2.y);
                painter.setPen(QPen(cPickHighlight, 3.0));
                painter.drawLine(s1, s2);
            } catch (...) {}
        } else if (fp.kind == SketchPickResult::Circle) {
            try {
                const auto& circ = sk->circle(fp.entityId);
                const auto& cp = sk->point(circ.centerPointId);
                QPointF sCenter = skToScreen(cp.x, cp.y);
                QPointF sEdge = skToScreen(cp.x + circ.radius, cp.y);
                double rPx = std::sqrt(std::pow(sEdge.x() - sCenter.x(), 2) +
                                       std::pow(sEdge.y() - sCenter.y(), 2));
                painter.setPen(QPen(cPickHighlight, 2.5));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(sCenter, rPx, rPx);
            } catch (...) {}
        }
    }

    painter.end();
}

// =============================================================================
// ViewCube overlay
// =============================================================================

void Viewport3D::drawViewCubeOverlay()
{
    // Use QPainter on top of the OpenGL context to draw a 2D overlay.
    // Qt requires endNativePainting() before using QPainter on an OpenGL widget.

    // Build the view rotation matrix (rotation only -- no translation)
    QMatrix4x4 viewMat;
    viewMat.lookAt(m_eye, m_center, m_up);
    // Extract the 3x3 rotation (upper-left) as a QMatrix4x4
    QMatrix4x4 viewRot;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            viewRot(r, c) = viewMat(r, c);

    // Cube half-size in screen pixels
    const float hs = kViewCubeSize * 0.35f;

    // 8 cube corners in world-aligned space
    const QVector3D corners[8] = {
        {-hs, -hs, -hs}, { hs, -hs, -hs}, { hs,  hs, -hs}, {-hs,  hs, -hs},
        {-hs, -hs,  hs}, { hs, -hs,  hs}, { hs,  hs,  hs}, {-hs,  hs,  hs}
    };

    // Project corners: rotate by camera, then map to 2D with simple ortho
    // Center of the ViewCube area in widget coords
    const float cx = width() - kViewCubeMargin - kViewCubeSize * 0.5f;
    const float cy = kViewCubeMargin + kViewCubeSize * 0.5f;

    QPointF p[8];
    float   pz[8];
    for (int i = 0; i < 8; ++i) {
        QVector3D r = viewRot.map(corners[i]);
        p[i]  = QPointF(cx + r.x(), cy - r.y());
        pz[i] = r.z();
    }

    // 6 faces: indices, labels, direction vectors for click-to-snap
    struct CubeFace {
        int v[4];
        const char* label;
        QVector3D direction;
        QVector3D up;
    };
    const CubeFace faces[6] = {
        // Front face: -Y direction  (camera looks along -Y to see front)
        {{4, 5, 6, 7}, "Front",  { 0,  1, 0}, {0, 0, 1}},
        // Back face: +Y direction
        {{1, 0, 3, 2}, "Back",   { 0, -1, 0}, {0, 0, 1}},
        // Right face: -X direction
        {{5, 1, 2, 6}, "Right",  {-1,  0, 0}, {0, 0, 1}},
        // Left face: +X direction
        {{0, 4, 7, 3}, "Left",   { 1,  0, 0}, {0, 0, 1}},
        // Top face: -Z direction (camera looks down)
        {{7, 6, 2, 3}, "Top",    { 0,  0, 1}, {0, 1, 0}},
        // Bottom face: +Z direction
        {{0, 1, 5, 4}, "Bottom", { 0,  0,-1}, {0,-1, 0}},
    };

    // Compute average Z for each face (for painter's algorithm)
    struct FaceOrder { int idx; float avgZ; };
    FaceOrder order[6];
    for (int i = 0; i < 6; ++i) {
        float z = 0;
        for (int j = 0; j < 4; ++j)
            z += pz[faces[i].v[j]];
        order[i] = {i, z * 0.25f};
    }
    // Sort back-to-front (most negative Z first = farthest)
    std::sort(std::begin(order), std::end(order),
              [](const FaceOrder& a, const FaceOrder& b) { return a.avgZ < b.avgZ; });

    // Begin QPainter overlay
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Semi-transparent background circle behind the cube
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(40, 40, 40, 100));
    painter.drawEllipse(QPointF(cx, cy), kViewCubeSize * 0.52, kViewCubeSize * 0.52);

    // Draw faces back-to-front
    for (int fi = 0; fi < 6; ++fi) {
        const CubeFace& face = faces[order[fi].idx];

        QPolygonF poly;
        for (int j = 0; j < 4; ++j)
            poly << p[face.v[j]];

        // Face fill: slightly transparent gray, brighter for front-facing
        int alpha = (order[fi].avgZ > 0) ? 180 : 100;
        painter.setBrush(QColor(70, 75, 80, alpha));
        painter.setPen(QPen(QColor(180, 180, 180, 200), 1.0));
        painter.drawPolygon(poly);

        // Draw label only on front-facing faces (positive average Z = closer)
        if (order[fi].avgZ > 0) {
            QPointF center(0, 0);
            for (int j = 0; j < 4; ++j)
                center += p[face.v[j]];
            center /= 4.0;

            painter.setPen(QColor(220, 220, 220));
            QFont f = painter.font();
            f.setPixelSize(10);
            f.setBold(true);
            painter.setFont(f);
            painter.drawText(QRectF(center.x() - 30, center.y() - 8, 60, 16),
                             Qt::AlignCenter, face.label);
        }
    }

    // Draw projection mode indicator
    {
        painter.setPen(QColor(140, 140, 140));
        QFont f = painter.font();
        f.setPixelSize(9);
        f.setBold(false);
        painter.setFont(f);
        const char* projLabel = m_perspectiveProjection ? "Persp" : "Ortho";
        painter.drawText(QRectF(cx - 25, cy + kViewCubeSize * 0.5f + 2, 50, 14),
                         Qt::AlignCenter, projLabel);
    }

    painter.end();
}

// =============================================================================
// Manipulator 2D overlay (value label and flip arrow)
// =============================================================================

void Viewport3D::drawManipulatorOverlay()
{
    if (!m_manipulator || !m_manipulator->isVisible())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QMatrix4x4 v = viewMatrix();
    QMatrix4x4 p = projectionMatrix();
    m_manipulator->drawOverlay(painter, v, p, width(), height());

    painter.end();
}

bool Viewport3D::handleViewCubeClick(const QPoint& pos)
{
    // Check if click is in the ViewCube region (top-right corner)
    const float cx = width() - kViewCubeMargin - kViewCubeSize * 0.5f;
    const float cy = kViewCubeMargin + kViewCubeSize * 0.5f;
    const float dx = pos.x() - cx;
    const float dy = pos.y() - cy;
    const float radius = kViewCubeSize * 0.55f;
    if (dx * dx + dy * dy > radius * radius)
        return false;

    // Build view rotation for projection
    QMatrix4x4 viewMat;
    viewMat.lookAt(m_eye, m_center, m_up);
    QMatrix4x4 viewRot;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            viewRot(r, c) = viewMat(r, c);

    const float hs = kViewCubeSize * 0.35f;
    const QVector3D corners[8] = {
        {-hs, -hs, -hs}, { hs, -hs, -hs}, { hs,  hs, -hs}, {-hs,  hs, -hs},
        {-hs, -hs,  hs}, { hs, -hs,  hs}, { hs,  hs,  hs}, {-hs,  hs,  hs}
    };

    QPointF p[8];
    float   pz[8];
    for (int i = 0; i < 8; ++i) {
        QVector3D r = viewRot.map(corners[i]);
        p[i]  = QPointF(cx + r.x(), cy - r.y());
        pz[i] = r.z();
    }

    struct CubeFace {
        int v[4];
        QVector3D direction;
        QVector3D up;
    };
    const CubeFace faces[6] = {
        {{4, 5, 6, 7}, { 0,  1, 0}, {0, 0, 1}},   // Front
        {{1, 0, 3, 2}, { 0, -1, 0}, {0, 0, 1}},   // Back
        {{5, 1, 2, 6}, {-1,  0, 0}, {0, 0, 1}},   // Right
        {{0, 4, 7, 3}, { 1,  0, 0}, {0, 0, 1}},   // Left
        {{7, 6, 2, 3}, { 0,  0, 1}, {0, 1, 0}},   // Top
        {{0, 1, 5, 4}, { 0,  0,-1}, {0,-1, 0}},   // Bottom
    };

    // Find the front-most face that contains the click point
    // Sort front-to-back (most positive Z first)
    struct FaceOrder { int idx; float avgZ; };
    FaceOrder order[6];
    for (int i = 0; i < 6; ++i) {
        float z = 0;
        for (int j = 0; j < 4; ++j)
            z += pz[faces[i].v[j]];
        order[i] = {i, z * 0.25f};
    }
    std::sort(std::begin(order), std::end(order),
              [](const FaceOrder& a, const FaceOrder& b) { return a.avgZ > b.avgZ; });

    QPointF clickPt(pos);
    for (int fi = 0; fi < 6; ++fi) {
        if (order[fi].avgZ <= 0)
            break;  // back-facing, not clickable

        const CubeFace& face = faces[order[fi].idx];
        QPolygonF poly;
        for (int j = 0; j < 4; ++j)
            poly << p[face.v[j]];

        if (poly.containsPoint(clickPt, Qt::OddEvenFill)) {
            setStandardView(face.direction, face.up);
            return true;
        }
    }

    return false;
}

// =============================================================================
// Preview mesh overlay
// =============================================================================

void Viewport3D::setPreviewMesh(const std::vector<float>& verts,
                                const std::vector<float>& normals,
                                const std::vector<uint32_t>& indices)
{
    if (verts.empty() || indices.empty()) {
        clearPreviewMesh();
        return;
    }

    makeCurrent();

    // Destroy old resources if they exist
    if (m_previewLoaded) {
        m_previewVao.destroy();
        m_previewVboPos.destroy();
        m_previewVboNorm.destroy();
        m_previewEbo.destroy();
    }

    m_previewVao.create();
    m_previewVboPos.create();
    m_previewVboNorm.create();
    m_previewEbo.create();

    m_previewVao.bind();

    // positions -> location 0
    m_previewVboPos.bind();
    m_previewVboPos.allocate(verts.data(),
                             static_cast<int>(verts.size() * sizeof(float)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    m_previewVboPos.release();

    // normals -> location 1
    m_previewVboNorm.bind();
    m_previewVboNorm.allocate(normals.data(),
                              static_cast<int>(normals.size() * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    m_previewVboNorm.release();

    // Provide a dummy face-id attribute (location 2, all -1 = no face picking)
    const size_t vertCount = verts.size() / 3;
    std::vector<float> dummyFaceIds(vertCount, -1.0f);
    QOpenGLBuffer tmpFidBuf(QOpenGLBuffer::VertexBuffer);
    tmpFidBuf.create();
    tmpFidBuf.bind();
    tmpFidBuf.allocate(dummyFaceIds.data(),
                       static_cast<int>(dummyFaceIds.size() * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(float), nullptr);
    tmpFidBuf.release();

    // index buffer
    m_previewEbo.bind();
    m_previewEbo.allocate(indices.data(),
                          static_cast<int>(indices.size() * sizeof(uint32_t)));

    m_previewVao.release();
    m_previewIndexCount = static_cast<GLsizei>(indices.size());
    m_previewLoaded = true;

    doneCurrent();
    update();
}

void Viewport3D::clearPreviewMesh()
{
    if (!m_previewLoaded)
        return;

    makeCurrent();

    m_previewVao.destroy();
    m_previewVboPos.destroy();
    m_previewVboNorm.destroy();
    m_previewEbo.destroy();
    m_previewIndexCount = 0;
    m_previewLoaded = false;

    doneCurrent();
    update();
}

void Viewport3D::drawPreviewMesh(const QMatrix4x4& model,
                                 const QMatrix4x4& view,
                                 const QMatrix4x4& projection)
{
    // Render the preview mesh with semi-transparent blue-tinted material
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);  // don't write to depth buffer

    m_program->bind();

    QMatrix3x3 normalMatrix = model.normalMatrix();

    m_program->setUniformValue("uModel",        model);
    m_program->setUniformValue("uView",         view);
    m_program->setUniformValue("uProjection",   projection);
    m_program->setUniformValue("uNormalMatrix", normalMatrix);
    m_program->setUniformValue("uClipEnabled", m_clipEnabled);
    m_program->setUniformValue("uClipPlane", m_clipPlane);

    // Lighting (same as main scene)
    QVector3D lightDir = QVector3D(0.3f, 1.0f, 0.5f).normalized();
    m_program->setUniformValue("uViewPos",     m_eye);
    m_program->setUniformValue("uLightDir",    lightDir);
    m_program->setUniformValue("uLightColor",  QVector3D(1.0f, 1.0f, 1.0f));

    // Preview color: semi-transparent blue tint
    m_program->setUniformValue("uObjectColor", QVector3D(0.3f, 0.5f, 0.9f));
    m_program->setUniformValue("uAlpha", 0.5f);

    // Disable highlight/pre-selection for preview
    m_program->setUniformValue("uHighlightedFaceCount", 0);
    m_program->setUniformValue("uPreSelectedFace", -1);

    m_previewVao.bind();
    glDrawElements(GL_TRIANGLES, m_previewIndexCount, GL_UNSIGNED_INT, nullptr);
    m_previewVao.release();

    m_program->release();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

// =============================================================================
// Smooth camera animation
// =============================================================================

void Viewport3D::animateTo(const QVector3D& targetEye, const QVector3D& targetCenter,
                            const QVector3D& targetUp, int durationMs)
{
    // If the move is very small, snap immediately (avoids jitter)
    float dist = (targetEye - m_eye).length() + (targetCenter - m_center).length();
    if (dist < 1e-4f || durationMs <= 0) {
        m_eye    = targetEye;
        m_center = targetCenter;
        m_up     = targetUp;
        update();
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
// Sketch snap indicators & live dimension overlay (QPainter 2D)
// =============================================================================

void Viewport3D::drawSketchSnapAndDimensionOverlay()
{
    if (!m_sketchEditor || !m_sketchEditor->isEditing())
        return;

    sketch::Sketch* sk = m_sketchEditor->currentSketch();
    if (!sk)
        return;

    double cursorX = m_sketchEditor->rubberCurrentX();
    double cursorY = m_sketchEditor->rubberCurrentY();

    // Helper: sketch 2D -> screen 2D
    auto skToScreen = [&](double ssx, double ssy) -> QPointF {
        double wx, wy, wz;
        sk->sketchToWorld(ssx, ssy, wx, wy, wz);
        return worldToScreen(QVector3D(static_cast<float>(wx),
                                       static_cast<float>(wy),
                                       static_cast<float>(wz)));
    };

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // ── Snap-to-point indicators (orange dot) ───────────────────────────
    constexpr double snapThreshold = 5.0;   // sketch-space units
    constexpr double alignThreshold = 1.5;  // sketch-space units for H/V alignment

    const QColor cSnapDot(255, 160, 0, 220);     // orange
    const QColor cAlignLine(100, 220, 100, 120);  // green dashed
    const QColor cAlignText(180, 255, 180, 200);
    const QColor cDimLive(200, 230, 255, 240);
    const QColor cDimLiveBg(30, 30, 30, 180);

    bool snappedToPoint = false;

    for (const auto& [id, pt] : sk->points()) {
        double dx = cursorX - pt.x;
        double dy = cursorY - pt.y;
        double distSq = dx * dx + dy * dy;

        // Point snap indicator
        if (distSq < snapThreshold * snapThreshold) {
            QPointF screenPt = skToScreen(pt.x, pt.y);
            painter.setPen(QPen(cSnapDot, 2.5));
            painter.setBrush(cSnapDot);
            painter.drawEllipse(screenPt, 6.0, 6.0);
            snappedToPoint = true;
        }
    }

    // ── H/V alignment indicators (dashed crosshair + H/V label) ────────
    if (!snappedToPoint) {
        QFont alignFont("Monospace", 9, QFont::Bold);
        alignFont.setStyleHint(QFont::Monospace);
        painter.setFont(alignFont);

        for (const auto& [id, pt] : sk->points()) {
            // Horizontal alignment
            if (std::abs(cursorY - pt.y) < alignThreshold) {
                QPointF s1 = skToScreen(pt.x, pt.y);
                QPointF s2 = skToScreen(cursorX, cursorY);
                QPen dashPen(cAlignLine, 1.0, Qt::DashLine);
                painter.setPen(dashPen);
                painter.drawLine(s1, s2);

                // "H" label at midpoint
                QPointF mid((s1.x() + s2.x()) / 2.0, (s1.y() + s2.y()) / 2.0 - 10.0);
                painter.setPen(cAlignText);
                painter.drawText(mid, "H");
            }

            // Vertical alignment
            if (std::abs(cursorX - pt.x) < alignThreshold) {
                QPointF s1 = skToScreen(pt.x, pt.y);
                QPointF s2 = skToScreen(cursorX, cursorY);
                QPen dashPen(cAlignLine, 1.0, Qt::DashLine);
                painter.setPen(dashPen);
                painter.drawLine(s1, s2);

                // "V" label at midpoint
                QPointF mid((s1.x() + s2.x()) / 2.0 + 8.0, (s1.y() + s2.y()) / 2.0);
                painter.setPen(cAlignText);
                painter.drawText(mid, "V");
            }
        }
    }

    // ── Live dimension display during rubber-band drawing ───────────────
    if (m_sketchEditor->isDrawingInProgress()) {
        double startX = m_sketchEditor->rubberStartX();
        double startY = m_sketchEditor->rubberStartY();
        double dx = cursorX - startX;
        double dy = cursorY - startY;
        double dist = std::sqrt(dx * dx + dy * dy);

        if (dist > 0.5) {
            // Distance text at midpoint of the rubber-band line, offset slightly
            double midSX = (startX + cursorX) / 2.0;
            double midSY = (startY + cursorY) / 2.0;
            QPointF screenMid = skToScreen(midSX, midSY);

            // Perpendicular offset so text doesn't overlap the line
            double len = std::sqrt(dx * dx + dy * dy);
            double nx = -dy / len * 16.0;  // screen pixel offset
            double ny =  dx / len * 16.0;
            QPointF textPos(screenMid.x() + nx, screenMid.y() + ny);

            // Format dimension string
            QString dimText = QString::number(dist, 'f', 1) + " mm";

            // Draw angle for non-axis-aligned lines
            SketchTool tool = m_sketchEditor->currentTool();
            if (tool == SketchTool::DrawLine) {
                double angleDeg = std::atan2(dy, dx) * 180.0 / M_PI;
                // Normalize to 0..360
                if (angleDeg < 0) angleDeg += 360.0;
                dimText += QString("  %1%2").arg(angleDeg, 0, 'f', 1).arg(QChar(0x00B0));
            }

            // For rectangles, show W x H
            if (tool == SketchTool::DrawRectangle || tool == SketchTool::DrawRectangleCenter) {
                double w = std::abs(dx);
                double h = std::abs(dy);
                dimText = QString("%1 x %2 mm").arg(w, 0, 'f', 1).arg(h, 0, 'f', 1);
            }

            // For circles, show radius
            if (tool == SketchTool::DrawCircle || tool == SketchTool::DrawCircle3Point) {
                dimText = QString("R %1 mm").arg(dist, 0, 'f', 1);
            }

            // Draw background rect + text
            QFont dimFont("Monospace", 10);
            dimFont.setStyleHint(QFont::Monospace);
            painter.setFont(dimFont);
            QFontMetricsF fm(dimFont);
            QRectF br = fm.boundingRect(dimText);
            double pad = 4.0;
            QRectF bgRect(textPos.x() - br.width() / 2.0 - pad,
                          textPos.y() - br.height() / 2.0 - pad,
                          br.width() + 2.0 * pad,
                          br.height() + 2.0 * pad);
            painter.setPen(Qt::NoPen);
            painter.setBrush(cDimLiveBg);
            painter.drawRoundedRect(bgRect, 4, 4);
            painter.setPen(cDimLive);
            painter.setBrush(Qt::NoBrush);
            painter.drawText(bgRect, Qt::AlignCenter, dimText);
        }
    }

    painter.end();
}
