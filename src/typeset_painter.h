#ifndef TYPESET_PAINTER_H
#define TYPESET_PAINTER_H

#include <hope_error.h>
#include <semantic_tags.h>
#include <utility>
#include <vector>
#include <QFontDatabase>
#include <QPainter>
#include <QWidget>
typedef QPainter WrappedPainter;
typedef QWidget Widget;

namespace Hope {

namespace Typeset {

inline constexpr double BIG_SYM_SCALE = 2.5;

//EVENTUALLY: probably want to codegen these
inline constexpr double CHARACTER_WIDTHS[3] {
    7.796875,
    5.390625,
    4.1875,
};

inline constexpr double ABOVE_CENTER[3] = {
    3.66666666666666696272613990004,
    3,
    2,
};

inline constexpr double UNDER_CENTER[3] = {
    8.33333333333333214909544039983,
    6,
    5,
};

inline constexpr double CHARACTER_HEIGHTS[3] {
    12,
    9,
    7,
};

inline constexpr double DESCENT[3] {
    3.125,
    2.15625,
    1.6875,
};


uint8_t depthToFontSize(uint8_t depth) noexcept;
const QFont& getFont(SemanticType type, uint8_t depth) noexcept;

class Painter {
public:
    static void init();
    Painter(WrappedPainter& painter);
    void setZoom(double zoom);
    void setType(SemanticType type);
    void setTypeIfAppropriate(SemanticType type);
    void setScriptLevel(uint8_t depth);
    void setOffset(double x, double y);
    void drawText(double x, double y, std::string_view text, bool forward = true);
    void drawText(double x, double y, char ch);
    void drawHighlightedGrouping(double x, double y, double w, std::string_view text);
    void drawSymbol(double x, double y, std::string_view text);
    void drawLine(double x, double y, double w, double h);
    void drawPath(const std::vector<std::pair<double,double> >& points);
    void drawRect(double x, double y, double w, double h);
    void drawSelection(double x, double y, double w, double h);
    void drawError(double x, double y, double w, double h);
    void drawEmptySubphrase(double x, double y, double w, double h);
    void drawHighlight(double x, double y, double w, double h);
    void drawNarrowCursor(double x, double y, double h);
    void drawElipse(double x, double y, double w, double h);
    void drawLeftParen(double x, double y, double w, double h);
    void drawRightParen(double x, double y, double w, double h);
    void drawDot(double x, double y);
    void drawLineNumber(double y, size_t num, bool active);
    void setSelectionMode();
    void exitSelectionMode();
    void drawSymbol(char ch, double x, double y, double w, double h);
    void drawSymbol(std::string_view str, double x, double y, double w, double h);
    void drawComma(double x, double y, bool selected);

    #ifdef HOPE_TYPESET_LAYOUT_DEBUG
    void drawDebugPhrase(double x, double y, double w, double u, double v);
    void drawHorizontalConstructionLine(double y);
    #endif

private:
    WrappedPainter& painter;
    uint8_t depth = 0;
    SemanticType type = SEM_DEFAULT;
    double x_offset = 0;
    bool color_can_change = true;
};

}

}

#endif // TYPESET_PAINTER_H
