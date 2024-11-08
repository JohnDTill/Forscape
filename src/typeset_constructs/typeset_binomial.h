#ifndef TYPESET_BINOMIAL_H
#define TYPESET_BINOMIAL_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

#include "forscape_serial_unicode.h"

namespace Forscape {

namespace Typeset {

class Binomial final : public Construct { 
public:
    Binomial(){
        setupBinaryArgs();
    }

    virtual char constructCode() const noexcept override { return BINOMIAL; }
    virtual void writePrefix(std::string& out) const noexcept override { out += BINOMIAL_STR; }
    virtual bool writeUnicode(std::string& out, int8_t script) const noexcept override {
        bool success = convertToUnicode(out, "(", script);
        assert(success);
        if(!args[0]->writeUnicode(out, script)) return false;
        if(script == 0) out += " ¦ ";
        else success = convertToUnicode(out, " | ", script);
        assert(success);
        if(!args[1]->writeUnicode(out, script)) return false;
        success = convertToUnicode(out, ")", script);
        assert(success);

        return true;
    }

    static constexpr double paren_width = 3;
    static constexpr double hgap = 1;
    static constexpr double vgap = 1;

    #ifndef FORSCAPE_TYPESET_HEADLESS
    virtual Text* textUp(const Subphrase* caller, double x) const noexcept override {
        return caller->id==1 ? first()->textLeftOf(x) : prev();
    }

    virtual Text* textDown(const Subphrase* caller, double x) const noexcept override {
        return caller->id==0 ? second()->textLeftOf(x) : next();
    }

    virtual void updateSizeFromChildSizes() noexcept override {
        width = std::max(first()->width, second()->width) + 2*hgap + 2*paren_width;
        above_center = first()->height() + vgap;
        under_center = second()->height() + vgap;
    }

    virtual void updateChildPositions() noexcept override {
        first()->x = x + (width - first()->width)/2;
        first()->y = y;
        second()->x = x + (width - second()->width)/2;
        second()->y = y + above_center+vgap;
    }

    virtual void paintSpecific(Painter& painter) const override {
        painter.drawSymbol('(', x, y, paren_width, height());
        painter.drawSymbol(')', x + width - paren_width, y, paren_width, height());
    }

    virtual void paintGrouping(Painter& painter) const override {
        painter.drawHighlightedGrouping(x, y, paren_width, height(), "(");
        painter.drawHighlightedGrouping(x + width - paren_width, y, paren_width, height(), ")");
    }

    virtual bool increasesScriptDepth(uint8_t) const noexcept override{
        return !parent->isLine();
    }
    #endif
};

}

}

#endif // TYPESET_BINOMIAL_H
