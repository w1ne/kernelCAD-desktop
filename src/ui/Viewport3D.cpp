#include "Viewport3D.h"
#include "CameraController.h"
#include "ViewportManipulator.h"
#include "SketchEditor.h"
#include "SelectionManager.h"
#include "../sketch/Sketch.h"
#include "../kernel/BRepModel.h"
#include "../kernel/BRepQuery.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QSurfaceFormat>
#include <QOpenGLFramebufferObject>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <unordered_map>

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

    // Create camera controller
    m_camera = new CameraController(this);
    connect(m_camera, &CameraController::cameraChanged, this, QOverload<>::of(&QWidget::update));

    // Pre-selection hover delay (50ms) to prevent flicker during fast mouse movement
    m_preSelectTimer.setSingleShot(true);
    m_preSelectTimer.setInterval(50);
    connect(&m_preSelectTimer, &QTimer::timeout, this, [this]() {
        handlePreSelection(m_preSelectPos);
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

    glClearColor(0.14f, 0.14f, 0.16f, 1.0f);  // blue-tinted dark gray
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

    // Create origin reference planes (XY, XZ, YZ)
    initOriginPlanes();
}

void Viewport3D::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    // Invalidate pick FBO so it gets recreated at new size
    m_pickFBO.reset();
}

void Viewport3D::paintGL()
{
    // Adjust near/far clip planes based on scene size
    m_camera->updateClipPlanes(m_bboxMin, m_bboxMax);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // -- common matrices -------------------------------------------------
    QMatrix4x4 model;                           // identity
    QMatrix4x4 view = m_camera->viewMatrix();

    const float aspect = static_cast<float>(width()) /
                         std::max(1.0f, static_cast<float>(height()));
    QMatrix4x4 projection = m_camera->projectionMatrix(aspect);

    QMatrix4x4 mvp = projection * view * model;

    // ====================================================================
    // 0. Draw ground grid (farthest back)
    // ====================================================================
    if (m_showGrid && m_gridInitialized && m_edgeProgram) {
        drawGrid(mvp);
    }

    // ====================================================================
    // 0b. Draw origin planes (semi-transparent, behind bodies)
    // ====================================================================
    if (m_showOrigin && m_originPlanesInitialized && m_edgeProgram) {
        drawOriginPlanes(mvp);
    }

    // ====================================================================
    // 0c. Draw origin axes and point (on top of planes, behind bodies)
    // ====================================================================
    if (m_showOrigin && m_gridInitialized && m_edgeProgram) {
        drawOriginAxes(mvp);
        drawOriginPoint(mvp);
    } else if (m_showGrid && m_gridInitialized && m_edgeProgram) {
        drawOriginAxes(mvp);
    }

    // ====================================================================
    // 0d. Draw custom construction planes
    // ====================================================================
    if (!m_constructionPlanes.empty() && m_edgeProgram) {
        drawConstructionPlanes(mvp);
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

        // Enable blending for semi-transparent bodies during sketch mode
        if (m_sketchModeActive) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
        m_program->setUniformValue("uViewPos",     m_camera->eye());
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

        // Ghost bodies at 30% opacity during sketch editing
        float bodyAlpha = m_sketchModeActive ? 0.3f : 1.0f;
        m_program->setUniformValue("uAlpha", bodyAlpha);

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

        if (m_sketchModeActive) {
            glDisable(GL_BLEND);
        }
        if (m_viewMode == ViewMode::HiddenLine) {
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        }
    }
    else if (drawFaces && m_meshLoaded && m_program) {
        // Legacy single-mesh fallback path
        if (m_viewMode == ViewMode::HiddenLine) {
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        }
        if (m_sketchModeActive) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
        m_program->setUniformValue("uViewPos",     m_camera->eye());
        m_program->setUniformValue("uLightDir",    lightDir);
        m_program->setUniformValue("uLightColor",  QVector3D(1.0f, 1.0f, 1.0f));
        m_program->setUniformValue("uObjectColor", QVector3D(0.6f, 0.65f, 0.7f));
        m_program->setUniformValue("uAlpha", m_sketchModeActive ? 0.3f : 1.0f);

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

        if (m_sketchModeActive) {
            glDisable(GL_BLEND);
        }
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
        m_edgeProgram->setUniformValue("uEdgeAlpha", m_sketchModeActive ? 0.15f : 1.0f);

        if (m_sketchModeActive) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
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
        if (m_sketchModeActive) {
            glDisable(GL_BLEND);
        }

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
        m_edgeProgram->setUniformValue("uEdgeAlpha", m_sketchModeActive ? 0.15f : 1.0f);

        if (m_sketchModeActive) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        glLineWidth(1.5f);

        m_edgeVao.bind();
        glDrawElements(GL_LINES, m_edgeIndexCount, GL_UNSIGNED_INT, nullptr);
        m_edgeVao.release();

        glDisable(GL_LINE_SMOOTH);
        if (m_sketchModeActive) {
            glDisable(GL_BLEND);
        }

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

    // -- passive sketches (finished sketches shown as thin lines) --------
    drawPassiveSketches();

    // -- active sketch overlay (drawn on top of 3D scene) ---------------
    if (m_sketchEditor) {
        drawSketchOverlay();
        drawSketchConstraintOverlay();
        drawSketchSnapAndDimensionOverlay();
    }

    // -- Welcome overlay (shown when no bodies are loaded) ────────────────
    if (!m_meshLoaded && !m_sketchModeActive)
        drawWelcomeOverlay();

    // -- ViewCube overlay (top-right corner) ------------------------------
    drawViewCubeOverlay();

    // -- manipulator 2D overlay (value label, flip arrow) ----------------
    drawManipulatorOverlay();

    // -- box selection rubber-band rectangle ─────────────────────────────
    if (m_boxSelecting) {
        drawBoxSelectOverlay();
    }
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
// fitAll -- delegate to CameraController
// =============================================================================

void Viewport3D::fitAll()
{
    if (!m_meshLoaded && !m_bodiesLoaded)
        return;

    m_camera->fitAll(m_bboxMin, m_bboxMax);
}

// (updateClipPlanes moved to CameraController)

// (buildProjectionMatrix moved to CameraController)

// =============================================================================
// Standard view -- delegate to CameraController
// =============================================================================

void Viewport3D::setStandardView(StandardView view)
{
    m_camera->setStandardView(view);
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
    QMatrix4x4 view = m_camera->viewMatrix();

    const float pickAspect = static_cast<float>(width()) /
                             std::max(1.0f, static_cast<float>(height()));
    QMatrix4x4 projection = m_camera->projectionMatrix(pickAspect);

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

    // Disable body/face picking during sketch mode -- only sketch entities should be pickable
    if (m_sketchModeActive)
        return;

    // Selection cycling: clicking same position cycles through filter modes
    if ((screenPos - m_lastPickPos).manhattanLength() < 4) {
        m_pickCycleIndex++;
        // Cycle through: All -> Faces -> Edges -> All ...
        static const SelectionFilter kCycleFilters[] = {
            SelectionFilter::All, SelectionFilter::Faces, SelectionFilter::Edges
        };
        int idx = m_pickCycleIndex % 3;
        m_selectionMgr->setFilter(kCycleFilters[idx]);
    } else {
        m_pickCycleIndex = 0;
        m_lastPickPos = screenPos;
    }

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
    QMatrix4x4 view = m_camera->viewMatrix();
    QMatrix4x4 projection = projectionMatrix();
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
    hit.depth  = (m_camera->eye() - hitPos).length();

    // Edge resolution: when filter is Edges, find the nearest edge on this body
    // to the 3D hit point. This converts a face pick into an edge pick.
    if (m_selectionMgr && m_selectionMgr->filter() == SelectionFilter::Edges &&
        !hit.bodyId.empty() && m_brepModelPtr) {
        try {
            // Use BRepQuery to find edges of the picked face and pick the closest
            // Edge resolution using stored BRepModel pointer
                
            
            kernel::BRepQuery bq(m_brepModelPtr->getShape(hit.bodyId));
            int bestEdge = -1;
            float bestDist = 1e30f;
            int edgeCount = bq.edgeCount();
            for (int ei = 0; ei < edgeCount; ++ei) {
                auto einfo = bq.edgeInfo(ei);
                // Edge midpoint
                float mx = (einfo.startX + einfo.endX) * 0.5f;
                float my = (einfo.startY + einfo.endY) * 0.5f;
                float mz = (einfo.startZ + einfo.endZ) * 0.5f;
                float dx = mx - hit.worldX, dy = my - hit.worldY, dz = mz - hit.worldZ;
                float dist = dx*dx + dy*dy + dz*dz;
                if (dist < bestDist) {
                    bestDist = dist;
                    bestEdge = ei;
                }
            }
            if (bestEdge >= 0)
                hit.edgeIndex = bestEdge;
        } catch (...) {
            // If edge resolution fails, proceed with face-only hit
        }
    }

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

    // Disable body/face hover during sketch mode
    if (m_sketchModeActive)
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

    // Check ViewCube area — start drag-to-orbit or click-to-snap
    if (event->button() == Qt::LeftButton && isInViewCubeArea(event->pos())) {
        m_viewCubeDragging = true;
        m_lastMousePos = event->pos();
        m_mousePressPos = event->pos();
        m_activeButton = Qt::LeftButton;
        m_isDragging = false;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    m_lastMousePos = event->pos();
    m_mousePressPos = event->pos();
    m_activeButton = event->button();
    m_isDragging = false;
    m_boxSelecting = false;

    // Stop orbit momentum on any mouse press
    m_camera->stopMomentum();

    // Set cursor for middle-button (orbit/pan)
    if (event->button() == Qt::MiddleButton)
        setCursor(Qt::OpenHandCursor);

    // On left-click, check if we hit empty space — if so, prepare for box selection
    if (event->button() == Qt::LeftButton && !m_sketchModeActive) {
        int faceId = pickAtScreenPos(event->pos());
        if (faceId <= 0) {
            // Hit background — this drag will be box selection, not orbit
            m_boxSelectStart = event->pos();
            m_boxSelectEnd = event->pos();
            m_boxSelecting = true;
        }
    }
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

    // ViewCube release: snap to face if click, stop orbit if drag
    if (m_viewCubeDragging && event->button() == Qt::LeftButton) {
        if (!m_isDragging) {
            // It was a click, not a drag — snap to the clicked face
            handleViewCubeClick(event->pos());
        }
        m_viewCubeDragging = false;
        m_activeButton = Qt::NoButton;
        m_isDragging = false;
        unsetCursor();
        event->accept();
        update();
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

    // Box selection: if we were dragging a selection rectangle, perform box select
    if (m_boxSelecting && event->button() == Qt::LeftButton) {
        m_boxSelectEnd = event->pos();
        QRect selRect = QRect(m_boxSelectStart, m_boxSelectEnd).normalized();
        if (selRect.width() > 5 && selRect.height() > 5) {
            performBoxSelect(selRect);
        }
        m_boxSelecting = false;
        update();
    } else if (event->button() == Qt::LeftButton && !m_isDragging) {
        // Single click without drag -- perform a pick
        bool shiftHeld = event->modifiers() & Qt::ShiftModifier;
        handlePick(event->pos(), shiftHeld);
    }

    // Orbit momentum: if releasing middle-button while dragging, apply momentum
    if (event->button() == Qt::MiddleButton && m_isDragging) {
        float lastDx = static_cast<float>(event->pos().x() - m_lastMousePos.x());
        float lastDy = static_cast<float>(event->pos().y() - m_lastMousePos.y());
        if (std::abs(lastDx) > 0.5f || std::abs(lastDy) > 0.5f) {
            m_camera->startMomentum(lastDx * 0.5f, lastDy * 0.5f);
        }
    }

    // Restore cursor after middle-button release
    if (event->button() == Qt::MiddleButton)
        setCursor(Qt::ArrowCursor);

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

    // Track ViewCube hover (only when no button pressed to avoid interfering with orbit/pan)
    if (m_activeButton == Qt::NoButton) {
        int hovered = hitTestViewCubeFace(event->pos());
        if (hovered != m_viewCubeHoveredFace) {
            m_viewCubeHoveredFace = hovered;
            if (hovered >= 0)
                setCursor(Qt::PointingHandCursor);
            else
                unsetCursor();
            update();
        }
    }

    // ViewCube drag-to-orbit
    if (m_viewCubeDragging) {
        const QPoint pos = event->pos();
        int dx = pos.x() - m_mousePressPos.x();
        int dy = pos.y() - m_mousePressPos.y();
        if (!m_isDragging && (dx * dx + dy * dy > kDragThreshold * kDragThreshold))
            m_isDragging = true;
        if (m_isDragging) {
            QVector3D va = arcballVector(m_lastMousePos);
            QVector3D vb = arcballVector(pos);
            m_camera->orbit(va, vb);
        }
        m_lastMousePos = pos;
        update();
        event->accept();
        return;
    }

    const QPoint pos = event->pos();

    // Check if we are dragging (exceeded threshold)
    if (m_activeButton != Qt::NoButton && !m_isDragging) {
        int dx = pos.x() - m_mousePressPos.x();
        int dy = pos.y() - m_mousePressPos.y();
        if (dx * dx + dy * dy > kDragThreshold * kDragThreshold) {
            m_isDragging = true;
            if (m_activeButton == Qt::MiddleButton)
                setCursor(Qt::ClosedHandCursor);
        }
    }

    // Box selection: update rectangle and repaint
    if (m_boxSelecting && m_activeButton == Qt::LeftButton && m_isDragging) {
        m_boxSelectEnd = pos;
        update();
        m_lastMousePos = pos;
        event->accept();
        return;
    }

    if (m_activeButton == Qt::LeftButton && m_isDragging &&
        (event->modifiers() & Qt::ControlModifier) && m_selectionMgr && m_selectionMgr->hasSelection()) {
        // -- Ctrl+drag: move selected body (occurrence drag) -----------------
        const float dx = static_cast<float>(pos.x() - m_lastMousePos.x());
        const float dy = static_cast<float>(pos.y() - m_lastMousePos.y());

        QVector3D forward = (m_camera->center() - m_camera->eye()).normalized();
        QVector3D right   = QVector3D::crossProduct(forward, m_camera->up()).normalized();
        QVector3D camUp   = QVector3D::crossProduct(right, forward).normalized();

        const float moveSpeed = m_camera->orbitDistance() * 0.002f;
        QVector3D delta = (dx * right - dy * camUp) * moveSpeed;

        emit occurrenceDragged(delta.x(), delta.y(), delta.z());
    }
    else if (m_activeButton == Qt::MiddleButton && m_isDragging) {
        // Middle-button = orbit, middle+Shift = pan
        bool shiftHeld = (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier);
        if (m_camera->lockRotation() || shiftHeld) {
            // -- Pan --
            const float dx2 = static_cast<float>(pos.x() - m_lastMousePos.x());
            const float dy2 = static_cast<float>(pos.y() - m_lastMousePos.y());

            m_camera->pan(dx2, dy2, static_cast<float>(width()), static_cast<float>(height()));
        } else {
            // -- Arcball rotation ------------------------------------------------
            QVector3D va = arcballVector(m_lastMousePos);
            QVector3D vb = arcballVector(pos);

            m_camera->orbit(va, vb);
        }
    }
    else if (m_activeButton == Qt::RightButton && m_isDragging) {
        // -- Right-button pan (alternative) -----------------------------------
        const float dx = static_cast<float>(pos.x() - m_lastMousePos.x());
        const float dy = static_cast<float>(pos.y() - m_lastMousePos.y());

        m_camera->pan(dx, dy, static_cast<float>(width()), static_cast<float>(height()));
    }
    else if (m_activeButton == Qt::NoButton) {
        // No button pressed -- perform pre-selection with 50ms delay to reduce flicker
        m_preSelectPos = pos;
        m_preSelectTimer.start();
    }

    m_lastMousePos = pos;
    update();
    event->accept();
}

void Viewport3D::wheelEvent(QWheelEvent* event)
{
    // angleDelta().y() is typically +/-120 per notch
    const float delta = static_cast<float>(event->angleDelta().y()) / 120.0f;

    // Compute NDC position of cursor for zoom-to-cursor
    QPointF cursorPos = event->position();
    float ndcX = (2.0f * static_cast<float>(cursorPos.x())) / width() - 1.0f;
    float ndcY = 1.0f - (2.0f * static_cast<float>(cursorPos.y())) / height();

    const float viewportAspect = static_cast<float>(width()) /
                                 std::max(1.0f, static_cast<float>(height()));
    m_camera->zoom(delta, ndcX, ndcY, viewportAspect);

    event->accept();
}

void Viewport3D::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        // Double-click middle button: set orbit center to 3D hit point
        int faceId = pickAtScreenPos(event->pos());
        if (faceId > 0 && m_lastPickDepth < 1.0f) {
            // Unproject screen pos at picked depth to get world position
            QMatrix4x4 view = m_camera->viewMatrix();
            QMatrix4x4 proj = projectionMatrix();
            QMatrix4x4 invMvp = (proj * view).inverted();

            float ndcX = (2.0f * event->pos().x()) / width() - 1.0f;
            float ndcY = 1.0f - (2.0f * event->pos().y()) / height();
            float ndcZ = m_lastPickDepth * 2.0f - 1.0f;

            QVector4D clipPt(ndcX, ndcY, ndcZ, 1.0f);
            QVector4D worldPt = invMvp * clipPt;
            if (std::abs(worldPt.w()) > 1e-7f)
                worldPt /= worldPt.w();

            QVector3D newCenter = worldPt.toVector3D();
            QVector3D direction = (m_camera->eye() - newCenter).normalized();
            m_camera->setCenter(newCenter);
            m_camera->setEye(m_camera->center() + direction * m_camera->orbitDistance());
            emit orbitCenterChanged(m_camera->center());
            update();
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        // Double-click left button: isolate body under cursor
        if (m_sketchModeActive) {
            event->accept();
            return;
        }
        int pickId = pickAtScreenPos(event->pos());
        if (pickId > 0) {
            int faceIndex = pickId - 1;
            if (m_faceMapLoaded &&
                faceIndex >= 0 &&
                static_cast<size_t>(faceIndex) < m_bodyIdPerFace.size()) {
                emit bodyDoubleClicked(m_bodyIdPerFace[static_cast<size_t>(faceIndex)]);
            }
        }
        event->accept();
        return;
    }

    QOpenGLWidget::mouseDoubleClickEvent(event);
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
            m_camera->setStandardView(QVector3D(0, -1, 0), upZ);   // Back
        else
            m_camera->setStandardView(QVector3D(0,  1, 0), upZ);   // Front
        event->accept();
        return;

    // Numpad 3: Right (look along -X) / Ctrl: Left (look along +X)
    case Qt::Key_3:
        if (ctrl)
            m_camera->setStandardView(QVector3D( 1, 0, 0), upZ);   // Left
        else
            m_camera->setStandardView(QVector3D(-1, 0, 0), upZ);   // Right
        event->accept();
        return;

    // Numpad 7: Top (look along -Z) / Ctrl: Bottom (look along +Z)
    case Qt::Key_7:
        if (ctrl)
            m_camera->setStandardView(QVector3D(0, 0, -1), -upY);  // Bottom
        else
            m_camera->setStandardView(QVector3D(0, 0,  1),  upY);  // Top
        event->accept();
        return;

    // Numpad 5: Toggle perspective / orthographic
    case Qt::Key_5:
        m_camera->togglePerspective();
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

void Viewport3D::setSketchMode(bool enabled)
{
    m_sketchModeActive = enabled;
    m_camera->setLockRotation(enabled);
    update();
}

void Viewport3D::saveCameraState()
{
    m_camera->saveCameraState();
}

void Viewport3D::restoreCameraState(bool animate)
{
    m_camera->restoreCameraState(animate);
}

bool Viewport3D::isPerspective() const
{
    return m_camera->isPerspective();
}

float Viewport3D::orbitDistance() const
{
    return m_camera->orbitDistance();
}

QMatrix4x4 Viewport3D::viewMatrix() const
{
    return m_camera->viewMatrix();
}

QMatrix4x4 Viewport3D::projectionMatrix() const
{
    const float aspect = static_cast<float>(width()) /
                         std::max(1.0f, static_cast<float>(height()));
    return m_camera->projectionMatrix(aspect);
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

void Viewport3D::setShowOrigin(bool show)
{
    m_showOrigin = show;
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

    // --- Origin axes (three colored lines extending in both directions) ---
    // X axis: (-100,0,0) -> (100,0,0)
    verts.push_back(-extent); verts.push_back(0.0f); verts.push_back(0.0f);
    verts.push_back( extent); verts.push_back(0.0f); verts.push_back(0.0f);
    // Y axis: (0,-100,0) -> (0,100,0)
    verts.push_back(0.0f); verts.push_back(-extent); verts.push_back(0.0f);
    verts.push_back(0.0f); verts.push_back( extent); verts.push_back(0.0f);
    // Z axis: (0,0,-100) -> (0,0,100)
    verts.push_back(0.0f); verts.push_back(0.0f); verts.push_back(-extent);
    verts.push_back(0.0f); verts.push_back(0.0f); verts.push_back( extent);
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
    if (m_camera->orbitDistance() > 200.0f)
        alpha = 0.0f;
    else if (m_camera->orbitDistance() > 100.0f)
        alpha = 1.0f - (m_camera->orbitDistance() - 100.0f) / 100.0f;

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

    // Draw minor grid lines (barely visible #262626)
    glLineWidth(1.0f);
    m_edgeProgram->setUniformValue("uEdgeColor",
        QVector3D(0.149f, 0.149f, 0.149f));  // #262626
    m_edgeProgram->setUniformValue("uEdgeAlpha", alpha);
    glDrawArrays(GL_LINES, 0, m_gridMinorVertexCount);

    // Draw major grid lines (subtle #333333)
    glLineWidth(1.0f);
    m_edgeProgram->setUniformValue("uEdgeColor",
        QVector3D(0.2f, 0.2f, 0.2f));  // #333333
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

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(3.0f);

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
    glDisable(GL_BLEND);
    m_edgeProgram->release();
}

// =============================================================================
// Origin reference planes (XY, XZ, YZ)
// =============================================================================

void Viewport3D::initOriginPlanes()
{
    // Build vertex data for three quads centered at origin, each 50x50mm.
    // Each quad = 4 vertices (position only), drawn as two triangles via indices.
    // Layout: [XY plane 4 verts] [XZ plane 4 verts] [YZ plane 4 verts]
    // Then border line-loop vertices reuse the same positions.
    const float S = 50.0f; // half-extent of each plane

    // clang-format off
    float verts[] = {
        // XY plane (Z=0): 4 corners  [indices 0-3]
        -S, -S, 0.0f,
         S, -S, 0.0f,
         S,  S, 0.0f,
        -S,  S, 0.0f,
        // XZ plane (Y=0): 4 corners  [indices 4-7]
        -S, 0.0f, -S,
         S, 0.0f, -S,
         S, 0.0f,  S,
        -S, 0.0f,  S,
        // YZ plane (X=0): 4 corners  [indices 8-11]
        0.0f, -S, -S,
        0.0f,  S, -S,
        0.0f,  S,  S,
        0.0f, -S,  S,
        // Origin point  [index 12]
        0.0f, 0.0f, 0.0f,
    };
    // clang-format on

    // Two triangles per quad: (0,1,2), (0,2,3) offset per plane
    uint32_t indices[] = {
        // XY
        0, 1, 2,   0, 2, 3,
        // XZ
        4, 5, 6,   4, 6, 7,
        // YZ
        8, 9, 10,  8, 10, 11,
    };

    m_originPlaneVao.create();
    m_originPlaneVbo.create();
    m_originPlaneEbo.create();

    m_originPlaneVao.bind();

    m_originPlaneVbo.bind();
    m_originPlaneVbo.allocate(verts, static_cast<int>(sizeof(verts)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    m_originPlaneVbo.release();

    m_originPlaneEbo.bind();
    m_originPlaneEbo.allocate(indices, static_cast<int>(sizeof(indices)));

    m_originPlaneVao.release();
    m_originPlaneEbo.release();

    m_originPlanesInitialized = true;
}

void Viewport3D::drawOriginPlanes(const QMatrix4x4& mvp)
{
    m_edgeProgram->bind();
    m_edgeProgram->setUniformValue("uMVP", mvp);
    m_edgeProgram->setUniformValue("uDepthBias", 0.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); // don't write to depth so bodies render in front

    m_originPlaneVao.bind();

    // --- Fill quads (semi-transparent) ---

    // XY plane -- blue tint
    m_edgeProgram->setUniformValue("uEdgeColor", QVector3D(0.2f, 0.4f, 0.8f));
    m_edgeProgram->setUniformValue("uEdgeAlpha", 0.12f);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

    // XZ plane -- red tint
    m_edgeProgram->setUniformValue("uEdgeColor", QVector3D(0.8f, 0.2f, 0.2f));
    m_edgeProgram->setUniformValue("uEdgeAlpha", 0.12f);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT,
                   reinterpret_cast<const void*>(6 * sizeof(uint32_t)));

    // YZ plane -- green tint
    m_edgeProgram->setUniformValue("uEdgeColor", QVector3D(0.2f, 0.7f, 0.2f));
    m_edgeProgram->setUniformValue("uEdgeAlpha", 0.12f);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT,
                   reinterpret_cast<const void*>(12 * sizeof(uint32_t)));

    // --- Border edges (GL_LINE_LOOP per plane, brighter) ---
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(1.0f);

    // XY border -- blue
    m_edgeProgram->setUniformValue("uEdgeColor", QVector3D(0.3f, 0.5f, 0.9f));
    m_edgeProgram->setUniformValue("uEdgeAlpha", 0.4f);
    glDrawArrays(GL_LINE_LOOP, 0, 4);

    // XZ border -- red
    m_edgeProgram->setUniformValue("uEdgeColor", QVector3D(0.9f, 0.3f, 0.3f));
    m_edgeProgram->setUniformValue("uEdgeAlpha", 0.4f);
    glDrawArrays(GL_LINE_LOOP, 4, 4);

    // YZ border -- green
    m_edgeProgram->setUniformValue("uEdgeColor", QVector3D(0.3f, 0.8f, 0.3f));
    m_edgeProgram->setUniformValue("uEdgeAlpha", 0.4f);
    glDrawArrays(GL_LINE_LOOP, 8, 4);

    m_originPlaneVao.release();

    glDisable(GL_LINE_SMOOTH);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    m_edgeProgram->release();
}

void Viewport3D::drawOriginPoint(const QMatrix4x4& mvp)
{
    m_edgeProgram->bind();
    m_edgeProgram->setUniformValue("uMVP", mvp);
    m_edgeProgram->setUniformValue("uDepthBias", 0.0f);
    m_edgeProgram->setUniformValue("uEdgeColor", QVector3D(1.0f, 1.0f, 1.0f));
    m_edgeProgram->setUniformValue("uEdgeAlpha", 1.0f);

    // Draw a single white point at the origin.
    // Vertex 12 in the origin plane VBO is at (0,0,0).
    m_originPlaneVao.bind();

    glPointSize(5.0f);
    glDrawArrays(GL_POINTS, 12, 1);

    m_originPlaneVao.release();
    m_edgeProgram->release();
}

// =============================================================================
// Custom construction plane rendering
// =============================================================================

void Viewport3D::setConstructionPlanes(const std::vector<ConstructionPlaneData>& planes)
{
    m_constructionPlanes = planes;
    update();
}

void Viewport3D::drawConstructionPlanes(const QMatrix4x4& mvp)
{
    if (m_constructionPlanes.empty() || !m_edgeProgram)
        return;

    m_edgeProgram->bind();
    m_edgeProgram->setUniformValue("uDepthBias", 0.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    const float S = 40.0f; // half-extent of construction plane quad

    for (const auto& cp : m_constructionPlanes) {
        // Compute Y direction = normal x xDir
        float yx = cp.normalY * cp.xDirZ - cp.normalZ * cp.xDirY;
        float yy = cp.normalZ * cp.xDirX - cp.normalX * cp.xDirZ;
        float yz = cp.normalX * cp.xDirY - cp.normalY * cp.xDirX;

        // Build model matrix: translate to origin, orient axes
        QMatrix4x4 model;
        model.setToIdentity();
        model.translate(cp.originX, cp.originY, cp.originZ);
        // Set column vectors: xDir, yDir, normal
        float m[16] = {
            cp.xDirX, cp.xDirY, cp.xDirZ, 0,
            yx, yy, yz, 0,
            cp.normalX, cp.normalY, cp.normalZ, 0,
            cp.originX, cp.originY, cp.originZ, 1
        };
        model = QMatrix4x4(m);

        QMatrix4x4 finalMVP = mvp * model;
        m_edgeProgram->setUniformValue("uMVP", finalMVP);

        // Build quad vertices in local XY plane
        float verts[] = {
            -S, -S, 0.0f,
             S, -S, 0.0f,
             S,  S, 0.0f,
            -S,  S, 0.0f,
        };

        // Upload and draw the filled quad (orange tint, semi-transparent)
        QOpenGLVertexArrayObject vao;
        QOpenGLBuffer vbo(QOpenGLBuffer::VertexBuffer);
        vao.create();
        vbo.create();
        vao.bind();
        vbo.bind();
        vbo.allocate(verts, sizeof(verts));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

        // Fill
        m_edgeProgram->setUniformValue("uEdgeColor", QVector3D(0.9f, 0.6f, 0.1f));
        m_edgeProgram->setUniformValue("uEdgeAlpha", 0.15f);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        // Border
        glEnable(GL_LINE_SMOOTH);
        glLineWidth(1.5f);
        m_edgeProgram->setUniformValue("uEdgeColor", QVector3D(1.0f, 0.7f, 0.2f));
        m_edgeProgram->setUniformValue("uEdgeAlpha", 0.5f);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
        glDisable(GL_LINE_SMOOTH);

        vbo.release();
        vao.release();
        vbo.destroy();
        vao.destroy();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    m_edgeProgram->release();
}

// =============================================================================
// Passive sketch rendering (thin gray lines for finished sketches)
// =============================================================================

void Viewport3D::setPassiveSketches(const std::vector<const sketch::Sketch*>& sketches)
{
    m_passiveSketches = sketches;
    update();
}

void Viewport3D::setHighlightedSketch(const sketch::Sketch* sk)
{
    if (m_highlightedSketch != sk) {
        m_highlightedSketch = sk;
        update();
    }
}

void Viewport3D::drawPassiveSketches()
{
    if (m_passiveSketches.empty() || !m_sketchOverlayProgram)
        return;

    // Don't draw passive sketches while actively editing one
    if (m_sketchEditor && m_sketchEditor->isEditing())
        return;

    QMatrix4x4 mvp = projectionMatrix() * viewMatrix();
    m_sketchOverlayProgram->bind();
    m_sketchOverlayProgram->setUniformValue("uMVP", mvp);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.0f);

    // Default: thin gray color for passive sketch geometry
    QVector4D passiveColor(0.5f, 0.6f, 0.8f, 0.5f);
    // Highlighted sketch: bright blue, fully opaque
    QVector4D highlightColor(0.3f, 0.6f, 1.0f, 0.95f);

    QOpenGLVertexArrayObject tmpVao;
    QOpenGLBuffer tmpVbo(QOpenGLBuffer::VertexBuffer);
    tmpVao.create();
    tmpVbo.create();

    for (const auto* sk : m_passiveSketches) {
        if (!sk) continue;

        bool isHighlighted = (sk == m_highlightedSketch);

        // Set color and line width based on highlight state
        if (isHighlighted) {
            m_sketchOverlayProgram->setUniformValue("uColor", highlightColor);
            glLineWidth(2.5f);
        } else {
            m_sketchOverlayProgram->setUniformValue("uColor", passiveColor);
            glLineWidth(1.0f);
        }

        // Collect all line segments
        std::vector<float> verts;

        for (const auto& [lid, ln] : sk->lines()) {
            if (ln.isConstruction) continue;
            const auto& p1 = sk->point(ln.startPointId);
            const auto& p2 = sk->point(ln.endPointId);
            double wx1, wy1, wz1, wx2, wy2, wz2;
            sk->sketchToWorld(p1.x, p1.y, wx1, wy1, wz1);
            sk->sketchToWorld(p2.x, p2.y, wx2, wy2, wz2);
            verts.insert(verts.end(), {(float)wx1,(float)wy1,(float)wz1,
                                       (float)wx2,(float)wy2,(float)wz2});
        }

        // Draw circles as polylines
        for (const auto& [cid, circ] : sk->circles()) {
            if (circ.isConstruction) continue;
            const auto& center = sk->point(circ.centerPointId);
            const int segs = 48;
            for (int i = 0; i < segs; ++i) {
                double a1 = 2.0 * M_PI * i / segs;
                double a2 = 2.0 * M_PI * (i + 1) / segs;
                double sx1 = center.x + circ.radius * std::cos(a1);
                double sy1 = center.y + circ.radius * std::sin(a1);
                double sx2 = center.x + circ.radius * std::cos(a2);
                double sy2 = center.y + circ.radius * std::sin(a2);
                double wx1, wy1, wz1, wx2, wy2, wz2;
                sk->sketchToWorld(sx1, sy1, wx1, wy1, wz1);
                sk->sketchToWorld(sx2, sy2, wx2, wy2, wz2);
                verts.insert(verts.end(), {(float)wx1,(float)wy1,(float)wz1,
                                           (float)wx2,(float)wy2,(float)wz2});
            }
        }

        if (verts.empty()) continue;

        tmpVao.bind();
        tmpVbo.bind();
        tmpVbo.allocate(verts.data(), static_cast<int>(verts.size() * sizeof(float)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(verts.size() / 3));
        tmpVbo.release();
        tmpVao.release();
    }

    tmpVbo.destroy();
    tmpVao.destroy();

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    m_sketchOverlayProgram->release();
}

// =============================================================================
// Sketch overlay rendering (active editing)
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

    // Colors -- per-curve constraint status coloring
    const QVector4D cBlue(0.27f, 0.53f, 1.0f, 1.0f);      // under-constrained
    const QVector4D cDark(0.75f, 0.75f, 0.75f, 1.0f);      // fully constrained (light gray on dark bg)
    const QVector4D cRed(1.0f, 0.25f, 0.25f, 1.0f);        // over-constrained / error
    const QVector4D cOrange(1.0f, 0.53f, 0.0f, 0.9f);      // construction geometry
    const QVector4D cWhite(1.0f, 1.0f, 1.0f, 1.0f);
    const QVector4D cRubberGreen(0.2f, 1.0f, 0.2f, 0.9f);  // bright green preview
    // Grid colors are now defined locally in the grid section below

    // Determine if the whole sketch is fully constrained or solver failed
    bool sketchFullyConstrained = sk->isFullyConstrained();
    sketch::SolveResult lastSolveResult = sk->solve();
    bool solverFailed = (lastSolveResult.status == sketch::SolveStatus::Failed ||
                         lastSolveResult.status == sketch::SolveStatus::OverConstrained);

    // Helper: determine the color for a non-construction curve entity
    // based on its constraint status
    auto curveColor = [&](const std::string& entityId,
                          const std::string& startPtId,
                          const std::string& endPtId) -> QVector4D {
        if (solverFailed) return cRed;
        if (sketchFullyConstrained) return cDark;

        // Count constraints involving this entity or its endpoints
        int constraintCount = 0;
        bool startFixed = false, endFixed = false;
        if (!startPtId.empty()) {
            try { startFixed = sk->point(startPtId).isFixed; } catch (...) {}
        }
        if (!endPtId.empty()) {
            try { endFixed = sk->point(endPtId).isFixed; } catch (...) {}
        }

        for (const auto& [cid, con] : sk->constraints()) {
            for (const auto& eid : con.entityIds) {
                if (eid == entityId || eid == startPtId || eid == endPtId) {
                    constraintCount++;
                    break;
                }
            }
        }

        if (startFixed && endFixed) return cDark;
        if (constraintCount >= 3) return cDark;
        return cBlue;
    };

    // Helper for center-only entities (circles, ellipses)
    auto centerCurveColor = [&](const std::string& entityId,
                                const std::string& centerPtId) -> QVector4D {
        if (solverFailed) return cRed;
        if (sketchFullyConstrained) return cDark;

        bool centerFixed = false;
        try { centerFixed = sk->point(centerPtId).isFixed; } catch (...) {}

        int constraintCount = 0;
        for (const auto& [cid, con] : sk->constraints()) {
            for (const auto& eid : con.entityIds) {
                if (eid == entityId || eid == centerPtId) {
                    constraintCount++;
                    break;
                }
            }
        }

        if (centerFixed && constraintCount >= 2) return cDark;
        if (constraintCount >= 3) return cDark;
        return cBlue;
    };

    // -- Grid: 5mm minor, 25mm major, 200mm extent ─────────────────────────
    {
        constexpr double gridSize      = 5.0;
        constexpr double majorGridSize = 25.0;
        constexpr double extent        = 200.0;

        // Minor grid lines (5mm spacing, very subtle)
        const QVector4D cMinorGrid(0.35f, 0.35f, 0.4f, 0.3f);
        std::vector<float> minorGv;
        for (double g = -extent; g <= extent; g += gridSize) {
            if (std::abs(g) < 0.01) continue;  // skip origin
            // Skip positions that fall on major grid lines
            double rem = std::fmod(std::abs(g), majorGridSize);
            if (rem < 0.01 || rem > majorGridSize - 0.01) continue;
            QVector3D h1 = skToWorld(-extent, g), h2 = skToWorld(extent, g);
            minorGv.push_back(h1.x()); minorGv.push_back(h1.y()); minorGv.push_back(h1.z());
            minorGv.push_back(h2.x()); minorGv.push_back(h2.y()); minorGv.push_back(h2.z());
            QVector3D v1 = skToWorld(g, -extent), v2 = skToWorld(g, extent);
            minorGv.push_back(v1.x()); minorGv.push_back(v1.y()); minorGv.push_back(v1.z());
            minorGv.push_back(v2.x()); minorGv.push_back(v2.y()); minorGv.push_back(v2.z());
        }
        glLineWidth(0.5f);
        uploadAndDraw(minorGv, cMinorGrid, GL_LINES);

        // Major grid lines (25mm spacing, more visible)
        const QVector4D cMajorGrid(0.4f, 0.4f, 0.5f, 0.5f);
        std::vector<float> majorGv;
        for (double g = -extent; g <= extent; g += majorGridSize) {
            if (std::abs(g) < 0.01) continue;  // skip origin
            QVector3D h1 = skToWorld(-extent, g), h2 = skToWorld(extent, g);
            majorGv.push_back(h1.x()); majorGv.push_back(h1.y()); majorGv.push_back(h1.z());
            majorGv.push_back(h2.x()); majorGv.push_back(h2.y()); majorGv.push_back(h2.z());
            QVector3D v1 = skToWorld(g, -extent), v2 = skToWorld(g, extent);
            majorGv.push_back(v1.x()); majorGv.push_back(v1.y()); majorGv.push_back(v1.z());
            majorGv.push_back(v2.x()); majorGv.push_back(v2.y()); majorGv.push_back(v2.z());
        }
        glLineWidth(1.0f);
        uploadAndDraw(majorGv, cMajorGrid, GL_LINES);

        // -- Sketch X axis (red) through origin ──────────────────────────
        {
            const QVector4D cAxisX(1.0f, 0.3f, 0.3f, 0.8f);
            QVector3D ax1 = skToWorld(-extent, 0), ax2 = skToWorld(extent, 0);
            std::vector<float> axv = {ax1.x(), ax1.y(), ax1.z(),
                                      ax2.x(), ax2.y(), ax2.z()};
            glLineWidth(2.0f);
            uploadAndDraw(axv, cAxisX, GL_LINES);
        }
        // -- Sketch Y axis (green) through origin ────────────────────────
        {
            const QVector4D cAxisY(0.3f, 1.0f, 0.3f, 0.8f);
            QVector3D ay1 = skToWorld(0, -extent), ay2 = skToWorld(0, extent);
            std::vector<float> ayv = {ay1.x(), ay1.y(), ay1.z(),
                                      ay2.x(), ay2.y(), ay2.z()};
            glLineWidth(2.0f);
            uploadAndDraw(ayv, cAxisY, GL_LINES);
        }
        // -- Light blue border rectangle around the sketch plane ──────────
        {
            const QVector4D cBorder(0.3f, 0.5f, 0.8f, 0.4f);
            QVector3D b0 = skToWorld(-extent, -extent);
            QVector3D b1 = skToWorld( extent, -extent);
            QVector3D b2 = skToWorld( extent,  extent);
            QVector3D b3 = skToWorld(-extent,  extent);
            std::vector<float> bv = {
                b0.x(), b0.y(), b0.z(), b1.x(), b1.y(), b1.z(),
                b1.x(), b1.y(), b1.z(), b2.x(), b2.y(), b2.z(),
                b2.x(), b2.y(), b2.z(), b3.x(), b3.y(), b3.z(),
                b3.x(), b3.y(), b3.z(), b0.x(), b0.y(), b0.z()
            };
            glLineWidth(1.5f);
            uploadAndDraw(bv, cBorder, GL_LINES);
        }
    }

    // -- Lines (per-curve color coding) ----------------------------------------
    {
        // Group line vertices by color to minimize draw calls
        std::unordered_map<uint32_t, std::vector<float>> colorBuckets;

        auto colorKey = [](const QVector4D& c) -> uint32_t {
            return (static_cast<uint32_t>(c.x() * 255) << 24) |
                   (static_cast<uint32_t>(c.y() * 255) << 16) |
                   (static_cast<uint32_t>(c.z() * 255) << 8)  |
                   static_cast<uint32_t>(c.w() * 255);
        };

        std::unordered_map<uint32_t, QVector4D> colorMap;

        for (const auto& [lid, ln] : sk->lines()) {
            const auto& p1 = sk->point(ln.startPointId);
            const auto& p2 = sk->point(ln.endPointId);
            QVector3D w1 = skToWorld(p1.x, p1.y);
            QVector3D w2 = skToWorld(p2.x, p2.y);

            QVector4D color = ln.isConstruction ? cOrange
                : curveColor(lid, ln.startPointId, ln.endPointId);

            uint32_t key = colorKey(color);
            colorMap[key] = color;
            auto& bucket = colorBuckets[key];
            bucket.push_back(w1.x()); bucket.push_back(w1.y()); bucket.push_back(w1.z());
            bucket.push_back(w2.x()); bucket.push_back(w2.y()); bucket.push_back(w2.z());
        }
        glLineWidth(2.0f);
        for (const auto& [key, verts] : colorBuckets) {
            uploadAndDraw(verts, colorMap[key], GL_LINES);
        }
    }

    // -- Circles (per-curve color coding) --------------------------------------
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
            QVector4D color = circ.isConstruction ? cOrange
                : centerCurveColor(cid, circ.centerPointId);
            glLineWidth(2.0f);
            uploadAndDraw(cv2, color, GL_LINE_LOOP);
        }
    }

    // -- Arcs (per-curve color coding) -----------------------------------------
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
            QVector4D color = arc.isConstruction ? cOrange
                : curveColor(aid, arc.startPointId, arc.endPointId);
            glLineWidth(2.0f);
            uploadAndDraw(av, color, GL_LINES);
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
            QVector4D ellColor = ell.isConstruction ? cOrange
                : centerCurveColor(eid, ell.centerPointId);
            glLineWidth(2.0f);
            uploadAndDraw(ev, ellColor, GL_LINES);
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
            QVector4D splColor = spl.isConstruction ? cOrange : cBlue;
            if (!spl.isConstruction) {
                // Splines: check constraint status via their control points
                if (solverFailed) splColor = cRed;
                else if (sketchFullyConstrained) splColor = cDark;
                else {
                    int cc = 0;
                    for (const auto& [cid2, con2] : sk->constraints()) {
                        for (const auto& eid2 : con2.entityIds) {
                            if (eid2 == sid) { cc++; break; }
                            for (const auto& cpid : spl.controlPointIds) {
                                if (eid2 == cpid) { cc++; break; }
                            }
                        }
                    }
                    if (cc >= 3) splColor = cDark;
                }
            }
            glLineWidth(2.0f);
            uploadAndDraw(sv, splColor, GL_LINES);
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
            uploadAndDraw(rv, cRubberGreen, GL_LINES);
        } else if (tool == SketchTool::DrawRectangle) {
            QVector3D c0 = skToWorld(rx1, ry1), c1 = skToWorld(rx2, ry1);
            QVector3D c2 = skToWorld(rx2, ry2), c3 = skToWorld(rx1, ry2);
            rv = {c0.x(), c0.y(), c0.z(), c1.x(), c1.y(), c1.z(),
                  c1.x(), c1.y(), c1.z(), c2.x(), c2.y(), c2.z(),
                  c2.x(), c2.y(), c2.z(), c3.x(), c3.y(), c3.z(),
                  c3.x(), c3.y(), c3.z(), c0.x(), c0.y(), c0.z()};
            glLineWidth(1.5f);
            uploadAndDraw(rv, cRubberGreen, GL_LINES);
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
            uploadAndDraw(rv, cRubberGreen, GL_LINES);
        } else if (tool == SketchTool::DrawArc) {
            QVector3D wc = skToWorld(rx1, ry1), wp = skToWorld(rx2, ry2);
            rv = {wc.x(), wc.y(), wc.z(), wp.x(), wp.y(), wp.z()};
            glLineWidth(1.5f);
            uploadAndDraw(rv, cRubberGreen, GL_LINES);
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
                uploadAndDraw(rv, cRubberGreen, GL_LINES);
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
            uploadAndDraw(rv, cRubberGreen, GL_LINES);
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
            uploadAndDraw(rv, cRubberGreen, GL_LINES);
        } else if (tool == SketchTool::DrawSlot) {
            // Slot rubber-band: show center1-center2 axis line + cursor
            QVector3D w1 = skToWorld(rx1, ry1), w2 = skToWorld(rx2, ry2);
            rv = {w1.x(), w1.y(), w1.z(), w2.x(), w2.y(), w2.z()};
            glLineWidth(1.5f);
            uploadAndDraw(rv, cRubberGreen, GL_LINES);
        } else if (tool == SketchTool::DrawCircle3Point ||
                   tool == SketchTool::DrawArc3Point) {
            // Show guide lines between clicked points and cursor
            QVector3D wc = skToWorld(rx1, ry1), wp = skToWorld(rx2, ry2);
            rv = {wc.x(), wc.y(), wc.z(), wp.x(), wp.y(), wp.z()};
            glLineWidth(1.5f);
            uploadAndDraw(rv, cRubberGreen, GL_LINES);
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
            uploadAndDraw(rv, cRubberGreen, GL_LINES);
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
    QMatrix4x4 view = m_camera->viewMatrix();
    QMatrix4x4 proj = projectionMatrix();
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

    // ── Profile region shading (semi-transparent blue for closed loops) ──
    {
        auto profiles = sk->detectProfiles();
        for (const auto& profile : profiles) {
            std::vector<QPointF> boundary;
            for (const auto& curveId : profile) {
                // Try line
                try {
                    const auto& ln = sk->line(curveId);
                    if (ln.isConstruction) continue;
                    const auto& p1 = sk->point(ln.startPointId);
                    const auto& p2 = sk->point(ln.endPointId);
                    boundary.push_back(skToScreen(p1.x, p1.y));
                    boundary.push_back(skToScreen(p2.x, p2.y));
                    continue;
                } catch (...) {}
                // Try circle (full circle = closed profile by itself)
                try {
                    const auto& circ = sk->circle(curveId);
                    if (circ.isConstruction) continue;
                    const auto& cp = sk->point(circ.centerPointId);
                    constexpr int N = 48;
                    for (int i = 0; i < N; ++i) {
                        double angle = 2.0 * M_PI * i / N;
                        double px = cp.x + circ.radius * std::cos(angle);
                        double py = cp.y + circ.radius * std::sin(angle);
                        boundary.push_back(skToScreen(px, py));
                    }
                    continue;
                } catch (...) {}
                // Try arc
                try {
                    const auto& a = sk->arc(curveId);
                    if (a.isConstruction) continue;
                    const auto& cp = sk->point(a.centerPointId);
                    const auto& sp = sk->point(a.startPointId);
                    const auto& ep = sk->point(a.endPointId);
                    double startAngle = std::atan2(sp.y - cp.y, sp.x - cp.x);
                    double endAngle   = std::atan2(ep.y - cp.y, ep.x - cp.x);
                    // Ensure CCW sweep
                    if (endAngle <= startAngle) endAngle += 2.0 * M_PI;
                    double sweep = endAngle - startAngle;
                    constexpr int N = 32;
                    for (int i = 0; i <= N; ++i) {
                        double t = startAngle + sweep * i / N;
                        double px = cp.x + a.radius * std::cos(t);
                        double py = cp.y + a.radius * std::sin(t);
                        boundary.push_back(skToScreen(px, py));
                    }
                    continue;
                } catch (...) {}
            }

            if (boundary.size() >= 3) {
                QPainterPath path;
                path.moveTo(boundary[0]);
                for (size_t i = 1; i < boundary.size(); ++i)
                    path.lineTo(boundary[i]);
                path.closeSubpath();

                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(42, 130, 218, 25));
                painter.drawPath(path);
            }
        }
    }

    const auto& constraints = sk->constraints();
    if (constraints.empty() && !m_sketchEditor->isDragging() &&
        !m_sketchEditor->hasFirstPick()) {
        painter.end();
        return;
    }

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
    // Get selected constraint ID for highlight
    const std::string selectedConId = m_sketchEditor ? m_sketchEditor->selectedConstraintId() : "";

    for (const auto& [cid, con] : constraints) {
        using CT = sketch::ConstraintType;

        // If this constraint is selected, draw a highlight ring around its marker
        const bool isSelected = (!selectedConId.empty() && cid == selectedConId);

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

                // Selection highlight: bright ring around the label
                if (isSelected) {
                    painter.setPen(QPen(QColor(255, 100, 100), 2.5));
                    painter.setBrush(Qt::NoBrush);
                    QFontMetrics fm(dimFont);
                    QRectF selRect(dimMid.x() - fm.horizontalAdvance(txt) / 2.0 - 6,
                                   dimMid.y() - fm.height() / 2.0 - 4,
                                   fm.horizontalAdvance(txt) + 12, fm.height() + 8);
                    painter.drawRoundedRect(selRect, 4, 4);
                }
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

                if (isSelected) {
                    painter.setPen(QPen(QColor(255, 100, 100), 2.5));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRoundedRect(iconRect.adjusted(-3, -3, 3, 3), 4, 4);
                }
            } catch (...) {
                continue;
            }

        } else if (con.type == CT::Tangent || con.type == CT::Equal) {
            // Draw a labeled marker at the midpoint of the first entity (line/circle/arc)
            if (con.entityIds.empty()) continue;
            try {
                double mx = 0, my = 0;
                bool found = false;
                // Try line first
                try {
                    const auto& ln = sk->line(con.entityIds[0]);
                    const auto& p1 = sk->point(ln.startPointId);
                    const auto& p2 = sk->point(ln.endPointId);
                    mx = (p1.x + p2.x) / 2.0;
                    my = (p1.y + p2.y) / 2.0;
                    found = true;
                } catch (...) {}
                // Try circle
                if (!found) try {
                    const auto& circ = sk->circle(con.entityIds[0]);
                    const auto& cp = sk->point(circ.centerPointId);
                    mx = cp.x; my = cp.y + circ.radius;
                    found = true;
                } catch (...) {}
                // Try arc
                if (!found) try {
                    const auto& a = sk->arc(con.entityIds[0]);
                    const auto& cp = sk->point(a.centerPointId);
                    mx = cp.x; my = cp.y;
                    found = true;
                } catch (...) {}
                // Fall back to point
                if (!found) {
                    const auto& pt = sk->point(con.entityIds[0]);
                    mx = pt.x; my = pt.y;
                }

                QPointF sMid = skToScreen(mx, my);
                sMid.setY(sMid.y() - 14.0);

                QString sym = (con.type == CT::Tangent) ? "T" : "=";

                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(60, 30, 100, 180));
                painter.drawRoundedRect(QRectF(sMid.x() - 10, sMid.y() - 8, 20, 16), 3, 3);

                painter.setFont(iconFont);
                painter.setPen(cConMarker);
                QRectF iconRect(sMid.x() - 10, sMid.y() - 8, 20, 16);
                painter.drawText(iconRect, Qt::AlignCenter, sym);

                if (isSelected) {
                    painter.setPen(QPen(QColor(255, 100, 100), 2.5));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRoundedRect(iconRect.adjusted(-3, -3, 3, 3), 4, 4);
                }
            } catch (...) {
                continue;
            }

        } else if (con.type == CT::Coincident || con.type == CT::PointOnLine ||
                   con.type == CT::PointOnCircle || con.type == CT::Concentric) {
            // Draw a small dot marker at the first entity (point)
            if (con.entityIds.empty()) continue;
            try {
                const auto& pt = sk->point(con.entityIds[0]);
                QPointF sPt = skToScreen(pt.x, pt.y);
                painter.setPen(Qt::NoPen);
                painter.setBrush(cConMarker);
                painter.drawEllipse(sPt, 4.0, 4.0);

                // For Concentric, draw a second concentric ring
                if (con.type == CT::Concentric) {
                    painter.setPen(QPen(cConMarker, 1.5));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawEllipse(sPt, 7.0, 7.0);
                }

                if (isSelected) {
                    painter.setPen(QPen(QColor(255, 100, 100), 2.5));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawEllipse(sPt, 10.0, 10.0);
                }
            } catch (...) {
                continue;
            }

        } else if (con.type == CT::Symmetric || con.type == CT::Midpoint) {
            // Draw a labeled marker at the constrained point
            if (con.entityIds.empty()) continue;
            try {
                const auto& pt = sk->point(con.entityIds[0]);
                QPointF sPt = skToScreen(pt.x, pt.y);
                sPt.setY(sPt.y() - 14.0);

                QString sym = (con.type == CT::Symmetric)
                    ? QString(QChar(0x2225))   // parallel bars for symmetry
                    : QString("M");            // midpoint

                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(60, 30, 100, 180));
                painter.drawRoundedRect(QRectF(sPt.x() - 10, sPt.y() - 8, 20, 16), 3, 3);

                painter.setFont(iconFont);
                painter.setPen(cConMarker);
                QRectF iconRect(sPt.x() - 10, sPt.y() - 8, 20, 16);
                painter.drawText(iconRect, Qt::AlignCenter, sym);

                if (isSelected) {
                    painter.setPen(QPen(QColor(255, 100, 100), 2.5));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRoundedRect(iconRect.adjusted(-3, -3, 3, 3), 4, 4);
                }
            } catch (...) {
                continue;
            }

        } else if (con.type == CT::Fix) {
            // Draw a small square at the fixed point
            if (con.entityIds.empty()) continue;
            try {
                const auto& pt = sk->point(con.entityIds[0]);
                QPointF sPt = skToScreen(pt.x, pt.y);

                painter.setPen(QPen(cConMarker, 1.5));
                painter.setBrush(QColor(60, 30, 100, 140));
                painter.drawRect(QRectF(sPt.x() - 5, sPt.y() - 5, 10, 10));

                if (isSelected) {
                    painter.setPen(QPen(QColor(255, 100, 100), 2.5));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRect(QRectF(sPt.x() - 8, sPt.y() - 8, 16, 16));
                }
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
// Welcome overlay (shown when viewport is empty)
// =============================================================================

void Viewport3D::drawWelcomeOverlay()
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const int w = width();
    const int h = height();

    // Title
    QFont titleFont = painter.font();
    titleFont.setPixelSize(22);
    titleFont.setWeight(QFont::Light);
    painter.setFont(titleFont);
    painter.setPen(QColor(180, 180, 180));
    painter.drawText(QRect(0, h / 2 - 60, w, 30), Qt::AlignCenter, "kernelCAD");

    // Subtitle with shortcuts
    QFont subFont = painter.font();
    subFont.setPixelSize(13);
    subFont.setWeight(QFont::Normal);
    painter.setFont(subFont);
    painter.setPen(QColor(120, 120, 120));
    painter.drawText(QRect(0, h / 2 - 25, w, 20), Qt::AlignCenter,
                     "Press S to create a sketch  |  Ctrl+O to open a file  |  Ctrl+N for new document");

    // Keyboard shortcut hints
    QFont hintFont = subFont;
    hintFont.setPixelSize(11);
    painter.setFont(hintFont);
    painter.setPen(QColor(100, 100, 100));
    painter.drawText(QRect(0, h / 2 + 10, w, 16), Qt::AlignCenter,
                     "Click an origin plane to start sketching");

    painter.end();
}

// ViewCube overlay
// =============================================================================

/// Helper: extract the 3x3 camera rotation as a QMatrix4x4.
static QMatrix4x4 cameraViewRotation(CameraController* cam)
{
    QMatrix4x4 viewMat;
    viewMat.lookAt(cam->eye(), cam->center(), cam->up());
    QMatrix4x4 viewRot;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            viewRot(r, c) = viewMat(r, c);
    return viewRot;
}

void Viewport3D::drawViewCubeOverlay()
{
    QMatrix4x4 viewRot = cameraViewRotation(m_camera);
    QPainter painter(this);
    ViewportOverlays::drawViewCube(painter, viewRot, width(), height(),
                                   kViewCubeSize, kViewCubeMargin,
                                   m_viewCubeHoveredFace,
                                   m_camera->isPerspective());
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

bool Viewport3D::isInViewCubeArea(const QPoint& pos) const
{
    return ViewportOverlays::isInViewCubeArea(pos, width(), height(),
                                              kViewCubeSize, kViewCubeMargin);
}

bool Viewport3D::handleViewCubeClick(const QPoint& pos)
{
    QMatrix4x4 viewRot = cameraViewRotation(m_camera);
    QVector3D direction, up;
    int face = ViewportOverlays::handleViewCubeClick(pos, viewRot, width(), height(),
                                                      kViewCubeSize, kViewCubeMargin,
                                                      direction, up);
    if (face >= 0) {
        m_camera->setStandardView(direction, up);
        return true;
    }
    return false;
}

int Viewport3D::hitTestViewCubeFace(const QPoint& pos) const
{
    QMatrix4x4 viewRot = cameraViewRotation(m_camera);
    return ViewportOverlays::hitTestViewCubeFace(pos, viewRot, width(), height(),
                                                  kViewCubeSize, kViewCubeMargin);
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
    m_program->setUniformValue("uViewPos",     m_camera->eye());
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
// Smooth camera animation -- delegate to CameraController
// =============================================================================

void Viewport3D::animateTo(const QVector3D& targetEye, const QVector3D& targetCenter,
                            const QVector3D& targetUp, int durationMs)
{
    m_camera->animateTo(targetEye, targetCenter, targetUp, durationMs);
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

    // ── Snap-to-point indicators (BRIGHT ORANGE circle with glow) ──────
    constexpr double snapThreshold = 5.0;   // sketch-space units
    constexpr double alignThreshold = 1.5;  // sketch-space units for H/V alignment
    constexpr double gridSize = 5.0;        // must match the grid spacing

    const QColor cSnapPoint(255, 140, 0, 255);    // bright orange
    const QColor cSnapGlow(255, 160, 0, 80);      // subtle glow around snap
    const QColor cSnapGrid(60, 140, 255, 230);    // blue for grid snap
    const QColor cAlignLine(60, 220, 60, 160);    // bright green dashed
    const QColor cAlignText(140, 255, 140, 220);
    const QColor cDimLive(200, 230, 255, 240);
    const QColor cDimLiveBg(30, 30, 30, 180);

    bool snappedToPoint = false;

    for (const auto& [id, pt] : sk->points()) {
        double dx = cursorX - pt.x;
        double dy = cursorY - pt.y;
        double distSq = dx * dx + dy * dy;

        // Point snap indicator: bright orange circle with glow
        if (distSq < snapThreshold * snapThreshold) {
            QPointF screenPt = skToScreen(pt.x, pt.y);
            // Outer glow
            painter.setPen(Qt::NoPen);
            painter.setBrush(cSnapGlow);
            painter.drawEllipse(screenPt, 14.0, 14.0);
            // Inner bright circle
            painter.setPen(QPen(cSnapPoint, 2.5));
            painter.setBrush(QColor(255, 140, 0, 100));
            painter.drawEllipse(screenPt, 8.0, 8.0);
            // Center dot
            painter.setPen(Qt::NoPen);
            painter.setBrush(cSnapPoint);
            painter.drawEllipse(screenPt, 3.0, 3.0);
            snappedToPoint = true;
        }
    }

    // ── Snap type tooltip (small label near the snap indicator) ───────────
    if (m_sketchEditor->snapType() != SnapType::None) {
        QPointF screenPos = skToScreen(m_sketchEditor->snapX(), m_sketchEditor->snapY());

        QString label;
        switch (m_sketchEditor->snapType()) {
            case SnapType::Endpoint: label = QStringLiteral("Endpoint"); break;
            case SnapType::Midpoint: label = QStringLiteral("Midpoint"); break;
            case SnapType::Center:   label = QStringLiteral("Center"); break;
            case SnapType::OnEdge:   label = QStringLiteral("On Edge"); break;
            default: break;
        }

        if (!label.isEmpty()) {
            QFont tooltipFont("", 8);
            painter.setFont(tooltipFont);
            QRect textRect = painter.fontMetrics().boundingRect(label);
            textRect.moveTopLeft(screenPos.toPoint() + QPoint(14, -8));
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(40, 40, 40, 210));
            painter.drawRoundedRect(textRect.adjusted(-4, -2, 4, 2), 3, 3);
            painter.setPen(QColor(220, 220, 220));
            painter.drawText(textRect, Qt::AlignCenter, label);
        }
    }

    // ── Snap-to-grid indicator (BLUE cross at snapped grid intersection) ─
    if (!snappedToPoint) {
        double snappedX = std::round(cursorX / gridSize) * gridSize;
        double snappedY = std::round(cursorY / gridSize) * gridSize;
        double gdx = cursorX - snappedX;
        double gdy = cursorY - snappedY;
        if (gdx * gdx + gdy * gdy < (gridSize * 0.6) * (gridSize * 0.6)) {
            QPointF screenSnap = skToScreen(snappedX, snappedY);
            constexpr double crossSize = 6.0;
            painter.setPen(QPen(cSnapGrid, 2.0));
            painter.setBrush(Qt::NoBrush);
            // Draw a cross (+) at the grid intersection
            painter.drawLine(QPointF(screenSnap.x() - crossSize, screenSnap.y()),
                             QPointF(screenSnap.x() + crossSize, screenSnap.y()));
            painter.drawLine(QPointF(screenSnap.x(), screenSnap.y() - crossSize),
                             QPointF(screenSnap.x(), screenSnap.y() + crossSize));
        }
    }

    // ── H/V alignment indicators (DASHED GREEN lines + H/V label) ───────
    if (!snappedToPoint) {
        QFont alignFont("Monospace", 9, QFont::Bold);
        alignFont.setStyleHint(QFont::Monospace);
        painter.setFont(alignFont);

        for (const auto& [id, pt] : sk->points()) {
            // Horizontal alignment
            if (std::abs(cursorY - pt.y) < alignThreshold) {
                QPointF s1 = skToScreen(pt.x, pt.y);
                QPointF s2 = skToScreen(cursorX, cursorY);
                QPen dashPen(cAlignLine, 1.5, Qt::DashLine);
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
                QPen dashPen(cAlignLine, 1.5, Qt::DashLine);
                painter.setPen(dashPen);
                painter.drawLine(s1, s2);

                // "V" label at midpoint
                QPointF mid((s1.x() + s2.x()) / 2.0 + 8.0, (s1.y() + s2.y()) / 2.0);
                painter.setPen(cAlignText);
                painter.drawText(mid, "V");
            }
        }
    }

    // ── Inference guide lines during rubber-band drawing ────────────────
    if (m_sketchEditor->isDrawingInProgress()) {
        double sx = m_sketchEditor->rubberStartX();
        double sy = m_sketchEditor->rubberStartY();
        double cx = cursorX;
        double cy = cursorY;

        double angle = std::atan2(std::abs(cy - sy), std::abs(cx - sx)) * 180.0 / M_PI;
        QPen guidePen(QColor(100, 200, 100, 150), 0.5, Qt::DotLine);

        // H/V guide from the rubber-band start point
        if (angle < 5.0 && std::abs(cx - sx) > 1.0) {
            // Horizontal guide across viewport
            QPointF left  = skToScreen(std::min(sx, cx) - 200.0, sy);
            QPointF right = skToScreen(std::max(sx, cx) + 200.0, sy);
            painter.setPen(guidePen);
            painter.drawLine(left, right);
        }
        if (std::abs(angle - 90.0) < 5.0 && std::abs(cy - sy) > 1.0) {
            // Vertical guide across viewport
            QPointF top = skToScreen(sx, std::max(sy, cy) + 200.0);
            QPointF bot = skToScreen(sx, std::min(sy, cy) - 200.0);
            painter.setPen(guidePen);
            painter.drawLine(top, bot);
        }

        // Alignment guides with existing points (beyond the snap system)
        for (const auto& [pid, pt] : sk->points()) {
            // Skip the start point itself
            if (std::abs(pt.x - sx) < 0.01 && std::abs(pt.y - sy) < 0.01)
                continue;
            // Vertical alignment between cursor and existing point
            if (std::abs(cx - pt.x) < 0.5 && std::abs(cy - pt.y) > 1.0) {
                QPointF p1 = skToScreen(pt.x, std::min(cy, pt.y) - 10.0);
                QPointF p2 = skToScreen(pt.x, std::max(cy, pt.y) + 10.0);
                painter.setPen(QPen(QColor(100, 200, 100, 120), 0.5, Qt::DotLine));
                painter.drawLine(p1, p2);
            }
            // Horizontal alignment between cursor and existing point
            if (std::abs(cy - pt.y) < 0.5 && std::abs(cx - pt.x) > 1.0) {
                QPointF p1 = skToScreen(std::min(cx, pt.x) - 10.0, pt.y);
                QPointF p2 = skToScreen(std::max(cx, pt.x) + 10.0, pt.y);
                painter.setPen(QPen(QColor(100, 200, 100, 120), 0.5, Qt::DotLine));
                painter.drawLine(p1, p2);
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

// =============================================================================
// Box selection (rubber-band rectangle)
// =============================================================================

void Viewport3D::drawBoxSelectOverlay()
{
    QRect rect = QRect(m_boxSelectStart, m_boxSelectEnd).normalized();
    if (rect.width() < 2 && rect.height() < 2) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Semi-transparent blue fill
    painter.setBrush(QColor(42, 130, 218, 30));
    // Dashed blue border
    QPen pen(QColor(42, 130, 218, 180), 1.5, Qt::DashLine);
    painter.setPen(pen);
    painter.drawRect(rect);
    painter.end();
}

void Viewport3D::performBoxSelect(const QRect& rect)
{
    if (!m_selectionMgr) return;

    m_selectionMgr->clearSelection();

    // Sample the pick FBO at a grid of points inside the rectangle
    // and collect all unique face IDs
    std::set<int> hitFaceIds;
    const int step = 8; // sample every 8 pixels

    for (int y = rect.top(); y <= rect.bottom(); y += step) {
        for (int x = rect.left(); x <= rect.right(); x += step) {
            int faceId = pickAtScreenPos(QPoint(x, y));
            if (faceId > 0)
                hitFaceIds.insert(faceId - 1);
        }
    }

    // Create selection hits for each unique face
    for (int faceIdx : hitFaceIds) {
        SelectionHit hit;
        hit.faceIndex = faceIdx;
        if (faceIdx < static_cast<int>(m_bodyIdPerFace.size()))
            hit.bodyId = m_bodyIdPerFace[faceIdx];
        m_selectionMgr->addToSelection(hit);
    }
}
