#ifndef HOPE_TYPESET_HEADLESS

#ifndef HOPE_PLOT_H
#define HOPE_PLOT_H

//DO THIS - the plot runs in the GUI thread, but needs input from the interpreter thread

#include <QWidget>
class QPainter;

namespace Hope {

class Plot : public QWidget{
public:
    void addSeries(const std::vector<double>& x, const std::vector<double>& y);
    void setTitle(const std::string& str) noexcept;
    void setXLabel(const std::string& str) noexcept;
    void setYLabel(const std::string& str) noexcept;

private:
    std::string title;
    std::string x_label;
    std::string y_label;

    class Series : public QWidget {
        virtual void paintEvent(QPaintEvent* event) override = 0;
    };

    class DiscreteSeries;
};

}

#endif // HOPE_PLOT_H

#endif
