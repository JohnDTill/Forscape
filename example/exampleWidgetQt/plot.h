#ifndef HOPE_TYPESET_HEADLESS

#ifndef HOPE_PLOT_H
#define HOPE_PLOT_H

#include <QWidget>
class QPainter;

class Plot : public QWidget {
public:
    Plot(const std::string& title, const std::string& x_label, const std::string& y_label);
    ~Plot();
    void addSeries(const std::vector<std::pair<double, double>>& discrete_data);
    void setTitle(const std::string& str) noexcept;
    void setXLabel(const std::string& str) noexcept;
    void setYLabel(const std::string& str) noexcept;
    //EVENTUALLY: add user interaction
    //EVENTUALLY: add syntax to support multiple series
    //EVENTUALLY: add syntax to support series names, colours, line styles, and markers
    //EVENTUALLY: add legend
    //EVENTUALLY: add toolbar with option to save as SVG
    //EVENTUALLY: add 3D plotting
    //EVENTUALLY: support plotting functions

private:
    std::string title;
    std::string x_label;
    std::string y_label;
    double scene_x = 0;
    double scene_y = 0;
    double scene_w = 1;
    double scene_h = 1;
    double sceneRightX() const noexcept { return scene_x + scene_w; }
    double sceneTopY() const noexcept { return scene_y + scene_h; }
    double min_x = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::min();
    double min_y = std::numeric_limits<double>::max();
    double max_y = std::numeric_limits<double>::min();

    static constexpr size_t MARGIN_TOP = 50;
    static constexpr size_t TEXT_MARGIN = 10;
    static constexpr size_t MARGIN_LEFT = 50;
    static constexpr size_t MARGIN_BOT = 50;
    static constexpr size_t MARGIN_RIGHT = 20;
    static constexpr int plotPixelX = MARGIN_LEFT;
    static constexpr int plotPixelY = MARGIN_TOP;
    int plotPixelWidth() noexcept { return width() - (MARGIN_RIGHT + plotPixelX); }
    int plotPixelHeight() noexcept { return height() - (MARGIN_BOT + plotPixelY); }

    class Series {
    public:
        virtual ~Series() noexcept {}
        virtual void draw(QPainter& painter) const = 0;
    };
    class DiscreteSeries;

    std::vector<Series*> series;

    virtual void paintEvent(QPaintEvent* event) override;
};

#endif // HOPE_PLOT_H

#endif
