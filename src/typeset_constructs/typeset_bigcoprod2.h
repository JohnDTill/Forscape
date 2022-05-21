#ifndef TYPESET_BIGCOPROD2_H
#define TYPESET_BIGCOPROD2_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Hope {

namespace Typeset {

class BigCoProd2 final : public Construct { 
public:
    BigCoProd2(){
        setupBinaryArgs();
    }

    virtual char constructCode() const noexcept override { return BIGCOPROD2; }

    #ifndef HOPE_TYPESET_HEADLESS
    virtual bool increasesScriptDepth(uint8_t) const noexcept override { return true; }
    double symbol_width;

    virtual Text* textUp(const Subphrase* caller, double x) const noexcept override {
        return caller->id==1 ? first()->textLeftOf(x) : prev();
    }

    virtual Text* textDown(const Subphrase* caller, double x) const noexcept override {
        return caller->id==0 ? second()->textLeftOf(x) : next();
    }

    virtual void updateSizeSpecific() noexcept override {
        symbol_width = CHARACTER_WIDTHS[scriptDepth()];
        width = std::max(symbol_width, std::max(first()->width, second()->width));
        above_center = ABOVE_CENTER[scriptDepth()] + first()->height();
        under_center = UNDER_CENTER[scriptDepth()] + second()->height();
    }

    virtual void updateChildPositions() override {
        first()->x = x + (width - first()->width)/2;
        first()->y = y;
        second()->x = x + (width - second()->width)/2;
        second()->y = y + height() - second()->height();
    }

    virtual void paintSpecific(Painter& painter) const override {
        double symbol_x = x + (width - symbol_width) / 2;
        painter.drawSymbol(symbol_x, y + second()->height(), "∐");
    }
    #endif
};

}

}

#endif // TYPESET_BIGCOPROD2_H
