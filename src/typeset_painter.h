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

uint8_t depthToFontSize(uint8_t depth) noexcept;
double getWidth(SemanticType type, uint8_t depth, const std::string& text);
double getWidth(SemanticType type, uint8_t depth, char ch);
double getHeight(SemanticType type, uint8_t depth);
double getAscent(SemanticType type, uint8_t depth);
double getDescent(SemanticType type, uint8_t depth);
double getXHeight(SemanticType type, uint8_t depth);
double getAboveCenter(SemanticType type, uint8_t depth);
double getUnderCenter(SemanticType type, uint8_t depth);

class Painter {
public:
    static void init();
    Painter(WrappedPainter& painter);
    void setZoom(double zoom);
    void setType(SemanticType type);
    void setTypeIfAppropriate(SemanticType type);
    void setScriptLevel(uint8_t depth);
    void setOffset(double x, double y);
    void drawText(double x, double y, const std::string& text, bool forward = true);
    void drawText(double x, double y, char ch);
    void drawHighlightedGrouping(double x, double y, double w, const std::string& text);
    void drawSymbol(double x, double y, const std::string& text);
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
    double getWidth(const std::string& text);
    void drawLineNumber(double y, size_t num, bool active);
    void setSelectionMode();
    void drawSymbol(char ch, double x, double y, double w, double h);
    void drawSymbol(const std::string& str, double x, double y, double w, double h);

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
