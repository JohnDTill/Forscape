#ifndef TYPESET_FRACTION_H
#define TYPESET_FRACTION_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Hope {

namespace Typeset {

class Fraction final : public Construct {
public:
    Fraction(){
        setupBinaryArgs();
    }

    virtual char constructCode() const noexcept override { return FRACTION; }

    static constexpr double hgap = 1;
    static constexpr double bar_margin = 0;
    static constexpr double vgap = 1;

    #ifndef HOPE_TYPESET_HEADLESS
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
        painter.drawLine(x, y + above_center, width, 0);
    }

    virtual bool increasesScriptDepth(uint8_t) const noexcept override{
        return !parent->isLine();
    }
    #endif
};

}

}

#endif // TYPESET_FRACTION_H
