#ifndef TYPESET_BIGINTERSECTION1_H
#define TYPESET_BIGINTERSECTION1_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Hope {

namespace Typeset {

class BigIntersection1 final : public Construct { 
public:
    BigIntersection1(){
        setupUnaryArg();
    }

    virtual char constructCode() const noexcept override { return BIGINTERSECTION1; }

    #ifndef HOPE_TYPESET_HEADLESS
    virtual bool increasesScriptDepth(uint8_t) const noexcept override { return true; }
    double symbol_width;

    virtual void updateSizeSpecific() noexcept override {
        symbol_width = getWidth(SEM_DEFAULT, parent->script_level, "⋂");
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
        painter.drawSymbol(symbol_x, y, "⋂");
    }
    #endif
};

}

}

#endif // TYPESET_BIGINTERSECTION1_H
