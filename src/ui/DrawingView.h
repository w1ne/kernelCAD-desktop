#pragma once
#include <QWidget>
#include <QLineF>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <vector>
#include <TopoDS_Shape.hxx>

/// A 2D drawing page that generates dimensioned orthographic views from a 3D body
/// using OCCT's HLR (Hidden Line Removal) algorithm. Supports standard 3-view layout
/// (Front, Top, Right) plus an optional isometric view, with PDF and SVG export.
class DrawingView : public QWidget
{
    Q_OBJECT
public:
    explicit DrawingView(QWidget* parent = nullptr);

    /// Set the body to create views from.
    void setBody(const TopoDS_Shape& shape);

    /// Standard 3-view layout: Front, Top, Right (plus optional isometric).
    void generateStandardViews();

    /// Export the drawing sheet to PDF.
    void exportPDF(const QString& path);

    /// Export the drawing sheet to SVG.
    void exportSVG(const QString& path);

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override;

private:
    struct ProjectedView {
        QString name;                         // "Front", "Top", "Right", "Isometric"
        QPointF position;                     // position on the drawing sheet (mm)
        double  scale = 1.0;
        std::vector<QLineF> visibleEdges;     // visible edge lines
        std::vector<QLineF> hiddenEdges;      // hidden (dashed) edge lines
        QRectF  boundingRect;                 // bounding rect of 2D projected edges
    };

    std::vector<ProjectedView> m_views;
    TopoDS_Shape m_shape;

    // Page settings (A4 landscape by default)
    double m_pageWidthMM  = 297;
    double m_pageHeightMM = 210;
    double m_margin       = 15;   // mm margin

    /// Project a 3D shape onto a 2D plane, extracting visible and hidden edges.
    ProjectedView projectShape(const TopoDS_Shape& shape,
                               const gp_Dir& viewDirection,
                               const gp_Dir& upDirection,
                               const QString& name);

    /// Extract 2D line segments from an OCCT edge compound into a QLineF vector.
    void extractEdges2D(const TopoDS_Shape& edgeCompound,
                        std::vector<QLineF>& outLines);

    /// Render the full drawing sheet onto the given painter (shared by paintEvent and export).
    void renderSheet(QPainter& painter, double dpiScale);

    /// Draw a title block at the bottom of the sheet.
    void drawTitleBlock(QPainter& painter, double dpiScale);

    /// Draw a border frame around the drawing area.
    void drawBorder(QPainter& painter, double dpiScale);

    /// Draw a single projected view (edges, label).
    void drawProjectedView(QPainter& painter, const ProjectedView& view, double dpiScale);
};
