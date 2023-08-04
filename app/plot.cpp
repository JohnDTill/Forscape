#include "plot.h"

#ifndef FORSCAPE_TYPESET_HEADLESS

#include <QPainter>
#include <QPainterPath>
#include <typeset_painter.h>
#include <typeset_themes.h>

#ifndef NDEBUG
#include <iostream>
#endif

class Plot::DiscreteSeries : public Plot::Series {
private:
    const std::vector<std::pair<double, double>> data;

public:
    DiscreteSeries(const std::vector<std::pair<double, double>>& discrete_data)
        : data(discrete_data) {}

private:
    const QPointF* asQPointFArray() const noexcept { return reinterpret_cast<const QPointF*>(data.data()); }
    const QPointF& qpoint(size_t index) const noexcept { return asQPointFArray()[index]; }

    void drawLinear(QPainter& painter) const {
        for(size_t i = 1; i < data.size(); i++) painter.drawLine(qpoint(i-1), qpoint(i));
    }

    //EVENTUALLY: cubic spline fitting

    void drawSquares(QPainter& painter) const {
        constexpr double HALF_WIDTH = 4;
        constexpr double WIDTH = 2*HALF_WIDTH;
        for(const auto& datum : data)
            painter.drawRect(datum.first-HALF_WIDTH, datum.second-HALF_WIDTH, WIDTH, WIDTH);
    }

    virtual void draw(QPainter& painter) const override final {
        drawLinear(painter);
    }
};

Plot::Plot(const std::string& title, const std::string& x_label, const std::string& y_label)
    : title(title), x_label(x_label), y_label(y_label){
    setMinimumHeight(MARGIN_BOT + MARGIN_TOP);
    setMinimumWidth(MARGIN_LEFT + MARGIN_RIGHT);
}

Plot::~Plot(){
    for(Series* s : series) delete s;
}

void Plot::addSeries(const std::vector<std::pair<double, double>>& discrete_data){
    series.push_back(new DiscreteSeries(discrete_data));
    for(const auto& entry : discrete_data){
        min_x = std::min(min_x, entry.first);
        max_x = std::max(max_x, entry.first);
        min_y = std::min(min_y, entry.second);
        max_y = std::max(max_y, entry.second);
    }

    scene_x = min_x;
    scene_y = min_y;
    scene_w = max_x - min_x;
    scene_h = max_y - min_y;
}

void Plot::setTitle(const std::string& str) noexcept {
    title = str;
}

void Plot::setXLabel(const std::string& str) noexcept {
    x_label = str;
}

void Plot::setYLabel(const std::string& str) noexcept {
    y_label = str;
}

void Plot::paintEvent(QPaintEvent* event){
    QPainter painter(this);

    painter.setBrush(Forscape::Typeset::getColour(Forscape::Typeset::COLOUR_BACKGROUND));
    painter.drawRect(plotPixelX, plotPixelY, plotPixelWidth(), plotPixelHeight());

    painter.translate(MARGIN_LEFT, MARGIN_TOP+plotPixelHeight());
    painter.scale(plotPixelWidth()/scene_w, -plotPixelHeight()/scene_h);
    painter.translate(-scene_x, -scene_y);
    QPen pen = painter.pen();
    QPen line_pen = pen;
    line_pen.setColor(QColor::fromRgb(0, 0, 255));
    line_pen.setWidthF(2);
    line_pen.setCosmetic(true);
    painter.setPen(line_pen);
    for(const Series* s : series) s->draw(painter);
    painter.resetTransform();
    painter.setPen(pen);

    //EVENTUALLY: codegen font metrics
    QFont title_font = Forscape::Typeset::getFont(Forscape::SEM_DEFAULT, 0);
    title_font.setPointSize(14);
    painter.setFont(title_font);
    QString qtitle = QString::fromStdString(title);
    QFontMetricsF metrics(painter.font());
    qreal title_width = metrics.horizontalAdvance(qtitle);
    qreal title_x = plotPixelX + (plotPixelWidth() - title_width)/2;
    qreal title_y = MARGIN_TOP - TEXT_MARGIN;
    painter.drawText(title_x, title_y, qtitle);

    QFont number_font = Forscape::Typeset::getFont(Forscape::SEM_NUMBER, 1);
    number_font.setPointSize(11);
    painter.setFont(number_font);
    metrics = QFontMetricsF(painter.font());

    qreal x_axis_baseline = height() - (MARGIN_BOT - TEXT_MARGIN) + metrics.height();

    QString sx = QString::number(scene_x);
    painter.drawText(MARGIN_LEFT - metrics.horizontalAdvance(sx)/2, x_axis_baseline, sx);
    QString sxF = QString::number(sceneRightX());
    painter.drawText(MARGIN_LEFT + plotPixelWidth() - metrics.horizontalAdvance(sxF)/2, x_axis_baseline, sxF);

    QFont label_font = Forscape::Typeset::getFont(Forscape::SEM_DEFAULT, 0);
    label_font.setPointSize(12);
    painter.setFont(label_font);
    metrics = QFontMetricsF(painter.font());

    QString qlabelx = QString::fromStdString(x_label);
    qreal xlabel_w = metrics.horizontalAdvance(qlabelx);
    qreal xlabel_x = plotPixelX + (plotPixelWidth() - xlabel_w)/2;
    painter.drawText(xlabel_x, x_axis_baseline, qlabelx);

    painter.rotate(-90);

    painter.setFont(number_font);
    metrics = QFontMetricsF(painter.font());

    static constexpr qreal y_axis_baseline = MARGIN_LEFT - TEXT_MARGIN;

    QString sy = QString::number(scene_y);
    painter.drawText(-(MARGIN_TOP + plotPixelHeight() + metrics.horizontalAdvance(sy)/2), y_axis_baseline, sy);
    QString syF = QString::number(sceneTopY());
    painter.drawText(-(MARGIN_TOP + metrics.horizontalAdvance(syF)/2), y_axis_baseline, syF);

    painter.setFont(label_font);
    metrics = QFontMetricsF(painter.font());

    QString qlabely = QString::fromStdString(y_label);
    qreal ylabel_w = metrics.horizontalAdvance(qlabely);
    qreal ylabel_y = plotPixelY + (plotPixelHeight() + ylabel_w)/2;

    painter.drawText(-ylabel_y, y_axis_baseline, qlabely);
    painter.resetTransform();
}

#endif
