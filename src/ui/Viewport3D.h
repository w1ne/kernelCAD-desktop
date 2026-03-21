#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLFramebufferObject>
#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <QPoint>
#include <QRectF>
#include <QTimer>

#include <vector>
#include <cstdint>
#include <memory>
#include <string>

class SelectionManager;
class SketchEditor;
class ViewportManipulator;

/// Per-body rendering data for multi-body display with distinct colors.
struct BodyRenderData {
    std::string bodyId;
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> faceIds;    // per-triangle face index
    float colorR = 0.6f, colorG = 0.65f, colorB = 0.7f;
    bool isVisible = true;
    bool hasError  = false;   ///< True if any feature on this body is in error state

    // Edge data for this body
    std::vector<float> edgeVertices;
    std::vector<uint32_t> edgeIndices;
};

/// Standard camera view presets (CAD numpad convention).
enum class StandardView {
    Front, Back, Left, Right, Top, Bottom, Isometric
};

/// View mode for 3D rendering.
enum class ViewMode {
    SolidWithEdges,   ///< Solid faces + edge lines (default, like real CAD)
    Solid,            ///< Solid faces only (no edges)
    Wireframe,        ///< Edge lines only (no faces)
    HiddenLine        ///< Edges only, with hidden-line removal via depth buffer
};

/// 3D viewport — OpenGL surface for rendering B-Rep tessellations.
/// Uses Qt6 OpenGLWidgets with OpenGL 3.3 Core for modern GPU rendering.
/// Provides an arcball orbit camera with mouse controls:
///   left-drag = rotate, middle-drag = pan, wheel = zoom.
/// Supports GPU color-based picking for face/edge selection.
class Viewport3D : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT
public:
    explicit Viewport3D(QWidget* parent = nullptr);
    ~Viewport3D() override;

    /// Upload triangle-mesh data to GPU (VAO/VBO/EBO).
    /// @param vertices  flat x,y,z triples
    /// @param normals   flat x,y,z triples (same count as vertices)
    /// @param indices   triangle index list
    void setMesh(const std::vector<float>& vertices,
                 const std::vector<float>& normals,
                 const std::vector<uint32_t>& indices);

    /// Upload per-body mesh data for multi-body rendering with distinct colors.
    /// This is the primary rendering path; setMesh() is kept as a fallback.
    void setBodies(const std::vector<BodyRenderData>& bodies);

    /// Upload edge line data to GPU for wireframe rendering.
    /// @param edgeVertices  flat x,y,z positions of edge polyline points
    /// @param edgeIndices   line segment index pairs (GL_LINES)
    void setEdges(const std::vector<float>& edgeVertices,
                  const std::vector<uint32_t>& edgeIndices);

    /// Upload per-triangle face-index data for GPU picking.
    /// @param faceIds  one int per triangle — the B-Rep face index that triangle belongs to
    /// @param bodyIds  one string per face index — maps face index to body ID
    void setFaceMap(const std::vector<int>& faceIds,
                    const std::vector<std::string>& bodyIds);

    /// Frame the camera so the entire mesh bounding-box is visible.
    void fitAll();

    /// Set the selection manager for pick/hover interaction.
    void setSelectionManager(SelectionManager* mgr);

    /// Set highlighted face indices (rendered with a blue tint overlay).
    void setHighlightedFaces(const std::vector<int>& faceIndices);

    /// Set the view/render mode.
    void setViewMode(ViewMode mode);
    ViewMode viewMode() const;

    /// Query current projection mode.
    bool isPerspective() const { return m_perspectiveProjection; }

    /// Set a standard view preset (Front, Back, Left, Right, Top, Bottom, Isometric).
    void setStandardView(StandardView view);

    /// Animate the camera to target position over durationMs milliseconds.
    void animateTo(const QVector3D& targetEye, const QVector3D& targetCenter,
                   const QVector3D& targetUp, int durationMs = 300);

    /// Set the sketch editor for overlay rendering and input delegation.
    void setSketchEditor(SketchEditor* editor);

    /// Enter/exit sketch editing mode (ghost bodies, lock rotation, disable body picking).
    void setSketchMode(bool enabled);
    bool isSketchMode() const { return m_sketchModeActive; }

    /// Current orbit distance (for camera positioning).
    float orbitDistance() const { return m_orbitDistance; }

    /// Save/restore camera state (used when entering/leaving sketch mode).
    void saveCameraState();
    void restoreCameraState(bool animate = true);

signals:
    /// Emitted when Ctrl+drag moves a selected body in the viewport.
    void occurrenceDragged(float dx, float dy, float dz);

public:
    /// Access camera matrices (needed by SketchEditor for ray-plane intersection).
    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projectionMatrix() const;

    /// Enable/disable a section (clipping) plane.
    void setSectionPlane(bool enabled,
                         float planeX = 0, float planeY = 0, float planeZ = 1,
                         float planeD = 0);

    /// Query section plane state.
    bool sectionPlaneEnabled() const { return m_clipEnabled; }
    float sectionPlaneD() const { return m_clipPlane.w(); }

    /// Upload a preview mesh to render as a semi-transparent blue overlay.
    /// Used by the PreviewEngine during live parameter editing.
    void setPreviewMesh(const std::vector<float>& verts,
                        const std::vector<float>& normals,
                        const std::vector<uint32_t>& indices);

    /// Clear the preview mesh overlay.
    void clearPreviewMesh();

    /// Set the explode factor for assembly exploded view.
    /// 0 = assembled (default), 1 = fully exploded.
    void setExplodeFactor(float factor);
    float explodeFactor() const { return m_explodeFactor; }

    /// Toggle ground grid and origin axes visibility.
    void setShowGrid(bool show);
    bool showGrid() const { return m_showGrid; }

    /// Toggle origin planes (XY/XZ/YZ reference planes + origin point) visibility.
    void setShowOrigin(bool show);
    bool showOrigin() const { return m_showOrigin; }

    /// Set the viewport manipulator (owned externally, e.g. by MainWindow).
    void setManipulator(ViewportManipulator* manipulator);
    ViewportManipulator* manipulator() const { return m_manipulator; }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    // ── helpers ──────────────────────────────────────────────────────────
    void buildShaderProgram();
    void buildPickShaderProgram();
    void buildEdgeShaderProgram();
    QVector3D arcballVector(const QPoint& screenPos) const;

    /// Perform a GPU pick at the given screen position.
    /// Returns the decoded face ID (0 = background/miss, >=1 = face id+1).
    int pickAtScreenPos(const QPoint& pos);

    /// Build a SelectionHit from a face index and screen position.
    void handlePick(const QPoint& screenPos, bool addToSelection);

    /// Perform pre-selection (hover highlight) at the given screen position.
    void handlePreSelection(const QPoint& screenPos);

    /// Ensure the pick FBO exists and matches the widget size.
    void ensurePickFBO();

    // ── shader programs ─────────────────────────────────────────────────
    QOpenGLShaderProgram* m_program     = nullptr;
    QOpenGLShaderProgram* m_pickProgram = nullptr;
    QOpenGLShaderProgram* m_edgeProgram = nullptr;

    // ── GPU buffers ─────────────────────────────────────────────────────
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vboPos{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_vboNorm{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_vboFaceId{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_ebo{QOpenGLBuffer::IndexBuffer};
    GLsizei m_indexCount = 0;
    bool m_meshLoaded = false;

    // ── per-vertex face ID buffer for picking ───────────────────────────
    std::vector<int> m_faceIdPerTriangle;   // one face ID per triangle
    std::vector<std::string> m_bodyIdPerFace;  // maps face index -> body ID
    bool m_faceMapLoaded = false;

    // ── per-body GPU buffers for multi-body rendering ────────────────────
    struct BodyGPU {
        std::string bodyId;
        float colorR = 0.6f, colorG = 0.65f, colorB = 0.7f;
        bool isVisible = true;
        bool hasError  = false;
        QOpenGLVertexArrayObject vao;
        QOpenGLBuffer vboPos{QOpenGLBuffer::VertexBuffer};
        QOpenGLBuffer vboNorm{QOpenGLBuffer::VertexBuffer};
        QOpenGLBuffer vboFaceId{QOpenGLBuffer::VertexBuffer};
        QOpenGLBuffer ebo{QOpenGLBuffer::IndexBuffer};
        GLsizei indexCount = 0;
        // Edge buffers
        QOpenGLVertexArrayObject edgeVao;
        QOpenGLBuffer edgeVboPos{QOpenGLBuffer::VertexBuffer};
        QOpenGLBuffer edgeEbo{QOpenGLBuffer::IndexBuffer};
        GLsizei edgeIndexCount = 0;
    };
    std::vector<std::unique_ptr<BodyGPU>> m_bodyGPUs;
    bool m_bodiesLoaded = false;

    /// Distinct per-body color palette.
    static const float kBodyColors[][3];

    // ── edge GPU buffers ────────────────────────────────────────────────
    QOpenGLVertexArrayObject m_edgeVao;
    QOpenGLBuffer m_edgeVboPos{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_edgeEbo{QOpenGLBuffer::IndexBuffer};
    GLsizei m_edgeIndexCount = 0;
    bool m_edgesLoaded = false;

    // ── exploded view ─────────────────────────────────────────────────
    float m_explodeFactor = 0.0f;

    // ── view mode ───────────────────────────────────────────────────────
    ViewMode m_viewMode = ViewMode::SolidWithEdges;

    // ── highlighted faces ───────────────────────────────────────────────
    std::vector<int> m_highlightedFaces;    // face indices to highlight

    // ── off-screen FBO for picking ──────────────────────────────────────
    std::unique_ptr<QOpenGLFramebufferObject> m_pickFBO;
    float m_lastPickDepth = 1.0f;  // depth value from last pick (for unproject)

    // ── selection manager ───────────────────────────────────────────────
    SelectionManager* m_selectionMgr = nullptr;

    // ── camera state ────────────────────────────────────────────────────
    QVector3D m_eye    {0.0f, 0.0f, 5.0f};
    QVector3D m_center {0.0f, 0.0f, 0.0f};
    QVector3D m_up     {0.0f, 1.0f, 0.0f};
    float m_fov  = 45.0f;
    float m_near = 0.01f;
    float m_far  = 1000.0f;

    // ── bounding box (set by setMesh) ───────────────────────────────────
    QVector3D m_bboxMin;
    QVector3D m_bboxMax;

    // ── mouse interaction ───────────────────────────────────────────────
    QPoint m_lastMousePos;
    QPoint m_mousePressPos;        // initial click position (for drag detection)
    Qt::MouseButton m_activeButton = Qt::NoButton;
    bool m_isDragging = false;
    static constexpr int kDragThreshold = 5;  // pixels

    // orbit distance (maintained for zoom)
    float m_orbitDistance = 5.0f;

    // ── cached vertex data for unproject ────────────────────────────────
    std::vector<float> m_vertexData;

    // ── sketch overlay ──────────────────────────────────────────────────
    SketchEditor* m_sketchEditor = nullptr;
    QOpenGLShaderProgram* m_sketchOverlayProgram = nullptr;

    void buildSketchOverlayShader();
    void drawSketchOverlay();

    /// Draw dimension text, constraint markers, and drag highlight using QPainter.
    /// Called after drawSketchOverlay() to layer 2D text on the GL surface.
    void drawSketchConstraintOverlay();

    /// Project a 3D world point to 2D widget coordinates.
    QPointF worldToScreen(const QVector3D& worldPt) const;

    // ── section / clipping plane ────────────────────────────────────────
    bool m_clipEnabled = false;
    QVector4D m_clipPlane{0.0f, 0.0f, 1.0f, 0.0f};  // (nx, ny, nz, d)

    QOpenGLShaderProgram* m_sectionQuadProgram = nullptr;
    QOpenGLVertexArrayObject m_sectionQuadVao;
    QOpenGLBuffer m_sectionQuadVbo{QOpenGLBuffer::VertexBuffer};

    void buildSectionQuadShader();
    void drawSectionQuad(const QMatrix4x4& view, const QMatrix4x4& projection);

    // ── ground grid & origin axes ──────────────────────────────────────
    bool m_showGrid = true;
    QOpenGLVertexArrayObject m_gridVao;
    QOpenGLBuffer m_gridVbo{QOpenGLBuffer::VertexBuffer};
    GLsizei m_gridMinorVertexCount = 0;   // number of minor-line vertices
    GLsizei m_gridMajorVertexCount = 0;   // number of major-line vertices
    GLsizei m_gridAxesVertexCount  = 0;   // number of origin-axis vertices
    bool m_gridInitialized = false;

    void initGridBuffers();
    void drawGrid(const QMatrix4x4& mvp);
    void drawOriginAxes(const QMatrix4x4& mvp);

    // ── origin planes (XY, XZ, YZ reference planes) ─────────────────────
    bool m_showOrigin = true;
    QOpenGLVertexArrayObject m_originPlaneVao;
    QOpenGLBuffer m_originPlaneVbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_originPlaneEbo{QOpenGLBuffer::IndexBuffer};
    bool m_originPlanesInitialized = false;

    void initOriginPlanes();
    void drawOriginPlanes(const QMatrix4x4& mvp);
    void drawOriginPoint(const QMatrix4x4& mvp);

    // ── standard views & ViewCube ───────────────────────────────────────
    /// Position camera along `direction` at m_orbitDistance from m_center.
    void setStandardView(const QVector3D& direction, const QVector3D& up);

    /// Build the projection matrix (perspective or ortho) into `out`.
    void buildProjectionMatrix(QMatrix4x4& out) const;

    /// Draw a small ViewCube overlay in the top-right corner using QPainter.
    void drawViewCubeOverlay();

    /// Handle a click inside the ViewCube area. Returns true if consumed.
    bool handleViewCubeClick(const QPoint& pos);

    bool m_perspectiveProjection = true;

    // ViewCube constants
    static constexpr int kViewCubeSize   = 100; // cube area in pixels
    static constexpr int kViewCubeMargin =  10; // margin from top-right corner

    // ── preview mesh overlay ───────────────────────────────────────────
    QOpenGLVertexArrayObject m_previewVao;
    QOpenGLBuffer m_previewVboPos{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_previewVboNorm{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_previewEbo{QOpenGLBuffer::IndexBuffer};
    GLsizei m_previewIndexCount = 0;
    bool m_previewLoaded = false;

    /// Draw the semi-transparent preview mesh overlay.
    void drawPreviewMesh(const QMatrix4x4& model,
                         const QMatrix4x4& view,
                         const QMatrix4x4& projection);

    // ── snap indicators & live dimensions (QPainter 2D) ─────────────────
    /// Draw snap dots, H/V alignment guides, and live dimension text during
    /// sketch drawing.  Called after drawSketchConstraintOverlay().
    void drawSketchSnapAndDimensionOverlay();

    // ── smooth camera animation ─────────────────────────────────────────
    QTimer  m_animTimer;
    int     m_animDuration    = 300;
    int     m_animElapsed     = 0;
    bool    m_animating       = false;
    QVector3D m_animStartEye, m_animStartCenter, m_animStartUp;
    QVector3D m_animEndEye,   m_animEndCenter,   m_animEndUp;
    static constexpr int kAnimTickMs = 16;  // ~60 fps

    // ── sketch mode state ─────────────────────────────────────────────────
    bool m_sketchModeActive = false;
    bool m_lockRotation     = false;

    // Saved camera state for restoring after sketch editing
    QVector3D m_savedEye    {0.0f, 0.0f, 5.0f};
    QVector3D m_savedCenter {0.0f, 0.0f, 0.0f};
    QVector3D m_savedUp     {0.0f, 1.0f, 0.0f};
    bool      m_hasSavedCamera = false;

    // ── viewport manipulator (drag handles) ──────────────────────────────
    ViewportManipulator* m_manipulator = nullptr;

    /// Draw the viewport manipulator overlay (value label, flip arrow).
    void drawManipulatorOverlay();
};
