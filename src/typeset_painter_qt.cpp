#include "typeset_painter.h"

#include <typeset_view.h>

#ifndef NDEBUG
#include <iostream>
#endif

#include <QPainterPath>

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

static QFont getFont(SemanticType type, uint8_t depth){
    QString name = QString::fromStdString(font_names[type].data());
    QString family = QString::fromStdString(font_family[type].data());
    int point_size = depthToFontSize(depth);

    QFont font = QFontDatabase().font(name, family, point_size);
    font.setKerning(false); //Probably can't have kerning and draw overlapping subtext

    return font;
}

static QColor getColor(SemanticType type){
    return QColor::fromRgb(r[type], g[type], b[type]);
}

static double getWidth(const QFont& font, const std::string& text){
    return QFontMetricsF(font).horizontalAdvance( QString::fromStdString(text) );
}

static double getWidth(const QFont& font, char ch){
    return QFontMetricsF(font).horizontalAdvance(ch);
}

double getWidth(SemanticType type, uint8_t depth, const std::string& text){
    return getWidth(getFont(type, depth), text);
}

double getWidth(SemanticType type, uint8_t depth, char ch){
    return getWidth(getFont(type, depth), ch);
}

double getHeight(SemanticType type, uint8_t depth){
    QFontMetrics fm(getFont(type, depth));
    return fm.capHeight()+fm.descent();
}

double getAscent(SemanticType type, uint8_t depth){
    return QFontMetrics(getFont(type, depth)).ascent();
}

double getDescent(SemanticType type, uint8_t depth){
    return QFontMetrics(getFont(type, depth)).descent();
}

double getXHeight(SemanticType type, uint8_t depth){
    return QFontMetrics(getFont(type, depth)).xHeight();
}

double getAboveCenter(SemanticType type, uint8_t depth){
    QFontMetrics fm(getFont(type, depth));
    return fm.capHeight() - static_cast<double>(fm.xHeight()+fm.capHeight())/3;
}

double getUnderCenter(SemanticType type, uint8_t depth){
    QFontMetrics fm(getFont(type, depth));
    return fm.descent() + static_cast<double>(fm.xHeight()+fm.capHeight())/3;
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

void Painter::setScriptLevel(uint8_t depth){
    this->depth = depth;
    painter.setFont(getFont(type, depth));
}

void Painter::setOffset(double x, double y){
    painter.translate(0, y);
    x_offset = x;
}

void Painter::drawText(double x, double y, const std::string& text, bool forward){
    x += x_offset;
    QString q_str = QString::fromStdString(text);

    if(forward){
        y += painter.fontMetrics().capHeight();
        painter.drawText(x, y, q_str);
    }else{
        y += painter.fontMetrics().capHeight();
        y -= painter.fontMetrics().ascent();
        double h = painter.fontMetrics().height();
        double w = QFontMetricsF(painter.font()).horizontalAdvance(q_str);
        painter.drawText(QRectF(x, y, w, h), Qt::AlignRight|Qt::AlignBaseline, q_str);
    }
}

void Painter::drawSymbol(double x, double y, const std::string& text){
    x += x_offset;
    QFont font = painter.font();
    font.setPointSizeF(1*font.pointSizeF());
    painter.setFont(font);
    y += painter.fontMetrics().capHeight();
    painter.drawText(x, y, QString::fromStdString(text));
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
    painter.setPen(View::selection_box_color);
    painter.setBrush(View::selection_box_color);

    drawRect(x, y, w, h);

    painter.setPen(View::selection_text_color);
    painter.setBrush(View::selection_text_color);
}

void Painter::drawError(double x, double y, double w, double h){
    painter.setPen(View::View::error_border_color);
    painter.setBrush(View::error_background_color);

    x += x_offset;
    painter.drawRoundedRect(x, y, w, h, 4, 4);

    painter.setPen(View::text_cursor_color);
    painter.setBrush(View::text_cursor_color);
}

void Painter::drawEmptySubphrase(double x, double y, double w, double h){
    x += x_offset;

    QPen pen = painter.pen();
    pen.setStyle(Qt::DotLine);
    pen.setWidthF(0.4);
    painter.setBrush(QColor(0,0,0,0));
    painter.setPen(pen);
    painter.drawRect(x, y, w, h);

    painter.setPen(View::text_cursor_color);
    painter.setBrush(View::text_cursor_color);
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
    grad.setColorAt(0.0, Qt::white);
    grad.setColorAt(0.6, QColor::fromRgb(230, 230, 230));
    grad.setColorAt(1.0, Qt::white);

    QPen pen(Qt::darkGray);
    pen.setWidthF(0.2);
    painter.setPen(pen);
    QBrush brush(grad);
    painter.setBrush(brush);

    painter.drawRoundedRect(x, y, w+1, h, 3, 3);
    //painter.drawRect(x, y, w, h);
    //painter.fillRect(x, y, w, h, grad);

    painter.setPen(View::text_cursor_color);
    painter.setBrush(View::text_cursor_color);
}

void Painter::drawNarrowCursor(double x, double y, double h){
    x += x_offset;
    painter.setPen(View::text_cursor_color);
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

double Painter::getWidth(const std::string& text){
    assert(painter.font().kerning() == false);
    return QFontMetricsF(painter.font()).horizontalAdvance( QString::fromStdString(text) );
}

void Painter::drawLineNumber(double y, size_t num, bool active){
    if(active) painter.setPen(View::line_num_active_color);
    else painter.setPen(View::line_num_passive_color);

    painter.setFont(QFontDatabase().font("CMU Bright", "Roman", 8));
    QFontMetricsF fm (painter.font());
    QString str = QString::number(num);
    qreal x = x_offset - fm.horizontalAdvance(str);
    y += painter.fontMetrics().descent() / 2;
    painter.drawText(x, y, str);
}

void Painter::setSelectionMode(){
    color_can_change = false;
}

void Painter::drawSymbol(char ch, double x, double y, double w, double h){
    x += x_offset;

    double scale_x = w / QFontMetricsF(painter.font()).horizontalAdvance(ch);
    double scale_y = h / painter.fontMetrics().ascent();
    x /= scale_x;
    y = (y + h - painter.fontMetrics().descent()) / scale_y;

    QTransform transform = painter.transform();
    painter.scale(scale_x, scale_y);
    painter.drawText(x, y, QChar(ch));
    painter.setTransform(transform);
}

void Painter::drawSymbol(const std::string& str, double x, double y, double w, double h){
    x += x_offset;

    QString qstr = QString::fromStdString(str);
    double scale_x = w / QFontMetricsF(painter.font()).horizontalAdvance(qstr);
    double scale_y = h / painter.fontMetrics().ascent();
    x /= scale_x;
    y = (y + h - painter.fontMetrics().descent()) / scale_y;

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
