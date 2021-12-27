#ifndef TYPESET_BIGSUM1_H
#define TYPESET_BIGSUM1_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Hope {

namespace Typeset {

class BigSum1 final : public Construct { 
public:
    BigSum1(){
        setupUnaryArg();
    }

    virtual char constructCode() const noexcept override { return BIGSUM1; }

    #ifndef HOPE_TYPESET_HEADLESS
    virtual bool increasesScriptDepth(uint8_t) const noexcept override { return true; }
    double symbol_width;

    virtual void updateSizeSpecific() noexcept override {
        symbol_width = getWidth(SEM_DEFAULT, parent->script_level, "∑");
        width = std::max(symbol_width, child()->width);
        above_center = getAboveCenter(SEM_DEFAULT, parent->script_level);
        under_center = getUnderCenter(SEM_DEFAULT, parent->script_level) + child()->height();
    }

    virtual void updateChildPositions() override {
        child()->x = x + (width - child()->width)/2;
        child()->y = y + height() - child()->height();
    }

    virtual void paintSpecific(Painter& painter) const override {
        double symbol_x = x + (width - symbol_width) / 2;
        painter.drawSymbol(symbol_x, y, "∑");
    }
    #endif
};

}

}

#endif // TYPESET_BIGSUM1_H
