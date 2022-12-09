#ifndef FORSCAPE_MESSAGE_H
#define FORSCAPE_MESSAGE_H

#include <string>
#include <vector>

namespace Forscape {

class InterpreterOutput {
public:
    enum MessageType {
        AddDiscreteSeries,
        CreatePlot,
        Print,
    };

    virtual MessageType getType() const noexcept = 0;
    virtual ~InterpreterOutput() noexcept {}
};

class PrintMessage : public InterpreterOutput {
public:
    virtual MessageType getType() const noexcept override { return Print; }
    const std::string msg;
    PrintMessage(const std::string& msg) noexcept : msg(msg) {}
};

class PlotCreate : public InterpreterOutput {
public:
    const std::string title;
    const std::string x_label;
    const std::string y_label;
    virtual MessageType getType() const noexcept override { return CreatePlot; }
    PlotCreate(const std::string& title, const std::string& x_label, const std::string& y_label) noexcept
        : title(title), x_label(x_label), y_label(y_label) {}
};

class PlotDiscreteSeries : public InterpreterOutput {
public:
    const std::vector<std::pair<double, double>> data;
    const std::string title;
    virtual MessageType getType() const noexcept override { return AddDiscreteSeries; }
    PlotDiscreteSeries(const std::vector<std::pair<double, double>>& data, const std::string& title = "") noexcept
        : data(data), title(title) {}
    static PlotDiscreteSeries* FromArrays(const double* const x, const double* const y, size_t sze, const std::string& title = ""){
        std::vector<std::pair<double, double>> data;
        data.reserve(sze);
        for(size_t i = 0; i < sze; i++) data.push_back(std::make_pair(x[i], y[i]));
        return new PlotDiscreteSeries(data, title);
    }
};

}

#endif // FORSCAPE_MESSAGE_H
