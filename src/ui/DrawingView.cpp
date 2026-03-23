#include "DrawingView.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPrinter>
#if __has_include(<QSvgGenerator>)
#include <QSvgGenerator>
#define HAS_QT_SVG 1
#else
#define HAS_QT_SVG 0
#endif
#include <QDate>
#include <QFileInfo>

#include <HLRBRep_Algo.hxx>
#include <HLRBRep_HLRToShape.hxx>
#include <HLRAlgo_Projector.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <Geom_Curve.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <GCPnts_UniformDeflection.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Ax2.hxx>

#include <cmath>
#include <algorithm>

// ─── Construction / setup ──────────────────────────────────────────────────

DrawingView::DrawingView(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(tr("2D Drawing"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background-color: #e0e0e0;");
}

void DrawingView::setBody(const TopoDS_Shape& shape)
{
    m_shape = shape;
}

QSize DrawingView::sizeHint() const
{
    // Display at roughly 3 pixels per mm for on-screen viewing
    constexpr double screenPxPerMM = 3.0;
    return QSize(static_cast<int>(m_pageWidthMM * screenPxPerMM),
                 static_cast<int>(m_pageHeightMM * screenPxPerMM));
}

// ─── HLR projection ───────────────────────────────────────────────────────

DrawingView::ProjectedView
DrawingView::projectShape(const TopoDS_Shape& shape,
                           const gp_Dir& viewDir,
                           const gp_Dir& upDir,
                           const QString& name)
{
    ProjectedView view;
    view.name = name;

    // Build a right-handed coordinate system for the projection.
    // X-axis = up x viewDir (the horizontal direction on the drawing).
    gp_Dir xDir = upDir.Crossed(viewDir);

    gp_Ax2 projAxes(gp_Pnt(0, 0, 0), viewDir, xDir);
    HLRAlgo_Projector projector(projAxes);

    Handle(HLRBRep_Algo) hlr = new HLRBRep_Algo;
    hlr->Add(shape);
    hlr->Projector(projector);
    hlr->Update();
    hlr->Hide();

    HLRBRep_HLRToShape hlrToShape(hlr);

    // Visible edges: sharp + smooth + outline
    TopoDS_Shape visSharp   = hlrToShape.VCompound();
    TopoDS_Shape visSmooth  = hlrToShape.Rg1LineVCompound();
    TopoDS_Shape visOutline = hlrToShape.OutLineVCompound();

    // Hidden edges (sharp only — keeping it clean)
    TopoDS_Shape hidSharp   = hlrToShape.HCompound();

    extractEdges2D(visSharp,   view.visibleEdges);
    extractEdges2D(visSmooth,  view.visibleEdges);
    extractEdges2D(visOutline, view.visibleEdges);
    extractEdges2D(hidSharp,   view.hiddenEdges);

    // Compute bounding rect over all edges
    double xmin =  1e30, ymin =  1e30;
    double xmax = -1e30, ymax = -1e30;

    auto updateBounds = [&](const std::vector<QLineF>& lines) {
        for (const auto& l : lines) {
            xmin = std::min({xmin, l.x1(), l.x2()});
            ymin = std::min({ymin, l.y1(), l.y2()});
            xmax = std::max({xmax, l.x1(), l.x2()});
            ymax = std::max({ymax, l.y1(), l.y2()});
        }
    };
    updateBounds(view.visibleEdges);
    updateBounds(view.hiddenEdges);

    if (view.visibleEdges.empty() && view.hiddenEdges.empty()) {
        view.boundingRect = QRectF(0, 0, 1, 1);
    } else {
        view.boundingRect = QRectF(xmin, ymin, xmax - xmin, ymax - ymin);
    }

    return view;
}

void DrawingView::extractEdges2D(const TopoDS_Shape& edgeCompound,
                                  std::vector<QLineF>& outLines)
{
    if (edgeCompound.IsNull())
        return;

    constexpr double deflection = 0.1; // chord-height tolerance in model units (mm)

    for (TopExp_Explorer ex(edgeCompound, TopAbs_EDGE); ex.More(); ex.Next()) {
        const TopoDS_Edge& edge = TopoDS::Edge(ex.Current());

        // HLR result edges may store geometry as curves-on-surface (2D PCurves)
        // rather than 3D curves, so BRep_Tool::Curve can return null.
        // BRepAdaptor_Curve handles both representations.
        try {
            BRepAdaptor_Curve adaptor(edge);
            GCPnts_UniformDeflection sampler(adaptor, deflection);
            if (!sampler.IsDone() || sampler.NbPoints() < 2)
                continue;

            for (int i = 1; i < sampler.NbPoints(); ++i) {
                gp_Pnt p1 = sampler.Value(i);
                gp_Pnt p2 = sampler.Value(i + 1);
                outLines.emplace_back(p1.X(), p1.Y(), p2.X(), p2.Y());
            }
        } catch (...) {
            // Some degenerate HLR edges may not be adaptable — skip them.
            continue;
        }
    }
}

// ─── Standard 3-view layout ───────────────────────────────────────────────

void DrawingView::generateStandardViews()
{
    if (m_shape.IsNull())
        return;

    m_views.clear();

    // Front: looking along -Y, up = +Z
    m_views.push_back(projectShape(m_shape,
        gp_Dir(0, -1, 0), gp_Dir(0, 0, 1), tr("FRONT")));

    // Top: looking along -Z, up = +Y
    m_views.push_back(projectShape(m_shape,
        gp_Dir(0, 0, -1), gp_Dir(0, 1, 0), tr("TOP")));

    // Right: looking along +X, up = +Z
    m_views.push_back(projectShape(m_shape,
        gp_Dir(1, 0, 0), gp_Dir(0, 0, 1), tr("RIGHT")));

    // Isometric: looking along (-1,-1,-1) normalised, up ~= +Z
    gp_Dir isoDir(-1, -1, -1);
    gp_Dir isoUp(0, 0, 1);
    m_views.push_back(projectShape(m_shape, isoDir, isoUp, tr("ISOMETRIC")));

    // ── Auto-scale and position ───────────────────────────────────────────
    // Drawable area (inside margins, minus title block at bottom 20 mm)
    const double drawW = m_pageWidthMM  - 2 * m_margin;
    const double drawH = m_pageHeightMM - 2 * m_margin - 20; // 20 mm title block

    // We arrange the four views in a 2x2 grid:
    //   [Top]          [Isometric]
    //   [Front]        [Right]
    const double cellW = drawW / 2.0;
    const double cellH = drawH / 2.0;

    // Find the uniform scale so every view fits its cell (with some padding)
    const double pad = 8.0; // mm padding inside each cell
    double minScale = 1e30;
    for (const auto& v : m_views) {
        double scaleX = (cellW - 2 * pad) / std::max(v.boundingRect.width(), 0.001);
        double scaleY = (cellH - 2 * pad) / std::max(v.boundingRect.height(), 0.001);
        minScale = std::min(minScale, std::min(scaleX, scaleY));
    }
    // Clamp to reasonable range
    if (minScale > 10.0) minScale = 1.0;
    if (minScale < 0.001) minScale = 0.001;

    // Round down to a nice scale factor (1:1, 1:2, 1:5, 2:1, etc.)
    auto niceScale = [](double s) -> double {
        const double niceVals[] = {0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0};
        double best = niceVals[0];
        for (double v : niceVals) {
            if (v <= s) best = v;
        }
        return best;
    };
    double scale = niceScale(minScale);

    // Assign positions: cell centers within the drawable area
    // Origin of drawable area = (margin, margin)
    double ox = m_margin;
    double oy = m_margin;

    // Cell centers
    QPointF cellCenters[4] = {
        {ox + cellW * 0.5, oy + cellH * 0.5},              // Top-left: Top view
        {ox + cellW * 1.5, oy + cellH * 0.5},              // Top-right: Isometric
        {ox + cellW * 0.5, oy + cellH * 1.5},              // Bottom-left: Front view
        {ox + cellW * 1.5, oy + cellH * 1.5},              // Bottom-right: Right view
    };

    // Order: Front=0, Top=1, Right=2, Iso=3 (as generated above)
    // Map: Top -> cell[0], Iso -> cell[1], Front -> cell[2], Right -> cell[3]
    int layoutMap[] = {2, 0, 3, 1}; // view index -> cell index

    for (size_t i = 0; i < m_views.size(); ++i) {
        auto& v = m_views[i];
        v.scale = scale;
        v.position = cellCenters[layoutMap[i]];
    }

    addAutoDimensions();

    update();
}

// ─── Auto-dimensions ──────────────────────────────────────────────────────

int DrawingView::dimensionCount() const
{
    int total = 0;
    for (const auto& v : m_views)
        total += static_cast<int>(v.dimensions.size());
    return total;
}

void DrawingView::addAutoDimensions()
{
    for (auto& view : m_views) {
        view.dimensions.clear();

        if (view.visibleEdges.empty() && view.hiddenEdges.empty())
            continue;

        // Skip isometric view — dimensions on iso projections are misleading
        if (view.name.contains("ISO", Qt::CaseInsensitive))
            continue;

        const QRectF& bbox = view.boundingRect;
        double w = bbox.width();
        double h = bbox.height();

        if (w < 0.01 || h < 0.01)
            continue;

        const double offsetMM = 8.0; // offset on paper in mm

        // Horizontal dimension (bottom of view) — overall width
        {
            DimensionLine dim;
            dim.start = QPointF(bbox.left(), bbox.bottom());
            dim.end   = QPointF(bbox.right(), bbox.bottom());
            dim.value = w;
            dim.isHorizontal = true;
            dim.offset = offsetMM / view.scale;
            view.dimensions.push_back(dim);
        }

        // Vertical dimension (right of view) — overall height
        {
            DimensionLine dim;
            dim.start = QPointF(bbox.right(), bbox.bottom());
            dim.end   = QPointF(bbox.right(), bbox.top());
            dim.value = h;
            dim.isHorizontal = false;
            dim.offset = offsetMM / view.scale;
            view.dimensions.push_back(dim);
        }
    }
}

void DrawingView::drawDimensionLine(QPainter& painter,
                                     const DimensionLine& dim,
                                     const QRectF& viewBBox,
                                     double scale)
{
    // Center of the bounding rect in model coords
    double cx = viewBBox.center().x();
    double cy = viewBBox.center().y();

    // Map dimension endpoints to view-local screen coords (Y flipped)
    QPointF p1((dim.start.x() - cx) * scale, -(dim.start.y() - cy) * scale);
    QPointF p2((dim.end.x()   - cx) * scale, -(dim.end.y()   - cy) * scale);

    // Offset perpendicular to the dimension line, away from geometry
    QPointF offset;
    if (dim.isHorizontal)
        offset = QPointF(0, dim.offset * scale);   // push down (positive Y on screen)
    else
        offset = QPointF(dim.offset * scale, 0);    // push right

    QPointF op1 = p1 + offset;
    QPointF op2 = p2 + offset;

    // Dimension line style: thin blue
    QPen dimPen(QColor(0, 100, 200), 0.15);
    dimPen.setCapStyle(Qt::RoundCap);
    painter.setPen(dimPen);

    // Extension lines (from geometry edge to dimension line, with small gap)
    double gap = 1.0; // 1mm gap from geometry
    if (dim.isHorizontal) {
        painter.drawLine(QPointF(p1.x(), p1.y() + gap), op1);
        painter.drawLine(QPointF(p2.x(), p2.y() + gap), op2);
    } else {
        painter.drawLine(QPointF(p1.x() + gap, p1.y()), op1);
        painter.drawLine(QPointF(p2.x() + gap, p2.y()), op2);
    }

    // Dimension line between the offset endpoints
    painter.drawLine(op1, op2);

    // Arrows at endpoints
    double arrowLen = 1.5;  // mm on paper
    QPointF dir = op2 - op1;
    double len = std::sqrt(dir.x() * dir.x() + dir.y() * dir.y());
    if (len > 0.01) {
        dir /= len;
        QPointF perp(-dir.y(), dir.x());

        // Arrow at op1 (pointing toward op2)
        painter.drawLine(op1, op1 + dir * arrowLen + perp * arrowLen * 0.3);
        painter.drawLine(op1, op1 + dir * arrowLen - perp * arrowLen * 0.3);

        // Arrow at op2 (pointing toward op1)
        painter.drawLine(op2, op2 - dir * arrowLen + perp * arrowLen * 0.3);
        painter.drawLine(op2, op2 - dir * arrowLen - perp * arrowLen * 0.3);
    }

    // Dimension text
    QFont font("Helvetica", 2);
    font.setStyleHint(QFont::SansSerif);
    painter.setFont(font);
    painter.setPen(Qt::black);

    QPointF textPos = (op1 + op2) * 0.5;
    QString text = QString::number(dim.value, 'f', 1);

    painter.save();
    painter.translate(textPos);
    if (!dim.isHorizontal)
        painter.rotate(-90);
    // Draw text centered above the dimension line
    painter.drawText(QRectF(-15, -3.5, 30, 3), Qt::AlignCenter, text);
    painter.restore();
}

// ─── Rendering ─────────────────────────────────────────────────────────────

void DrawingView::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Compute DPI scale: map mm to widget pixels
    double dpiScale = std::min(
        static_cast<double>(width())  / m_pageWidthMM,
        static_cast<double>(height()) / m_pageHeightMM);

    // Center the sheet in the widget
    double sheetPxW = m_pageWidthMM  * dpiScale;
    double sheetPxH = m_pageHeightMM * dpiScale;
    double offX = (width()  - sheetPxW) / 2.0;
    double offY = (height() - sheetPxH) / 2.0;

    painter.save();
    painter.translate(offX, offY);
    painter.scale(dpiScale, dpiScale);

    // White paper
    painter.fillRect(QRectF(0, 0, m_pageWidthMM, m_pageHeightMM), Qt::white);

    renderSheet(painter, dpiScale);

    painter.restore();
}

void DrawingView::renderSheet(QPainter& painter, double dpiScale)
{
    drawBorder(painter, dpiScale);

    for (const auto& v : m_views) {
        drawProjectedView(painter, v, dpiScale);
    }

    drawTitleBlock(painter, dpiScale);
}

void DrawingView::drawBorder(QPainter& painter, double /*dpiScale*/)
{
    QPen borderPen(Qt::black, 0.5);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(m_margin, m_margin,
                            m_pageWidthMM - 2 * m_margin,
                            m_pageHeightMM - 2 * m_margin));
}

void DrawingView::drawTitleBlock(QPainter& painter, double /*dpiScale*/)
{
    // Title block: a box at the bottom-right of the sheet
    const double tbW = 120;
    const double tbH = 20;
    const double tbX = m_pageWidthMM - m_margin - tbW;
    const double tbY = m_pageHeightMM - m_margin - tbH;

    QPen pen(Qt::black, 0.35);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(tbX, tbY, tbW, tbH));

    // Vertical dividers
    painter.drawLine(QPointF(tbX + 40, tbY), QPointF(tbX + 40, tbY + tbH));
    painter.drawLine(QPointF(tbX + 80, tbY), QPointF(tbX + 80, tbY + tbH));

    // Horizontal divider
    painter.drawLine(QPointF(tbX, tbY + tbH / 2), QPointF(tbX + tbW, tbY + tbH / 2));

    QFont labelFont("Helvetica", 2);
    QFont valueFont("Helvetica", 3);
    labelFont.setStyleHint(QFont::SansSerif);
    valueFont.setStyleHint(QFont::SansSerif);

    auto drawCell = [&](double cx, double cy, double cw, double ch,
                        const QString& label, const QString& value) {
        painter.setFont(labelFont);
        painter.setPen(QColor(100, 100, 100));
        painter.drawText(QRectF(cx + 1, cy + 0.5, cw - 2, ch * 0.4),
                         Qt::AlignLeft | Qt::AlignTop, label);
        painter.setFont(valueFont);
        painter.setPen(Qt::black);
        painter.drawText(QRectF(cx + 1, cy + ch * 0.35, cw - 2, ch * 0.65),
                         Qt::AlignLeft | Qt::AlignVCenter, value);
    };

    double halfH = tbH / 2.0;
    drawCell(tbX,      tbY,         40, halfH, tr("Project"),  tr("kernelCAD"));
    drawCell(tbX + 40, tbY,         40, halfH, tr("Title"),    tr("Part Drawing"));
    drawCell(tbX + 80, tbY,         40, halfH, tr("Date"),     QDate::currentDate().toString("yyyy-MM-dd"));

    // Determine the scale string from the first view
    QString scaleStr = "1:1";
    if (!m_views.empty()) {
        double s = m_views[0].scale;
        if (s >= 1.0)
            scaleStr = QString("%1:1").arg(static_cast<int>(s));
        else
            scaleStr = QString("1:%1").arg(static_cast<int>(std::round(1.0 / s)));
    }

    drawCell(tbX,      tbY + halfH, 40, halfH, tr("Scale"),   scaleStr);
    drawCell(tbX + 40, tbY + halfH, 40, halfH, tr("Sheet"),   tr("1 / 1"));
    drawCell(tbX + 80, tbY + halfH, 40, halfH, tr("Units"),   tr("mm"));
}

void DrawingView::drawProjectedView(QPainter& painter,
                                     const ProjectedView& view,
                                     double /*dpiScale*/)
{
    painter.save();

    // Translate to view position (centre of its cell)
    painter.translate(view.position.x(), view.position.y());

    // The edges are in model coordinates — we need to scale and center them.
    // Center of the bounding rect in model coords:
    double cx = view.boundingRect.center().x();
    double cy = view.boundingRect.center().y();

    // Visible edges: solid black 0.35mm
    {
        QPen pen(Qt::black, 0.35);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);

        for (const auto& line : view.visibleEdges) {
            QLineF mapped(
                (line.x1() - cx) * view.scale,
                -(line.y1() - cy) * view.scale,  // flip Y for screen coords
                (line.x2() - cx) * view.scale,
                -(line.y2() - cy) * view.scale
            );
            painter.drawLine(mapped);
        }
    }

    // Hidden edges: dashed gray 0.18mm
    {
        QPen pen(QColor(120, 120, 120), 0.18);
        pen.setStyle(Qt::DashLine);
        QVector<qreal> dashes;
        dashes << 2.0 << 1.5;
        pen.setDashPattern(dashes);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);

        for (const auto& line : view.hiddenEdges) {
            QLineF mapped(
                (line.x1() - cx) * view.scale,
                -(line.y1() - cy) * view.scale,
                (line.x2() - cx) * view.scale,
                -(line.y2() - cy) * view.scale
            );
            painter.drawLine(mapped);
        }
    }

    // View label underneath
    {
        QFont font("Helvetica", 3);
        font.setStyleHint(QFont::SansSerif);
        painter.setFont(font);
        painter.setPen(Qt::black);

        double labelY = view.boundingRect.height() * view.scale / 2.0 + 4;
        painter.drawText(QRectF(-30, labelY, 60, 5),
                         Qt::AlignHCenter | Qt::AlignTop,
                         view.name);
    }

    // Dimension annotations
    for (const auto& dim : view.dimensions) {
        drawDimensionLine(painter, dim, view.boundingRect, view.scale);
    }

    painter.restore();
}

// ─── Export ────────────────────────────────────────────────────────────────

void DrawingView::exportPDF(const QString& path)
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageSize(QPageSize(QSizeF(m_pageWidthMM, m_pageHeightMM),
                                  QPageSize::Millimeter));
    printer.setPageMargins(QMarginsF(0, 0, 0, 0));

    QPainter painter(&printer);
    if (!painter.isActive())
        return;

    // In printer coordinates, 1 unit = 1 dot at printer DPI.
    // We want to paint in mm, so scale: pxPerMM = DPI / 25.4
    double pxPerMM = printer.resolution() / 25.4;
    painter.scale(pxPerMM, pxPerMM);

    // White background
    painter.fillRect(QRectF(0, 0, m_pageWidthMM, m_pageHeightMM), Qt::white);

    renderSheet(painter, pxPerMM);
    painter.end();
}

void DrawingView::exportSVG(const QString& path)
{
#if HAS_QT_SVG
    QSvgGenerator svg;
    svg.setFileName(path);
    svg.setSize(QSize(static_cast<int>(m_pageWidthMM), static_cast<int>(m_pageHeightMM)));
    svg.setViewBox(QRectF(0, 0, m_pageWidthMM, m_pageHeightMM));
    svg.setTitle(tr("kernelCAD Drawing"));
    svg.setDescription(tr("2D engineering drawing exported from kernelCAD"));

    QPainter painter(&svg);
    if (!painter.isActive())
        return;

    painter.fillRect(QRectF(0, 0, m_pageWidthMM, m_pageHeightMM), Qt::white);
    renderSheet(painter, 1.0);
    painter.end();
#else
    Q_UNUSED(path);
#endif
}
