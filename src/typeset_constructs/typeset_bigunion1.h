#ifndef TYPESET_BIGUNION1_H
#define TYPESET_BIGUNION1_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Hope {

namespace Typeset {

class BigUnion1 final : public Construct { 
public:
    BigUnion1(){
        setupUnaryArg();
    }

    virtual char constructCode() const noexcept override { return BIGUNION1; }

    #ifndef HOPE_TYPESET_HEADLESS
    virtual bool increasesScriptDepth(uint8_t) const noexcept override { return true; }
    double symbol_width;

    virtual void updateSizeSpecific() noexcept override {
        symbol_width = CHARACTER_WIDTHS[scriptDepth()];
        width = std::max(symbol_width, child()->width);
        above_center = ABOVE_CENTER[scriptDepth()];
        under_center = UNDER_CENTER[scriptDepth()] + child()->height();
    }

    virtual void updateChildPositions() override {
        child()->x = x + (width - child()->width)/2;
        child()->y = y + height() - child()->height();
    }

    virtual void paintSpecific(Painter& painter) const override {
        double symbol_x = x + (width - symbol_width) / 2;
        painter.drawSymbol(symbol_x, y, "⋃");
    }
    #endif
};

}

}

#endif // TYPESET_BIGUNION1_H
