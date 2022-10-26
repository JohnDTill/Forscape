#include "typeset_painter.h"

#include <typeset_themes.h>
#include <typeset_view.h>
#include <hope_unicode.h>

#ifndef NDEBUG
#include <iostream>
#endif

#include <array>
#include <QDialog>
#include <QLabel>
#include <QPainterPath>

//EVENTUALLY: probably want to codegen these
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

static QFont readFont(const QString& file, const QString& name, const QString& family){
    int id = QFontDatabase::addApplicationFont(file);
    #ifdef NDEBUG
    (void)id;
    #endif
    assert(id!=-1);
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QFont font = QFontDatabase::font(name, family, depthToFontSize(0));
    #else
    QFont font = QFontDatabase().font(name, family, depthToFontSize(0));
    #endif
    font.setKerning(false);

    return font;
}

void Painter::init(){
    is_init = true;

    std::array<QFont, NUM_FONTS> loaded_fonts;

    //EVENTUALLY: codegen font loading
    loaded_fonts[JULIAMONO_REGULAR] = readFont(":/fonts/JuliaMono-Regular.ttf", "JuliaMono", "Regular");
    loaded_fonts[CMU_SERIF_MONO_ITALIC] = readFont(":/fonts/CMUSerifMono-Italic.ttf", "CMU Serif Mono", "Italic");
    loaded_fonts[TRIMMEDJULIAMONO_BOLD] = readFont(":/fonts/TrimmedJuliaMono-Bold.ttf", "TrimmedJuliaMono", "Bold");
    loaded_fonts[TRIMMEDJULIAMONO_ITALIC] = readFont(":/fonts/TrimmedJuliaMono-RegularItalic.ttf", "TrimmedJuliaMono", "Italic");
    loaded_fonts[TRIMMEDJULIAMONO_BOLD_ITALIC] = readFont(":/fonts/TrimmedJuliaMono-BoldItalic.ttf", "TrimmedJuliaMono", "Bold Italic");
    loaded_fonts[CMU_TYPEWRITER_SCALED_REGULAR] = readFont(":/fonts/CMUTypewriterScaled-Regular.ttf", "CMU Typewriter Scaled", "Regular");
    loaded_fonts[CMU_TYPEWRITER_SCALED_BOLD_BOLD] = readFont(":/fonts/CMUTypewriterScaled-Bold.ttf", "CMU Typewriter Scaled Bold", "Bold");

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

Painter::Painter(WrappedPainter& painter, double xL, double yT, double xR, double yB)
    : painter(painter), xL(xL), yT(yT), xR(xR), yB(yB) {
    const double w = xR-xL;
    for(size_t i = 0; i < 3; i++)
        max_graphemes[i] = 1 + static_cast<size_t>(w / CHARACTER_WIDTHS[i]);
}

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

void Painter::drawText(double x, double y, std::string_view text){
    if(x >= xR || y >= yB || (y += CAPHEIGHT[depth]) <= yT) return;

    const size_t grapheme_start = x > xL ? 0 : (xL - x) / CHARACTER_WIDTHS[depth];
    const size_t char_start = charIndexOfGrapheme(text, grapheme_start);

    x += x_offset + grapheme_start * CHARACTER_WIDTHS[depth];

    const size_t char_end = charIndexOfGrapheme(text, max_graphemes[depth], char_start);

    QString q_str = QString::fromUtf8(text.data() + char_start, static_cast<int>(char_end - char_start));

    painter.drawText(QPointF(x, y), q_str);
}

void Painter::drawHighlightedGrouping(double x, double y, double w, std::string_view text){
    x += x_offset;

    if(x >= xR || y >= yB || (y + CAPHEIGHT[depth]) <= yT || x + CHARACTER_WIDTHS[depth] < xL) return;

    painter.setPen(getColour(COLOUR_GROUPINGBACKGROUND));
    painter.setBrush(getColour(COLOUR_GROUPINGBACKGROUND));
    painter.drawRect(x, y, w, ASCENT[depth]);

    painter.setPen(getColour(COLOUR_GROUPINGHIGHLIGHT));
    y += CAPHEIGHT[depth];
    painter.drawText(QPointF(x, y), QString::fromUtf8(text.data(), static_cast<int>(text.size())));

    painter.setPen(getColour(COLOUR_CURSOR));
    painter.setBrush(getColour(COLOUR_CURSOR));
}

void Painter::drawSymbol(double x, double y, std::string_view text){
    x += x_offset;
    if(x >= xR ||
       y >= yB ||
       (y += BIG_SYM_SCALE*CAPHEIGHT[depth]) <= yT ||
       x + BIG_SYM_SCALE*CHARACTER_WIDTHS[depth] < xL) return;
    QFont font = getFont(SEM_BIG_SYM, depth);
    font.setPointSizeF(font.pointSizeF()*BIG_SYM_SCALE);
    painter.setFont(font);
    painter.drawText(x, y, QString::fromUtf8(text.data(), static_cast<int>(text.size())));
}

void Painter::drawLine(double x, double y, double w, double h){
    QPen pen = painter.pen();
    static constexpr double THICKNESS = 1.0;
    pen.setWidthF(THICKNESS);
    painter.setPen(pen);

    x += x_offset;
    painter.drawLine(x, y-THICKNESS/2, x+w, y+h-THICKNESS/2);
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
    painter.drawRect(QRectF(x, y, w, h));
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
    y += CAPHEIGHT[0];
    QString str = QString::number(num);

    qreal x = x_offset;
    if(num < 1000000){
        painter.setFont(getFont(SEM_DEFAULT, 0));
        x -= CHARACTER_WIDTHS[0]*str.size();
    }else if(num < 100000000){
        painter.setFont(getFont(SEM_DEFAULT, 1));
        x -= CHARACTER_WIDTHS[1]*str.size();
    }else{
        painter.setFont(getFont(SEM_DEFAULT, 2));
        x -= CHARACTER_WIDTHS[2]*str.size();
    }

    painter.drawText(x, y, str);
}

void Painter::setSelectionMode(){
    color_can_change = false;
}

void Painter::exitSelectionMode(){
    color_can_change = true;
}

void Painter::drawSymbol(char ch, double x, double y, double w, double h){
    x += x_offset;

    double scale_x = w / CHARACTER_WIDTHS[depth];
    double scale_y = h / ASCENT[depth];
    x /= scale_x;
    y = (y + h - DESCENT[depth]) / scale_y;

    QTransform transform = painter.transform();
    painter.scale(scale_x, scale_y);
    painter.drawText(QPointF(x, y), QChar(ch));
    painter.setTransform(transform);
}

void Painter::drawSymbol(std::string_view str, double x, double y, double w, double h){
    x += x_offset;

    QString qstr = QString::fromUtf8(str.data(), static_cast<int>(str.size()));
    double scale_x = w / CHARACTER_WIDTHS[depth]*qstr.size();
    double scale_y = h / ASCENT[depth];
    x /= scale_x;
    y = (y + h - DESCENT[depth]) / scale_y;

    QTransform transform = painter.transform();
    painter.scale(scale_x, scale_y);
    painter.drawText(QPointF(x, y), qstr);
    painter.setTransform(transform);
}

void Painter::drawComma(double x, double y, bool selected) {
    painter.setPen(selected ? getColour(COLOUR_SELECTEDTEXT) : getColour(COLOUR_KEYWORD));

    x += x_offset - CHARACTER_WIDTHS[depth]*0.32;
    y += CAPHEIGHT[depth];
    const QFont font = painter.font();
    QFont new_font = font;
    new_font.setPointSizeF(font.pointSizeF()*0.6);
    new_font.setBold(true);
    painter.setFont(new_font);
    painter.drawText(x, y, ",");
    painter.setFont(font);
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
