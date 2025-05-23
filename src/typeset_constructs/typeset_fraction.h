#ifndef TYPESET_FRACTION_H
#define TYPESET_FRACTION_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

#include "forscape_serial_unicode.h"

namespace Forscape {

namespace Typeset {

class Fraction final : public Construct {
public:
    Fraction(){
        setupBinaryArgs();
    }

    virtual char constructCode() const noexcept override { return FRACTION; }
    virtual void writePrefix(std::string& out) const noexcept override { out += FRACTION_STR; }
    virtual bool writeUnicode(std::string& out, int8_t script) const noexcept override {
        //EVENTUALLY: better conversion with precedence to avoid stupid parenthesis
        //            e.g. 1/2 should not convert to (1)/(2)

        do_and_assert(convertToUnicode(out, "(", script));

        if(!first()->writeUnicode(out, script)) return false;

        do_and_assert(convertToUnicode(out, ")/(", script));

        if(!second()->writeUnicode(out, script)) return false;
        do_and_assert(convertToUnicode(out, ")", script));

        return true;
    }

    static constexpr double hgap = 5;
    static constexpr double bar_margin = 0;
    static constexpr double vgap = 2.5;

    #ifndef FORSCAPE_TYPESET_HEADLESS
    virtual Text* textUp(const Subphrase* caller, double x) const noexcept override {
        return caller->id==1 ? first()->textLeftOf(x) : prev();
    }

    virtual Text* textDown(const Subphrase* caller, double x) const noexcept override {
        return caller->id==0 ? second()->textLeftOf(x) : next();
    }

    virtual void updateSizeFromChildSizes() noexcept override {
        width = std::max(first()->width, second()->width) + 2*hgap + 2*bar_margin;
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
        painter.drawLine(x + hgap, y + above_center, width - 2*hgap, 0);
    }

    virtual bool increasesScriptDepth(uint8_t) const noexcept override{
        return !parent->isLine();
    }
    #endif
};

}

}

#endif // TYPESET_FRACTION_H
