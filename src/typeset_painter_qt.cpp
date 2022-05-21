#include "typeset_painter.h"

#include <typeset_themes.h>
#include <typeset_view.h>

#ifndef NDEBUG
#include <iostream>
#endif

#include <array>
#include <QDialog>
#include <QLabel>
#include <QPainterPath>

//DO THIS - codegen
static constexpr double ASCENT[3] = {
    12.34375,
    8.546875,
    6.65625,
};

static constexpr double CAPHEIGHT[3] = {
    9.421875,
    6.515625,
    5.0625,
};

namespace Hope {

namespace Typeset {

uint8_t depthToFontSize(uint8_t depth) noexcept{
    switch (depth) {
        case 0: return 10;
        case 1: return 7;
        case 2: return 5;
        default:
            assert(false);
            return 0;
    }
}

bool is_init = false;

static constexpr size_t N_DEPTHS = 3;
QFont fonts[N_DEPTHS*NUM_SEM_TYPES];
static QFont line_font;

static QFont readFont(const QString& file, const QString& name, const QString& family){
    int id = QFontDatabase::addApplicationFont(file);
    #ifdef NDEBUG
    (void)id;
    #endif
    assert(id!=-1);
    QFont font = QFontDatabase().font(name, family, depthToFontSize(0));

    return font;
}

void Painter::init(){
    is_init = true;

    std::array<QFont, NUM_FONTS> loaded_fonts;

    //DO THIS: codegen font loading
    loaded_fonts[JULIAMONO_REGULAR] = readFont(":/fonts/JuliaMono-Regular.ttf", "JuliaMono", "Regular");
    loaded_fonts[JULIAMONO_ITALIC] = readFont(":/fonts/JuliaMono-RegularItalic.ttf", "JuliaMono", "Italic");
    loaded_fonts[JULIAMONO_BOLD] = readFont(":/fonts/JuliaMono-Bold.ttf", "JuliaMono", "Bold");
    loaded_fonts[JULIAMONO_BOLD_ITALIC] = readFont(":/fonts/JuliaMono-BoldItalic.ttf", "JuliaMono", "Bold Italic");

    line_font = loaded_fonts[JULIAMONO_REGULAR];
    line_font.setPointSize(8);

    for(size_t i = 0; i < NUM_SEM_TYPES; i++){
        QFont font = loaded_fonts[font_enum[i]];
        fonts[N_DEPTHS*i] = font;

        for(uint8_t j = 1; j < N_DEPTHS; j++){
            font.setPointSize(depthToFontSize(j));
            fonts[N_DEPTHS*i + j] = font;
        }
    }
}

const QFont& getFont(SemanticType type, uint8_t depth) noexcept {
    assert(is_init);
    return fonts[N_DEPTHS*static_cast<size_t>(type) + depth];
}

static QColor getColor(SemanticType type) noexcept {
    return Typeset::getColour(sem_colours[type]);
}

Painter::Painter(WrappedPainter& painter)
    : painter(painter) {}

void Painter::setZoom(double zoom){
    painter.scale(zoom, zoom);
}

void Painter::setType(SemanticType type) {
    this->type = type;
    painter.setFont(getFont(type, depth));
    if(color_can_change) painter.setPen(getColor(type));
}

void Painter::setTypeIfAppropriate(SemanticType type){
    if(this->type != SEM_COMMENT && this->type != SEM_STRING) setType(type);
}

void Painter::setScriptLevel(uint8_t depth){
    this->depth = depth;
    painter.setFont(getFont(type, depth));
}

void Painter::setOffset(double x, double y){
    painter.translate(0, y);
    x_offset = x;
}

void Painter::drawText(double x, double y, std::string_view text, bool forward){
    x += x_offset;
    QString q_str = QString::fromUtf8(text.data(), text.size());

    if(forward){
        y += CAPHEIGHT[depth];
        painter.drawText(x, y, q_str);
    }else{
        y += CAPHEIGHT[depth];
        y -= ASCENT[depth];
        double h = CHARACTER_HEIGHTS[depth];
        double w = CHARACTER_WIDTHS[depth]*q_str.size();
        painter.drawText(QRectF(x, y, w, h), Qt::AlignRight|Qt::AlignBaseline, q_str);
    }
}

void Painter::drawHighlightedGrouping(double x, double y, double w, std::string_view text){
    x += x_offset;

    painter.setPen(getColour(COLOUR_GROUPINGBACKGROUND));
    painter.setBrush(getColour(COLOUR_GROUPINGBACKGROUND));
    painter.drawRect(x, y, w, ASCENT[depth]);

    painter.setPen(getColour(COLOUR_GROUPINGHIGHLIGHT));
    y += CAPHEIGHT[depth];
    painter.drawText(x, y, QString::fromUtf8(text.data(), text.size()));

    painter.setPen(getColour(COLOUR_CURSOR));
    painter.setBrush(getColour(COLOUR_CURSOR));
}

void Painter::drawSymbol(double x, double y, std::string_view text){
    x += x_offset;
    y += CAPHEIGHT[depth];
    painter.drawText(x, y, QString::fromUtf8(text.data(), text.size()));
}

void Painter::drawLine(double x, double y, double w, double h){
    QPen pen = painter.pen();
    pen.setWidthF(0.5);
    painter.setPen(pen);

    x += x_offset;
    painter.drawLine(x, y, x+w, y+h);
}

void Painter::drawPath(const std::vector<std::pair<double, double> >& points){
    QPen pen = painter.pen();
    pen.setWidthF(0.5);
    painter.setPen(pen);

    painter.setBrush(QColor(0,0,0,0));

    QPainterPath path;
    path.moveTo(points.front().first + x_offset, points.front().second);
    for(size_t i = 1; i < points.size(); i++){
        const auto& pt = points[i];
        path.lineTo(pt.first + x_offset, pt.second);
    }

    painter.drawPath(path);
}

void Painter::drawRect(double x, double y, double w, double h){
    x += x_offset;
    painter.drawRect(x, y, w, h);
}

void Painter::drawSelection(double x, double y, double w, double h){
    painter.setPen(getColour(COLOUR_SELECTION));
    painter.setBrush(getColour(COLOUR_SELECTION));

    drawRect(x, y, w, h);

    painter.setPen(getColour(COLOUR_SELECTEDTEXT));
    painter.setBrush(getColour(COLOUR_SELECTEDTEXT));
}

void Painter::drawError(double x, double y, double w, double h){
    painter.setPen(getColour(COLOUR_ERRORBORDER));
    painter.setBrush(getColour(COLOUR_ERRORBACKGROUND));

    x += x_offset;
    painter.drawRoundedRect(x, y, w, h, 4, 4);

    painter.setPen(getColour(COLOUR_CURSOR));
    painter.setBrush(getColour(COLOUR_CURSOR));
}

void Painter::drawEmptySubphrase(double x, double y, double w, double h){
    x += x_offset;

    const QPen old_pen = painter.pen();
    const QBrush old_brush = painter.brush();
    QPen pen = old_pen;
    pen.setStyle(Qt::DotLine);
    pen.setWidthF(0.4);
    painter.setBrush(QColor(0,0,0,0));
    painter.setPen(pen);

    painter.drawRect(x, y, w, h);

    painter.setPen(old_pen);
    painter.setBrush(old_brush);
}

void Painter::drawHighlight(double x, double y, double w, double h){
    x += x_offset;

    /*
    QRadialGradient grad;
    grad.setColorAt(0.0, Qt::black);
    grad.setColorAt(0.5, Qt::red);
    grad.setColorAt(1.0, Qt::black);
    grad.setCenter(x+w/2, y+h/2);
    grad.setRadius(std::max(w/2, h/2));
    grad.setFocalPoint(x+w/2, y+h/2);
    grad.setFocalRadius(grad.radius());
    grad.setCenterRadius(grad.radius());
    */

    QLinearGradient grad;
    grad.setStart(x, y);
    grad.setFinalStop(x, y+h);
    const QColor& primary = getColour(COLOUR_HIGHLIGHTPRIMARY);
    grad.setColorAt(0.0, primary);
    grad.setColorAt(0.6, getColour(COLOUR_HIGHLIGHTSECONDARY));
    grad.setColorAt(1.0, primary);

    QPen pen(getColour(COLOUR_HIGHLIGHTBORDER));
    pen.setWidthF(0.2);
    painter.setPen(pen);
    QBrush brush(grad);
    painter.setBrush(brush);

    painter.drawRoundedRect(x, y, w+1, h, 3, 3);
    //painter.drawRect(x, y, w, h);
    //painter.fillRect(x, y, w, h, grad);

    painter.setPen(getColour(COLOUR_CURSOR));
    painter.setBrush(getColour(COLOUR_CURSOR));
}

void Painter::drawNarrowCursor(double x, double y, double h){
    x += x_offset;
    painter.setPen(getColour(COLOUR_CURSOR));
    painter.drawLine(x, y, x, y+h);
}

void Painter::drawElipse(double x, double y, double w, double h){
    drawSymbol('{', x, y, w, h);
}

void Painter::drawLeftParen(double x, double y, double w, double h){
    drawSymbol('(', x, y, w, h);
}

void Painter::drawRightParen(double x, double y, double w, double h){
    drawSymbol(')', x, y, w, h);
}

void Painter::drawDot(double x, double y){
    x += x_offset;
    painter.drawText(x, y, QChar('.'));
}

void Painter::drawLineNumber(double y, size_t num, bool active){
    if(active) painter.setPen(getColour(COLOUR_LINENUMACTIVE));
    else painter.setPen(getColour(COLOUR_LINENUMPASSIVE));

    painter.setFont(line_font);
    QString str = QString::number(num);
    qreal x = x_offset - CHARACTER_WIDTHS[depth]*str.size();
    y += DESCENT[0] / 2;
    painter.drawText(x, y, str);
}

void Painter::setSelectionMode(){
    color_can_change = false;
}

void Painter::drawSymbol(char ch, double x, double y, double w, double h){
    x += x_offset;

    double scale_x = w / CHARACTER_WIDTHS[depth];
    double scale_y = h / ASCENT[depth];
    x /= scale_x;
    y = (y + h - DESCENT[depth]) / scale_y;

    QTransform transform = painter.transform();
    painter.scale(scale_x, scale_y);
    painter.drawText(x, y, QChar(ch));
    painter.setTransform(transform);
}

void Painter::drawSymbol(std::string_view str, double x, double y, double w, double h){
    x += x_offset;

    QString qstr = QString::fromUtf8(str.data(), str.size());
    double scale_x = w / CHARACTER_WIDTHS[depth]*qstr.size();
    double scale_y = h / ASCENT[depth];
    x /= scale_x;
    y = (y + h - DESCENT[depth]) / scale_y;

    QTransform transform = painter.transform();
    painter.scale(scale_x, scale_y);
    painter.drawText(x, y, qstr);
    painter.setTransform(transform);
}

#ifdef HOPE_TYPESET_LAYOUT_DEBUG
void Painter::drawDebugPhrase(double x, double y, double w, double u, double v){
    x += x_offset;

    QPen pen = painter.pen();
    QBrush brush = painter.brush();

    QPen debug_pen(Qt::yellow);
    debug_pen.setWidthF(0.2);
    painter.setPen(debug_pen);
    painter.setBrush(Qt::gray);
    painter.drawRect(x, y, w, u);
    painter.setBrush(Qt::lightGray);
    painter.drawRect(x, y+u, w, v);

    painter.setPen(pen);
    painter.setBrush(brush);
}

void Painter::drawHorizontalConstructionLine(double y){
    QPen pen = painter.pen();
    QBrush brush = painter.brush();

    QPen debug_pen(Qt::blue);
    debug_pen.setWidthF(0.2);
    debug_pen.setStyle(Qt::PenStyle::DashLine);
    painter.setPen(debug_pen);
    painter.drawLine(0, y, 10000, y);

    painter.setPen(pen);
    painter.setBrush(brush);
}
#endif

}

}
