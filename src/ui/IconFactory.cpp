#include "IconFactory.h"
#include "IconDrawFunctions.h"
#include <QPainter>
#include <QHash>

// ─── helpers (for fallback icon only) ───────────────────────────────────────

namespace {

QPixmap makePixmap(int size) {
    QPixmap px(size, size);
    px.fill(Qt::transparent);
    return px;
}

void initPainter(QPainter& p, const QColor& penColor, qreal penWidth = 1.5) {
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(penColor, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
}

} // anonymous namespace

// ─── factory dispatch ───────────────────────────────────────────────────────

QIcon IconFactory::createIcon(const QString& name, int size) {
    static const QHash<QString, QPixmap(*)(int)> map = {
        {"extrude",       &IconDraw::extrude},
        {"revolve",       &IconDraw::revolve},
        {"fillet",        &IconDraw::fillet},
        {"chamfer",       &IconDraw::chamfer},
        {"hole",          &IconDraw::hole},
        {"shell",         &IconDraw::shell},
        {"draft",         &IconDraw::draft},
        {"sketch",        &IconDraw::sketch},
        {"line",          &IconDraw::line},
        {"rectangle",     &IconDraw::rectangle},
        {"circle",        &IconDraw::circle},
        {"arc",           &IconDraw::arc},
        {"ellipse",       &IconDraw::ellipse},
        {"polygon",       &IconDraw::polygon},
        {"slot",          &IconDraw::slot},
        {"box",           &IconDraw::box},
        {"cylinder",      &IconDraw::cylinder},
        {"sphere",        &IconDraw::sphere},
        {"mirror",        &IconDraw::mirror},
        {"rect_pattern",  &IconDraw::rectPattern},
        {"circ_pattern",  &IconDraw::circPattern},
        {"measure",       &IconDraw::measure},
        {"undo",          &IconDraw::undo},
        {"redo",          &IconDraw::redo},
        {"save",          &IconDraw::save},
        {"open",          &IconDraw::open},
        {"new",           &IconDraw::newIcon},
        {"sweep",         &IconDraw::sweep},
        {"loft",          &IconDraw::loft},
        {"joint",         &IconDraw::joint},
        {"component",     &IconDraw::component},
        {"insert",        &IconDraw::insert},
        {"trim",          &IconDraw::trim},
        {"extend",        &IconDraw::extend},
        {"offset",        &IconDraw::offset},
        {"project",       &IconDraw::project},
        {"construction",  &IconDraw::construction},
        {"select",        &IconDraw::select},
        {"finish",        &IconDraw::finish},
        {"delete",        &IconDraw::deleteIcon},
        {"interference",  &IconDraw::interference},
        {"plane",         &IconDraw::plane},
        {"axis",          &IconDraw::axis},
        {"point",         &IconDraw::point},
        {"coincident",    &IconDraw::coincident},
        {"parallel_c",    &IconDraw::parallel},
        {"perpendicular", &IconDraw::perpendicular},
        {"tangent_c",     &IconDraw::tangentIcon},
        {"equal_c",       &IconDraw::equalIcon},
        {"symmetric_c",   &IconDraw::symmetric},
        {"fix",           &IconDraw::fix},
        {"dimension",     &IconDraw::dimension},
        {"spline",        &IconDraw::spline},
        {"center_rectangle", &IconDraw::centerRectangle},
        {"circle_3point", &IconDraw::circle3Point},
        {"arc_3point",    &IconDraw::arc3Point},
        {"fillet_sketch", &IconDraw::filletSketch},
        {"chamfer_sketch",&IconDraw::chamferSketch},
        {"constraint",    &IconDraw::constraintIcon},
        {"import_dxf",    &IconDraw::importDxf},
        {"import_svg",    &IconDraw::importSvg},
    };

    auto it = map.find(name.toLower());
    if (it != map.end())
        return QIcon((*it)(size));

    // Fallback: rounded-rect with subtle fill and "?" letter
    QPixmap px = makePixmap(size);
    QPainter p(&px);
    initPainter(p, QColor(160, 160, 160, 150), 1.0);
    QColor fillColor(160, 160, 160, 100);
    p.setBrush(fillColor);
    int margin = 2;
    p.drawRoundedRect(QRectF(margin, margin, size - 2*margin, size - 2*margin), 4, 4);
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setPixelSize(size / 2);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, "?");
    return QIcon(px);
}
