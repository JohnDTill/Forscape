#ifndef TYPESET_DOUBLEINTEGRAL0_H
#define TYPESET_DOUBLEINTEGRAL0_H

#include "typeset_construct.h"
#include "typeset_subphrase.h"

namespace Hope {

namespace Typeset {

class DoubleIntegral0 final : public Construct { 
public:
    virtual char constructCode() const noexcept override { return DOUBLEINTEGRAL0; }

    #ifndef HOPE_TYPESET_HEADLESS
    virtual void updateSizeSpecific() noexcept override {
        width = getWidth(SEM_DEFAULT, parent->script_level, "∬");
        above_center = getAboveCenter(SEM_DEFAULT, parent->script_level);
        under_center = getUnderCenter(SEM_DEFAULT, parent->script_level);
    }

    virtual void paintSpecific(Painter& painter) const override {
        painter.drawSymbol(x, y, "∬");
    }
    #endif
};

}

}

#endif // TYPESET_DOUBLEINTEGRAL0_H
