#include "hope_plot.h"

#ifndef HOPE_TYPESET_HEADLESS

#include <QPainter>

namespace Hope {

class Plot::DiscreteSeries : Plot::Series {
private:
    const std::vector<QPointF> points;

    static std::vector<QPointF> fromStdVectors(const std::vector<double>& x, const std::vector<double>& y){
        assert(x.size() == y.size() && !x.empty());
        std::vector<QPointF> points;
        points.reserve(x.size());
        for(size_t i = 0; i < x.size(); i++)
            points.push_back(QPointF(x[i], y[i]));
        return points;
    }

public:
    enum InterpolationScheme {
        None,
        Linear,
        Smooth,
    };

    //DO THIS - do markers really belong? How do they fit with marking specific discrete behaviour, such as discontinuity?
    enum Marker {
        NoMarker,
        Square,
    };

    DiscreteSeries(const std::vector<double>& x, const std::vector<double>& y)
        : points(fromStdVectors(x, y)) {
        assert(x.size() == y.size() && !x.empty());
    }

private:
    void drawLinear(QPainter& painter){
        for(size_t i = 1; i < points.size(); i++)
            painter.drawLine(points[i-1], points[i]);
    }

    void drawSmooth(QPainter& painter){
        painter.drawPolyline(points.data(), points.size());
    }

    void drawSquares(QPainter& painter){
        constexpr double HALF_WIDTH = 4;
        constexpr double WIDTH = 2*HALF_WIDTH;
        for(const QPointF& point : points)
            painter.drawRect(point.x()-HALF_WIDTH, point.y()-HALF_WIDTH, WIDTH, WIDTH);
    }

    virtual void paintEvent(QPaintEvent* event) override final {
        QPainter painter(this);


    }
};

void Plot::setTitle(const std::string& str) noexcept {
    title = str;
}

void Plot::setXLabel(const std::string& str) noexcept{
    x_label = str;
}

void Plot::setYLabel(const std::string& str) noexcept{
    y_label = str;
}

}

#endif
